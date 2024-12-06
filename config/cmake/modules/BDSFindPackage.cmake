include_guard(GLOBAL)
include(CMakeParseArguments)
include(FindPkgConfig)

function(bds_find_package)
    set(options REQUIRED)
    set(oneValueArgs NAME MIN_VERSION MAX_VERSION)
    set(multiValueArgs SOURCES LIBRARIES INCLUDE_DIRECTORIES)
    cmake_parse_arguments(BDS_FIND_PACKAGE "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if (BDS_FIND_PACKAGE_MIN_VERSION)
        set(PKG_CONFIG_VERSION_CHECK ">=${BDS_FIND_PACKAGE_MIN_VERSION}")

        if (BDS_FIND_PACKAGE_MAX_VERSION)
            set(FIND_PACKAGE_VERSION_CHECK ${BDS_FIND_PACKAGE_MIN_VERSION}...${BDS_FIND_PACKAGE_MAX_VERSION})
        else()
            set(FIND_PACKAGE_VERSION_CHECK ${BDS_FIND_PACKAGE_MIN_VERSION})
        endif()
    endif()

    if (BDS_FIND_PACKAGE_REQUIRED)
        set(PACKAGE_REQUIRED "REQUIRED")
    endif()

    pkg_check_modules(BDS_PACKAGE ${BDS_FIND_PACKAGE_NAME}${PKG_CONFIG_VERSION_CHECK})

    if (NOT BDS_PACKAGE_FOUND)
        find_package(${BDS_FIND_PACKAGE_NAME} ${FIND_PACKAGE_VERSION_CHECK})

	if (NOT ${BDS_FIND_PACKAGE_NAME}_FOUND)
	    # Last ditch, try find_library
	    find_library(BDS_LIBRARY NAMES ${BDS_FIND_PACKAGE_NAME} NO_CMAKE_FIND_ROOT_PATH ${PACKAGE_REQUIRED})
	endif()
    endif()
endfunction()
