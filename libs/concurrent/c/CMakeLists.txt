# ------------------------------ tabstop = 4 ----------------------------------
#
# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2024 Comcast Cable Communications Management, LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# SPDX-License-Identifier: Apache-2.0
#
# ------------------------------ tabstop = 4 ----------------------------------

#--------------------------------------------------------------------------------------
#
# CMake definition for the fcore 'concurrent library'
# Should not be utilized directly, but imported from our parent
# file via the 'add_subdirectory()' routine.
#
#--------------------------------------------------------------------------------------
include(BDSConfigureGLib)
include(BDSAddCMockaTest)

# setup include paths
include_directories(
        ${CMAKE_CURRENT_SOURCE_DIR}/public
        ${ZILKER_INCLUDE_DIR}
)

# define source files (using wildcard)
file(GLOB SOURCES "src/*.[ch]" "public/icConcurrent/*.h")

add_library(xhConcurrent STATIC ${SOURCES})
target_link_libraries(xhConcurrent xhLog xhTypes xhTime ${ZILKER_EXE_LIBS})

# export our 'public' folder so others can include our headers
target_include_directories(xhConcurrent PUBLIC public)

bds_configure_glib()

# add our 'unit tests' (part of the "make unitTest")
bds_add_cmocka_test(
        NAME testThreadPool
        TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/test/src/threadPoolTest.c
        LINK_LIBRARIES xhConcurrent
)

bds_add_cmocka_test(
        NAME testTaskExecutor
        TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/test/src/taskExecutorTest.c
        LINK_LIBRARIES xhConcurrent
)

bds_add_cmocka_test(
        NAME testDelayedTask
        TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/test/src/delayedTaskTest.c
        LINK_LIBRARIES xhConcurrent
)

bds_add_cmocka_test(
        NAME testRepeatingTask
        TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/test/src/repeatingTaskTest.c
        LINK_LIBRARIES xhConcurrent
)

bds_add_cmocka_test(
        NAME testBlockingQueue
        TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/test/src/blockingQueueTest.c
        LINK_LIBRARIES xhConcurrent xhUtil
)

bds_add_cmocka_test(
        NAME testThreadSafeWrapper
        TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/test/src/threadSafeWrapperTest.c
        LINK_LIBRARIES xhConcurrent xhUtil
)

bds_add_cmocka_test(
        NAME testThreadUtils
        TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/test/src/threadUtilsTest.c
        LINK_LIBRARIES xhConcurrent xhUtil
)

bds_add_cmocka_test(
        NAME testDebounce
        TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/test/src/debounceTest.c
        LINK_LIBRARIES xhConcurrent xhUtil
)
