include_guard(GLOBAL)

function(bds_add_glib_test)
    set(options)
    set(oneValueArgs NAME GLOG_DOMAIN)
    set(multiValueArgs SOURCES LIBRARIES INCLUDE_DIRECTORIES)
    cmake_parse_arguments(BDS_ADD_GLIB_TEST "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    add_executable(${BDS_ADD_GLIB_TEST_NAME} ${BDS_ADD_GLIB_TEST_SOURCES})
    target_link_libraries(${BDS_ADD_GLIB_TEST_NAME} PRIVATE brtnDeviceServiceStatic ${BDS_ADD_GLIB_TEST_LIBRARIES})
    target_compile_definitions(${BDS_ADD_GLIB_TEST_NAME} PRIVATE G_LOG_DOMAIN="${BDS_ADD_GLIB_TEST_GLOG_DOMAIN}")
    target_include_directories(${BDS_ADD_GLIB_TEST_NAME} PRIVATE ${BDS_ADD_GLIB_TEST_INCLUDE_DIRECTORIES})

    add_test(NAME ${BDS_ADD_GLIB_TEST_NAME} COMMAND ${BDS_ADD_GLIB_TEST_NAME})
    set_property(TEST ${BDS_ADD_GLIB_TEST_NAME} PROPERTY ENVIRONMENT "LD_LIBRARY_PATH=${CMAKE_PREFIX_PATH}/lib:${CMAKE_INSTALL_PREFIX}/lib:$ENV{LD_LIBRARY_PATH}")
endfunction()