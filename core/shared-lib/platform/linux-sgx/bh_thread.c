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

#include "bh_thread.h"
#include "bh_assert.h"
#include "bh_memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

int _vm_thread_sys_init()
{
    return 0;
}

void vm_thread_sys_destroy(void)
{
}

int _vm_thread_create_with_prio(korp_tid *tid, thread_start_routine_t start,
        void *arg, unsigned int stack_size, int prio)
{
    return BHT_ERROR;
    // return BHT_OK;
}

int _vm_thread_create(korp_tid *tid, thread_start_routine_t start, void *arg,
        unsigned int stack_size)
{
    return _vm_thread_create_with_prio(tid, start, arg, stack_size,
                                       BH_THREAD_DEFAULT_PRIORITY);
}

korp_tid _vm_self_thread()
{
    return 0;
}

void vm_thread_exit(void * code)
{
}

// storage for one thread
static void *_tls_store = NULL;

void *_vm_tls_get(unsigned idx)
{
    return _tls_store;
}

int _vm_tls_put(unsigned idx, void * tls)
{
    _tls_store = tls;
    return BHT_OK;
    //return BHT_ERROR;
}

int _vm_mutex_init(korp_mutex *mutex)
{
    return BHT_OK;
    //return BHT_ERROR;
}

int _vm_recursive_mutex_init(korp_mutex *mutex)
{
    return BHT_OK;
    //return BHT_ERROR;
}

int _vm_mutex_destroy(korp_mutex *mutex)
{
    return BHT_OK;
    //return BHT_ERROR;
}

/* Returned error (EINVAL, EAGAIN and EDEADLK) from
 locking the mutex indicates some logic error present in
 the program somewhere.
 Don't try to recover error for an existing unknown error.*/
void vm_mutex_lock(korp_mutex *mutex)
{
}

int vm_mutex_trylock(korp_mutex *mutex)
{
    return BHT_OK;
    //return BHT_ERROR;
}

/* Returned error (EINVAL, EAGAIN and EPERM) from
 unlocking the mutex indicates some logic error present
 in the program somewhere.
 Don't try to recover error for an existing unknown error.*/
void vm_mutex_unlock(korp_mutex *mutex)
{
}

int _vm_sem_init(korp_sem* sem, unsigned int c)
{
    return BHT_OK;
    //return BHT_ERROR;
}

int _vm_sem_destroy(korp_sem *sem)
{
    return BHT_OK;
    //return BHT_ERROR;
}

int _vm_sem_wait(korp_sem *sem)
{
    return BHT_OK;
    //return BHT_ERROR;
}

int _vm_sem_reltimedwait(korp_sem *sem, int mills)
{
    return BHT_OK;
    //return BHT_ERROR;
}

int _vm_sem_post(korp_sem *sem)
{
    return BHT_OK;
    //return BHT_ERROR;
}

int _vm_cond_init(korp_cond *cond)
{
    return BHT_OK;
    //return BHT_ERROR;
}

int _vm_cond_destroy(korp_cond *cond)
{
    return BHT_OK;
    //return BHT_ERROR;
}

int _vm_cond_wait(korp_cond *cond, korp_mutex *mutex)
{
    return BHT_OK;
    //return BHT_ERROR;
}

int _vm_cond_reltimedwait(korp_cond *cond, korp_mutex *mutex, int mills)
{
    return BHT_OK;
    //return BHT_ERROR;
}

int _vm_cond_signal(korp_cond *cond)
{
    return BHT_OK;
    //return BHT_ERROR;
}

int _vm_cond_broadcast(korp_cond *cond)
{
    return BHT_OK;
    //return BHT_ERROR;
}

int _vm_thread_cancel(korp_tid thread)
{
    return 0;
}

int _vm_thread_join(korp_tid thread, void **value_ptr, int mills)
{
    return 0;
}

int _vm_thread_detach(korp_tid thread)
{
    return 0;
}
