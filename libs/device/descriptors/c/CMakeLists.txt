#--------------------------------------------------------------------------------------
#
# CMake definition for the fcore 'device descriptor library'
# Should not be utilized directly, but imported from our parent
# file via the 'add_subdirectory()' routine.
#
#--------------------------------------------------------------------------------------
include(BDSConfigureGLib)

# setup include paths
include_directories(
        ${CMAKE_CURRENT_SOURCE_DIR}/public
        ${ZILKER_INCLUDE_DIR}
        ${CROSS_OUTPUT}/include/libxml2     # 3rd Party
)

# define source files (using wildcard)
file(GLOB SOURCES "src/*.c")

add_library(xhDeviceDescriptors STATIC ${SOURCES})
target_link_libraries(xhDeviceDescriptors xhLog xhTypes xhUtil xhXmlHelper)

# export our 'public' folder so others can include our headers
target_include_directories(xhDeviceDescriptors PUBLIC public PRIVATE)

# install in $PREFIX/lib
install(TARGETS xhDeviceDescriptors DESTINATION lib)

bds_configure_glib()

# add our test to be part of the "make manualTest"
add_subdirectory(test)
