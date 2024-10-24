#--------------------------------------------------------------------------------------
#
# CMake definition for the fcore 'url library'
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

add_library(xhUrlHelper STATIC ${SOURCES})
target_link_libraries(xhUrlHelper xhConcurrent xhLog xhTypes xhUtil curl xhCrypto)

# export our 'public' folder so others can include our headers
target_include_directories(xhUrlHelper PUBLIC public)

# fetch platform details for urlHelper - userAgent and generate header
include(FindPkgConfig)
pkg_check_modules(CURL libcurl)

#Set the zilker release version
set(ZILKER_RELEASE_VERSION "Zilker/")
    execute_process(
        COMMAND git describe --tags
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        OUTPUT_VARIABLE ZILKER_RELEASE_VERSION_VALUE
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
string(CONCAT ZILKER_RELEASE_VERSION "${ZILKER_RELEASE_VERSION}" "${ZILKER_RELEASE_VERSION_VALUE}")

if(DEFINED ENV{ZILKER_DISTRO_VERSION})
   set(ZILKER_DISTRO_VERSION $ENV{ZILKER_DISTRO_VERSION})
endif()

set(USER_AGENT_VALUE "libcurl/${CURL_VERSION} (${CMAKE_SYSTEM_NAME};${CMAKE_SYSTEM_PROCESSOR}")

if(ZILKER_DISTRO_VERSION)
    string(APPEND USER_AGENT_VALUE ";${ZILKER_DISTRO_VERSION}")
endif()

string(APPEND USER_AGENT_VALUE ") ${ZILKER_RELEASE_VERSION}")

#Generate header for urlhelper-userAgent macros
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config_url_helper.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/config_url_helper.h)

INCLUDE_DIRECTORIES(BEFORE ${CMAKE_CURRENT_BINARY_DIR})

# add_zilker_test(
#         NAME urlHelperTest
#         TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/test/src/urlHelperTest.c
#         LINK_LIBRARIES xhUrlHelper xhCommServiceAPI
#         TYPE manual
# )

# # add our 'unit test' to be part of the "make unitTest"
# add_subdirectory(test)
