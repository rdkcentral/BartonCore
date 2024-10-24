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
