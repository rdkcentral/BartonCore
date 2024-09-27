#--------------------------------------------------------------------------------------
#
# CMake definition for the fcore 'config library'
# Should not be utilized directly, but imported from our parent
# file via the 'add_subdirectory()' routine.
#
#--------------------------------------------------------------------------------------

# define source files (using wildcard)
file(GLOB SOURCES "src/*.c")

pkg_check_modules(XML2 libxml-2.0>=${XML_MIN_VERSION})

add_library(xhConfig STATIC ${SOURCES})
target_link_libraries(xhConfig xhLog xhUtil xhConcurrent xhTime mbedcrypto ${XML2_LINK_LIBRARIES})
target_include_directories(xhConfig PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/public PRIVATE ${XML2_INCLUDE_DIRS})

install(TARGETS xhConfig DESTINATION lib)

add_subdirectory(test)