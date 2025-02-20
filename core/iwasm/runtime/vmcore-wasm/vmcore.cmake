# Copyright (C) 2019 Intel Corporation.  All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

set (VMCORE_LIB_DIR ${CMAKE_CURRENT_LIST_DIR})

include_directories(${VMCORE_LIB_DIR})
include_directories(${VMCORE_LIB_DIR}/../include)

file (GLOB_RECURSE c_source_all ${VMCORE_LIB_DIR}/*.c)
list (REMOVE_ITEM c_source_all ${VMCORE_LIB_DIR}/invokeNative_general.c)

if (${BUILD_AS_64BIT_SUPPORT} STREQUAL "YES")
set (source_all ${c_source_all} ${VMCORE_LIB_DIR}/invokeNative_em64.s)
else ()
set (source_all ${c_source_all} ${VMCORE_LIB_DIR}/invokeNative_ia32.s)
endif ()

set (VMCORE_LIB_SOURCE ${source_all})

