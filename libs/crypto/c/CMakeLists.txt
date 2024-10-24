#--------------------------------------------------------------------------------------
#
# CMake definition for the Zilker 'crypto library'
# Should not be used directly, but imported from our parent
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

# build as shared library
add_library(xhCrypto STATIC ${SOURCES})
target_link_libraries(xhCrypto ssl crypto xhTypes xhUtil xhConcurrent)

# export our 'public' folder so others can include our headers
target_include_directories(xhCrypto PUBLIC public)

add_subdirectory(test)
