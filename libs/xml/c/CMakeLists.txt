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
        ${CROSS_OUTPUT}/include/libxml2     # 3rd Party
)

# define source files (using wildcard)
file(GLOB SOURCES "src/*.c")

pkg_check_modules(XML2 libxml-2.0>=${XML_MIN_VERSION})

add_library(xhXmlHelper STATIC ${SOURCES})
target_link_libraries(xhXmlHelper xhTypes xhUtil ${XML2_LINK_LIBRARIES})

# export xml2 includes so that cmake transitively adds it to dependent targets. It's a little lazy
target_include_directories(xhXmlHelper PUBLIC public ${XML2_INCLUDE_DIRS})

#TODO: test target
