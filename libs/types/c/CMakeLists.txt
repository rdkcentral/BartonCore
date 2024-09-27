#--------------------------------------------------------------------------------------
#
# CMake definition for the fcore 'types library'
# Should not be utilized directly, but imported from our parent
# file via the 'add_subdirectory()' routine.
#
#--------------------------------------------------------------------------------------


# setup include paths
include_directories(
        ${CMAKE_CURRENT_SOURCE_DIR}/public
        ${ZILKER_INCLUDE_DIR}
)

# define source files (using wildcard)
file(GLOB SOURCES "src/*.[ch]" "public/icTypes/*.h")

add_library(xhTypes STATIC ${SOURCES})
target_link_libraries(xhTypes xhLog cjson)

# export our 'public' folder so others can include our headers
target_include_directories(xhTypes PUBLIC public)

# install in $PREFIX/lib
install(TARGETS xhTypes DESTINATION lib)

# add our 'unit test' to be part of the "make unitTest"
add_subdirectory(test)
