#--------------------------------------------------------------------------------------
#
# CMake definition for the fcore 'utils library unit tests'
# Should not be utilized directly, but imported from our parent
# file via the 'add_subdirectory()' routine.
#
#--------------------------------------------------------------------------------------

# setup a unit test, but such that it is NOT part of the 'make all' sequence.
# allows us to only build/run unit tests on the platforms we can (meaning
# not during a cross-compile session).  should be build/ran as part of
# the 'make unitTest' command

# setup include paths
include_directories(
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${ZILKER_INCLUDE_DIR}
        ${CROSS_OUTPUT}/include         # 3rd Party
)

# define source files (using wildcard)
file(GLOB SOURCES "src/*.c")
#Remove tests that use add_zilker_test from the parent
list(FILTER SOURCES EXCLUDE REGEX ".*fileTest.c$")
list(FILTER SOURCES EXCLUDE REGEX ".*stringTest.c$")
list(FILTER SOURCES EXCLUDE REGEX ".*pingTest.c$")
list(FILTER SOURCES EXCLUDE REGEX ".*regexTest.c$")
list(FILTER SOURCES EXCLUDE REGEX ".*systemCommandUtilsTest.c$")
list(FILTER SOURCES EXCLUDE REGEX ".*stateMachineTest.c$")

# build as an executable, but NOT part of the "make all" sequence
add_executable(testUtilLib EXCLUDE_FROM_ALL ${SOURCES})

# add library dependencies for this test-binary
target_link_libraries(testUtilLib xhUtil xhTypes ${ZILKER_EXE_LIBS})

# run as a 'test', however need to force the LD_LIBRARY_PATH since
# the RPATH will not be set (as this was not 'installed' via make)
add_test(NAME testUtilLib COMMAND testUtilLib -a WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
if (CONFIG_OS_DARWIN)
    set_property(TEST testUtilLib PROPERTY ENVIRONMENT "DYLD_LIBRARY_PATH=${CROSS_OUTPUT}/lib:${CMAKE_INSTALL_PREFIX}/lib")
else()
    set_property(TEST testUtilLib PROPERTY ENVIRONMENT "LD_LIBRARY_PATH=${CROSS_OUTPUT}/lib:${CMAKE_INSTALL_PREFIX}/lib")
endif()

#bds_add_cmocka_test(NAME testPing TEST_SOURCES src/pingTest.c LINK_LIBRARIES xhUtil)

bds_add_cmocka_test(
    NAME testStateMachine
    TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/stateMachineTest.c
    LINK_LIBRARIES xhUtil
)