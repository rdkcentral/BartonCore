#--------------------------------------------------------------------------------------
#
# CMake definition for the fcore 'time library'
# Should not be utilized directly, but imported from our parent
# file via the 'add_subdirectory()' routine.
#
#--------------------------------------------------------------------------------------


# setup include paths
include_directories(
        ${CMAKE_CURRENT_SOURCE_DIR}/public
        ${ZILKER_INCLUDE_DIR}
        ${CROSS_OUTPUT}/include     # 3rd Party
)

# define source files (using wildcard)
file(GLOB SOURCES "src/*.c")

add_library(xhTime STATIC ${SOURCES})
target_link_libraries(xhTime xhLog xhUtil m ${CONFIG_LDFLAG})

# export our 'public' folder so others can include our headers
target_include_directories(xhTime PUBLIC public)

# add our 'unit test' to be part of the "make unitTest"
bds_add_cmocka_test(
        NAME testTimeUtils
        TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/test/src/timeUtilsTest.c
        LINK_LIBRARIES xhTime
)

bds_add_cmocka_test(
        NAME testTimeTracker
        TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/test/src/timeTrackerTest.c
        LINK_LIBRARIES xhTime
)

