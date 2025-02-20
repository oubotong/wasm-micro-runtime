/*
 * Copyright (C) 2019 Intel Corporation.  All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * Note:
 * This file implements the linux version connection library which is
 * defined in connection_lib.h.
 * It also provides a reference implementation of connections manager.
 */

#include "connection_lib.h"
#include "bh_thread.h"
#include "app_manager_export.h"
#include "module_wasm_app.h"
#include "conn_tcp.h"
#include "conn_udp.h"
#include "conn_uart.h"
#include "bh_definition.h"

#include <unistd.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <fcntl.h>

#define MAX_EVENTS 10
#define IO_BUF_SIZE 256

/* Connection type */
typedef enum conn_type {
    CONN_TYPE_TCP,
    CONN_TYPE_UDP,
    CONN_TYPE_UART,
    CONN_TYPE_UNKNOWN
} conn_type_t;

/* Sys connection */
typedef struct sys_connection {
    /* Next connection */
    struct sys_connection *next;

    /* Type */
    conn_type_t type;

    /* Handle to interact with wasm app */
    uint32 handle;

    /* Underlying connection ID, may be socket fd */
    int fd;

    /* Module id that the connection belongs to */
    uint32 module_id;

    /* Argument, such as dest addr for udp */
    void *arg;
} sys_connection_t;

/* Epoll instance */
static int epollfd;

/* Connections list */
static sys_connection_t *g_connections = NULL;

/* Max handle */
static uint32 g_handle_max = 0;

/* Lock to protect g_connections and g_handle_max */
static korp_mutex g_lock;

/* Epoll events */
static struct epoll_event epoll_events[MAX_EVENTS];

/* Buffer to receive data */
static char io_buf[IO_BUF_SIZE];

static uint32 _conn_open(wasm_module_inst_t module_inst,
                         const char *name, attr_container_t *args);
static void _conn_close(uint32 handle);
static int _conn_send(uint32 handle, const char *data, int len);
static bool _conn_config(uint32 handle, attr_container_t *cfg);

/*
 * Platform implementation of connection library
 */
connection_interface_t connection_impl = {
        ._open = _conn_open,
        ._close = _conn_close,
        ._send = _conn_send,
        ._config = _conn_config
};

static void add_connection(sys_connection_t *conn)
{
    vm_mutex_lock(&g_lock);

    g_handle_max++;
    if (g_handle_max == -1)
        g_handle_max++;
    conn->handle = g_handle_max;

    if (g_connections) {
        conn->next = g_connections;
        g_connections = conn;
    } else {
        g_connections = conn;
    }

    vm_mutex_unlock(&g_lock);
}

#define FREE_CONNECTION(conn) do {      \
    if (conn->arg)                      \
        bh_free(conn->arg);             \
    bh_free(conn);                      \
} while (0)

static int get_app_conns_num(uint32 module_id)
{
    sys_connection_t *conn;
    int num = 0;

    vm_mutex_lock(&g_lock);

    conn = g_connections;
    while (conn) {
        if (conn->module_id == module_id)
            num++;
        conn = conn->next;
    }

    vm_mutex_unlock(&g_lock);

    return num;
}

static sys_connection_t *find_connection(uint32 handle, bool remove_found)
{
    sys_connection_t *conn, *prev = NULL;

    vm_mutex_lock(&g_lock);

    conn = g_connections;
    while (conn) {
        if (conn->handle == handle) {
            if (remove_found) {
                if (prev != NULL) {
                    prev->next = conn->next;
                } else {
                    g_connections = conn->next;
                }
            }
            vm_mutex_unlock(&g_lock);
            return conn;
        } else {
            prev = conn;
            conn = conn->next;
        }
    }

    vm_mutex_unlock(&g_lock);

    return NULL;
}

static void cleanup_connections(uint32 module_id)
{
    sys_connection_t *conn, *prev = NULL;

    vm_mutex_lock(&g_lock);

    conn = g_connections;
    while (conn) {
        if (conn->module_id == module_id) {
            epoll_ctl(epollfd, EPOLL_CTL_DEL, conn->fd, NULL);
            close(conn->fd);

            if (prev != NULL) {
                prev->next = conn->next;
                FREE_CONNECTION(conn);
                conn = prev->next;
            } else {
                g_connections = conn->next;
                FREE_CONNECTION(conn);
                conn = g_connections;
            }
        } else {
            prev = conn;
            conn = conn->next;
        }
    }

    vm_mutex_unlock(&g_lock);
}

static conn_type_t get_conn_type(const char *name)
{
    if (strcmp(name, "TCP") == 0)
        return CONN_TYPE_TCP;
    if (strcmp(name, "UDP") == 0)
        return CONN_TYPE_UDP;
    if (strcmp(name, "UART") == 0)
        return CONN_TYPE_UART;

    return CONN_TYPE_UNKNOWN;
}

/* --- connection lib function --- */
static uint32 _conn_open(wasm_module_inst_t module_inst,
                         const char *name, attr_container_t *args)
{
    int fd;
    sys_connection_t *conn;
    struct epoll_event ev;
    uint32 module_id = app_manager_get_module_id(Module_WASM_App,
                                                 module_inst);

    if (get_app_conns_num(module_id) >= MAX_CONNECTION_PER_APP)
        return -1;

    conn = (sys_connection_t *)bh_malloc(sizeof(*conn));
    if (conn == NULL)
        return -1;

    memset(conn, 0, sizeof(*conn));
    conn->module_id = module_id;
    conn->type = get_conn_type(name);

    /* Generate a handle and add to list */
    add_connection(conn);

    if (conn->type == CONN_TYPE_TCP) {
        char *address;
        uint16 port;

        /* Check and parse connection parameters */
        if (!attr_container_contain_key(args, "address") ||
            !attr_container_contain_key(args, "port"))
            goto fail;

        address = attr_container_get_as_string(args, "address");
        port = attr_container_get_as_uint16(args, "port");

        /* Connect to TCP server */
        if ((fd = tcp_open(address, port)) == -1)
            goto fail;

    } else if (conn->type == CONN_TYPE_UDP) {
        uint16 port;

        /* Check and parse connection parameters */
        if (!attr_container_contain_key(args, "bind port"))
            goto fail;
        port = attr_container_get_as_uint16(args, "bind port");

        /* Bind port */
        if ((fd = udp_open(port)) == -1)
            goto fail;

    } else if (conn->type == CONN_TYPE_UART) {
        char *device;
        int baud;

        /* Check and parse connection parameters */
        if (!attr_container_contain_key(args, "device") ||
            !attr_container_contain_key(args, "baudrate"))
            goto fail;
        device = attr_container_get_as_string(args, "device");
        baud = attr_container_get_as_int(args, "baudrate");

        /* Open device */
        if ((fd = uart_open(device, baud)) == -1)
            goto fail;

    }

    conn->fd = fd;

    /* Set current connection as event data */
    ev.events = EPOLLIN;
    ev.data.ptr = conn;

    /* Monitor incoming data */
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &ev) == -1) {
        close(fd);
        goto fail;
    }

    return conn->handle;

fail:
    find_connection(conn->handle, true);
    bh_free(conn);
    return -1;
}

/* --- connection lib function --- */
static void _conn_close(uint32 handle)
{
    sys_connection_t *conn = find_connection(handle, true);

    if (conn != NULL) {
        epoll_ctl(epollfd, EPOLL_CTL_DEL, conn->fd, NULL);
        close(conn->fd);
        FREE_CONNECTION(conn);
    }
}

/* --- connection lib function --- */
static int _conn_send(uint32 handle, const char *data, int len)
{
    sys_connection_t *conn = find_connection(handle, false);

    if (conn == NULL)
        return -1;

    if (conn->type == CONN_TYPE_TCP)
        return tcp_send(conn->fd, data, len);

    if (conn->type == CONN_TYPE_UDP) {
        struct sockaddr *addr = (struct sockaddr *)conn->arg;
        return udp_send(conn->fd, addr, data, len);
    }

    if (conn->type == CONN_TYPE_UART)
        return uart_send(conn->fd, data, len);

    return -1;
}

/* --- connection lib function --- */
static bool _conn_config(uint32 handle, attr_container_t *cfg)
{
    sys_connection_t *conn = find_connection(handle, false);

    if (conn == NULL)
        return false;

    if (conn->type == CONN_TYPE_UDP) {
        char *address;
        uint16_t port;
        struct sockaddr_in *addr;

        /* Parse remote address/port */
        if (!attr_container_contain_key(cfg, "address") ||
            !attr_container_contain_key(cfg, "port"))
            return false;
        address = attr_container_get_as_string(cfg, "address");
        port = attr_container_get_as_uint16(cfg, "port");

        if (conn->arg == NULL) {
            addr = (struct sockaddr_in *)bh_malloc(sizeof(*addr));
            if (addr == NULL)
                return false;

            memset(addr, 0, sizeof(*addr));
            addr->sin_family = AF_INET;
            addr->sin_addr.s_addr = inet_addr(address);
            addr->sin_port = htons(port);

            /* Set remote address as connection arg */
            conn->arg = addr;
        } else {
            addr = (struct sockaddr_in *)conn->arg;
            addr->sin_addr.s_addr = inet_addr(address);
            addr->sin_port = htons(port);
        }

        return true;
    }

    return false;
}

/* --- connection manager reference implementation ---*/

typedef struct connection_event {
    uint32 handle;
    char *data;
    uint32 len;
} connection_event_t;

static void connection_event_cleaner(connection_event_t *conn_event)
{
    if (conn_event->data != NULL)
        bh_free(conn_event->data);
    bh_free(conn_event);
}

static void post_msg_to_module(sys_connection_t *conn,
                               char *data,
                               uint32 len)
{
    module_data *module = module_data_list_lookup_id(conn->module_id);
    char *data_copy = NULL;
    connection_event_t *conn_data_event;
    bh_message_t msg;

    if (module == NULL)
        return;

    conn_data_event = (connection_event_t *)bh_malloc(sizeof(*conn_data_event));
    if (conn_data_event == NULL)
        return;

    if (len > 0) {
        data_copy = (char *)bh_malloc(len);
        if (data_copy == NULL) {
            bh_free(conn_data_event);
            return;
        }
        memcpy(data_copy, data, len);
    }

    memset(conn_data_event, 0, sizeof(*conn_data_event));
    conn_data_event->handle = conn->handle;
    conn_data_event->data = data_copy;
    conn_data_event->len = len;

    msg = bh_new_msg(CONNECTION_EVENT_WASM,
                     conn_data_event,
                     sizeof(*conn_data_event),
                     connection_event_cleaner);
    if (!msg) {
        connection_event_cleaner(conn_data_event);
        return;
    }

    bh_post_msg2(module->queue, msg);
}

static void* polling_thread_routine (void *arg)
{
    while (true) {
        int i, n;

        n = epoll_wait(epollfd, epoll_events, MAX_EVENTS, -1);

        if (n == -1 && errno != EINTR)
            continue;

        for (i = 0; i < n; i++) {
            sys_connection_t *conn
                             = (sys_connection_t *)epoll_events[i].data.ptr;

            if (conn->type == CONN_TYPE_TCP) {
                int count = tcp_recv(conn->fd, io_buf, IO_BUF_SIZE);
                if (count <= 0) {
                    /* Connection is closed by peer */
                    post_msg_to_module(conn, NULL, 0);
                    _conn_close(conn->handle);
                } else {
                    /* Data is received */
                    post_msg_to_module(conn, io_buf, count);
                }
            } else if (conn->type == CONN_TYPE_UDP) {
                int count = udp_recv(conn->fd, io_buf, IO_BUF_SIZE);
                if (count > 0)
                    post_msg_to_module(conn, io_buf, count);
            } else if (conn->type == CONN_TYPE_UART) {
                int count = uart_recv(conn->fd, io_buf, IO_BUF_SIZE);
                if (count > 0)
                    post_msg_to_module(conn, io_buf, count);
            }
        }
    }

    return NULL;
}

void app_mgr_connection_event_callback(module_data *m_data, bh_message_t msg)
{
    uint32 argv[3];
    wasm_function_inst_t func_on_conn_data;
    bh_assert(CONNECTION_EVENT_WASM == bh_message_type(msg));
    wasm_data *wasm_app_data = (wasm_data*) m_data->internal_data;
    wasm_module_inst_t inst = wasm_app_data->wasm_module_inst;
    connection_event_t *conn_event
                       = (connection_event_t *)bh_message_payload(msg);
    int32 data_offset;

    if (conn_event == NULL)
        return;

    func_on_conn_data = wasm_runtime_lookup_function(inst, "_on_connection_data",
                                                      "(i32i32i32)");
    if (!func_on_conn_data) {
        printf("Cannot find function _on_connection_data\n");
        return;
    }

    /* 0 len means connection closed */
    if (conn_event->len == 0) {
        argv[0] = conn_event->handle;
        argv[1] = 0;
        argv[2] = 0;
        if (!wasm_runtime_call_wasm(inst, NULL, func_on_conn_data, 3, argv)) {
            printf(":Got exception running wasm code: %s\n",
                    wasm_runtime_get_exception(inst));
            wasm_runtime_clear_exception(inst);
            return;
        }
    } else {
        data_offset = wasm_runtime_module_dup_data(inst,
                                                   conn_event->data,
                                                   conn_event->len);
        if (data_offset == 0) {
            printf("Got exception running wasm code: %s\n",
                    wasm_runtime_get_exception(inst));
            wasm_runtime_clear_exception(inst);
            return;
        }

        argv[0] = conn_event->handle;
        argv[1] = (uint32) data_offset;
        argv[2] = conn_event->len;
        if (!wasm_runtime_call_wasm(inst, NULL, func_on_conn_data, 3, argv)) {
            printf(":Got exception running wasm code: %s\n",
                    wasm_runtime_get_exception(inst));
            wasm_runtime_clear_exception(inst);
            wasm_runtime_module_free(inst, data_offset);
            return;
        }
        wasm_runtime_module_free(inst, data_offset);
    }
}

bool init_connection_framework()
{
    korp_thread tid;

    epollfd = epoll_create(MAX_EVENTS);
    if (epollfd == -1)
        return false;

    if (vm_mutex_init(&g_lock) != BH_SUCCESS) {
        close(epollfd);
        return false;
    }

    if (!wasm_register_cleanup_callback(cleanup_connections)) {
        goto fail;
    }

    if (!wasm_register_msg_callback(CONNECTION_EVENT_WASM,
                               app_mgr_connection_event_callback)) {
        goto fail;
    }

    if (vm_thread_create(&tid,
                         polling_thread_routine,
                         NULL,
                         BH_APPLET_PRESERVED_STACK_SIZE) != BH_SUCCESS) {
        goto fail;
    }

    return true;

fail:
    vm_mutex_destroy(&g_lock);
    close(epollfd);
    return false;
}
