#--------------------------------------------------------------------------------------
#
# CMake definition for the fcore 'zhal library'
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

add_library(zhal STATIC ${SOURCES})
target_link_libraries(zhal xhLog xhTypes xhUtil xhConcurrent cjson m ${CONFIG_LDFLAG})

# export our 'public' folder so others can include our headers
target_include_directories(zhal PUBLIC public)

bds_add_cmocka_test(
        NAME testZhalImpl
        TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/test/src/zhalImplUnitTest.c
        WRAPPED_FUNCTIONS connect send recv zhalNetworkTerm
        LINK_LIBRARIES zhal
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/test
        INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# add our test to be part of the "make manualTest"
add_subdirectory(test)

bds_configure_glib()
