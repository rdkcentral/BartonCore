#------------------------------ tabstop = 4 ----------------------------------
#
# Copyright (C) 2024 Comcast
#
# All rights reserved.
#
# This software is protected by copyright laws of the United States
# and of foreign countries. This material may also be protected by
# patent laws of the United States and of foreign countries.
#
# This software is furnished under a license agreement and/or a
# nondisclosure agreement and may only be used or copied in accordance
# with the terms of those agreements.
#
# The mere transfer of this software does not imply any licenses of trade
# secrets, proprietary technology, copyrights, patents, trademarks, or
# any other form of intellectual property whatsoever.
#
# Comcast retains all ownership rights.
#
#------------------------------ tabstop = 4 ----------------------------------

#
# Created by Christian Leithner on 09/23/2024
#

include(BDSFindPackage)

# These are intended to be set at the project level as other cmake refer to them.
set(GLIB_MIN_VERSION 2.70.2)
set(CMOCKA_MIN_VERSION 1.1.5)
set(CURL_MIN_VERSION 8.4.0)
set(GOOGLETEST_MIN_VERSION 1.14.0)
set(OPENSSL_MIN_VERSION 1.1.1v)
set(UUID_MIN_VERSION 1.0.3)
set(XML_MIN_VERSION 2.9.8)
set(MBEDCRYPTO_MIN_VERSION 2.28.4)

# We depend on dbus client library and some headers but versioning may not
# exist for those, so we'll just let the linker find it and pray it
# works.
# TODO: find_symbol to make this a bit less of a prayer
#set(OTBR_GIT_TAG="790dc775144e33995cd1cb2c15b348849cacf737")

bds_find_package(NAME glib-2.0 MIN_VERSION ${GLIB_MIN_VERSION} REQUIRED)
bds_find_package(NAME gio-2.0 MIN_VERSION ${GLIB_MIN_VERSION} REQUIRED)
bds_find_package(NAME gio-unix-2.0 MIN_VERSION ${GLIB_MIN_VERSION} REQUIRED)
bds_find_package(NAME cmocka MIN_VERSION ${CMOCKA_MIN_VERSION} REQUIRED)
bds_find_package(NAME libcurl MIN_VERSION ${CURL_MIN_VERSION} REQUIRED)
bds_find_package(NAME libssl MIN_VERSION ${OPENSSL_MIN_VERSION} REQUIRED)
bds_find_package(NAME uuid MIN_VERSION ${UUID_MIN_VERSION} REQUIRED)
bds_find_package(NAME libxml-2.0 MIN_VERSION ${XML_MIN_VERSION} REQUIRED)
bds_find_package(NAME gtest MIN_VERSION ${GOOGLETEST_MIN_VERSION} REQUIRED)
bds_find_package(NAME gmock MIN_VERSION ${GOOGLETEST_MIN_VERSION} REQUIRED)
bds_find_package(NAME mbedcrypto MIN_VERSION ${MBEDCRYPTO_MIN_VERSION} REQUIRED)

if (BDS_MATTER)
    find_library(MATTER NAMES ${BDS_MATTER_LIB} REQUIRED)
endif()
