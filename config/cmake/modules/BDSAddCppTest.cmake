include(CMakeParseArguments)
include(BDSConfigureGLib)

# Add a C++ unit test executable target.
# Arguments:
# - NAME the executable name (e.g., testMyTest).
# - SOURCES The sources with which to generate the test executable.
# - LIBS Libraries to link the test executable against
# - INCLUDES Header files to be included
function(bds_add_cpp_test)
    set(singleValueArgs NAME)
    set(multiValueArgs SOURCES LIBS INCLUDES)
    cmake_parse_arguments(BDS_ADD_CPP_TEST "" "${singleValueArgs}" "${multiValueArgs}" ${ARGN})

    add_executable(${BDS_ADD_CPP_TEST_NAME} ${BDS_ADD_CPP_TEST_SOURCES})
    target_link_libraries(${BDS_ADD_CPP_TEST_NAME} gtest gmock gmock_main ${BDS_ADD_CPP_TEST_LIBS})
    target_include_directories(${BDS_ADD_CPP_TEST_NAME} PRIVATE ${BDS_ADD_CPP_TEST_INCLUDES})
    set_target_properties(${BDS_ADD_CPP_TEST_NAME} PROPERTIES
                                                  SKIP_BUILD_RPATH false
                                                  BUILD_RPATH_USE_ORIGIN true
                                                  BUILD_RPATH ${CMAKE_PREFIX_PATH}/lib)
    include(GoogleTest)
    gtest_discover_tests(${BDS_ADD_CPP_TEST_NAME})
endfunction()
