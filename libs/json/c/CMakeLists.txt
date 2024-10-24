#--------------------------------------------------------------------------------------
#
# CMake definition for the fcore 'xml library'
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

add_library(xhJsonHelper STATIC ${SOURCES})
target_link_libraries(xhJsonHelper cjson xhTypes xhUtil)

# export our 'public' folder so others can include our headers
target_include_directories(xhJsonHelper PUBLIC public)

# test target
bds_add_cmocka_test(
        NAME testJsonPointer
        TYPE unit
        TEST_SOURCES "test/src/jsonPointerTest.c"
        LINK_LIBRARIES xhJsonHelper
)
