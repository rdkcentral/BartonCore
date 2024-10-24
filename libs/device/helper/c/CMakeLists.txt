#--------------------------------------------------------------------------------------
#
# CMake definition for the fcore 'device helper library'
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

add_library(xhDeviceHelper STATIC ${SOURCES})
target_link_libraries(xhDeviceHelper xhUtil xhLog xhTypes)

# export our 'public' folder so others can include our headers
target_include_directories(xhDeviceHelper PUBLIC public)

#TODO: test target
