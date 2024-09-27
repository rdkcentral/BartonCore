#--------------------------------------------------------------------------------------
#
# CMake definition for the fcore 'device-descriptor library unit tests'
# Should not be utilized directly, but imported from our parent
# file via the 'add_subdirectory()' routine.
#
#--------------------------------------------------------------------------------------

# setup a unit test, but such that it is NOT part of the 'make all' sequence.
# allows us to only build tests on the platforms we can (meaning
# not during a cross-compile session).  should be built as part of
# the 'make manualTest' command

# setup include paths
include_directories(
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${ZILKER_INCLUDE_DIR}
        ${CROSS_OUTPUT}/include         # 3rd Party
)

# define source files (using wildcard)
file(GLOB SOURCES "src/*.c")

# build as an executable, but NOT part of the "make all" sequence
add_executable(testDeviceDescriptorsLib EXCLUDE_FROM_ALL ${SOURCES})

# add library dependencies for this test-binary
target_link_libraries(testDeviceDescriptorsLib xhDeviceDescriptors cmocka ${ZILKER_EXE_LIBS})

# run as a 'test', however need to force the LD_LIBRARY_PATH since
# the RPATH will not be set (as this was not 'installed' via make)
add_test(NAME testDeviceDescriptorsLib WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMMAND testDeviceDescriptorsLib)

set_property(TEST testDeviceDescriptorsLib PROPERTY ENVIRONMENT "LD_LIBRARY_PATH=${CROSS_OUTPUT}/lib:${CMAKE_INSTALL_PREFIX}/lib")