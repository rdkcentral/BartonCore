#--------------------------------------------------------------------------------------
#
# CMake definition for the fcore 'log library'
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

# rdk needs some additional include paths
if (CONFIG_PLATFORM_RDK)
    include_directories(
        ${CROSS_OUTPUT}/include/ccsp
        ${CROSS_OUTPUT}/include/dbus-1.0
        ${CROSS_OUTPUT}/lib/dbus-1.0/include
    )
endif()

set(LOG_SRC src/loggingDebug.c)

# Add the logging common file
set(LOG_SRC ${LOG_SRC} src/loggingCommon.c)

add_library(xhLog STATIC ${LOG_SRC})

bds_configure_glib()

# export our 'public' folder so others can include our headers
target_include_directories(xhLog PUBLIC public)

#TODO: test target
