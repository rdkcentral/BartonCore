# ------------------------------ tabstop = 4 ----------------------------------
#
# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2024 Comcast Cable Communications Management, LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# SPDX-License-Identifier: Apache-2.0
#
# ------------------------------ tabstop = 4 ----------------------------------

#
# Created by Christian Leithner on 09/23/2024
#

include(BCoreFindPackage)
include(BCoreFindIncludeDir)

# These are intended to be set at the project level as other cmake refer to them.
set(GLIB_MIN_VERSION 2.62.4)
set(CMOCKA_MIN_VERSION 1.1.5)
set(CURL_MIN_VERSION 7.82.0)
set(GOOGLETEST_MIN_VERSION 1.14.0)
set(OPENSSL_MIN_VERSION 1.1.1l)
set(OPENSSL_MAX_VERSION 1.1.1v)
set(UUID_MIN_VERSION 1.0.3)
set(XML_MIN_VERSION 2.9.8)
set(MBEDCRYPTO_MIN_VERSION 2.28.4)
set(LIBCERTIFIER_MIN_VERSION 2.2.2)

# We depend on dbus client library and some headers but versioning may not
# exist for those, so we'll just let the linker find it and pray it
# works.
# TODO: find_symbol to make this a bit less of a prayer
#set(OTBR_GIT_TAG="790dc775144e33995cd1cb2c15b348849cacf737")

bcore_find_package(NAME glib-2.0 MIN_VERSION ${GLIB_MIN_VERSION} REQUIRED)
bcore_find_package(NAME gio-2.0 MIN_VERSION ${GLIB_MIN_VERSION} REQUIRED)
bcore_find_package(NAME gio-unix-2.0 MIN_VERSION ${GLIB_MIN_VERSION} REQUIRED)
if(BUILD_TESTING)
    bcore_find_package(NAME cmocka MIN_VERSION ${CMOCKA_MIN_VERSION} REQUIRED)
    #bcore_find_include(FFF_INCLUDE_DIR fff.h fff "fff include directory" "")
endif()
# This is the version of libcurl that's linked against the older OpenSSL version
set(ENV{PKG_CONFIG_PATH} "/usr/local/lib/pkgconfig:$ENV{PKG_CONFIG_PATH}")
bcore_find_package(NAME libcurl MIN_VERSION ${CURL_MIN_VERSION} REQUIRED)
set(ENV{PKG_CONFIG_PATH} "/usr/local/openssl/lib/pkgconfig:$ENV{PKG_CONFIG_PATH}")
bcore_find_package(NAME libssl MIN_VERSION ${OPENSSL_MIN_VERSION} MAX_VERSION ${OPENSSL_MAX_VERSION} REQUIRED)
bcore_find_package(NAME uuid MIN_VERSION ${UUID_MIN_VERSION} REQUIRED)
bcore_find_package(NAME libxml-2.0 MIN_VERSION ${XML_MIN_VERSION} REQUIRED)
bcore_find_package(NAME mbedcrypto MIN_VERSION ${MBEDCRYPTO_MIN_VERSION} REQUIRED)

if (BCORE_MATTER)
    bcore_find_package(NAME certifier MIN_VERSION ${LIBCERTIFIER_MIN_VERSION} REQUIRED)
endif()
