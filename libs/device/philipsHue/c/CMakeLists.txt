#--------------------------------------------------------------------------------------
#
# CMake definition for the fcore 'device philips hue light library'
# Should not be utilized directly, but imported from our parent
# file via the 'add_subdirectory()' routine.
#
#--------------------------------------------------------------------------------------

# NOTE: should only be built if BARTON_CONFIG_PHILIPS_HUE is set

# setup include paths
include_directories(
        ${CMAKE_CURRENT_SOURCE_DIR}/public
        ${ZILKER_INCLUDE_DIR}
        ${CROSS_OUTPUT}/include     # 3rd Party
)

# define source files (using wildcard)
file(GLOB SOURCES "src/*.c")

add_library(xhPhilipsHue STATIC ${SOURCES})
target_link_libraries(xhPhilipsHue xhLog xhTypes xhUtil xhSsdp curl cjson m)

# export our 'public' folder so others can include our headers
target_include_directories(xhPhilipsHue PUBLIC public)

# add our test to be part of the "make manualTest"
add_subdirectory(test)

