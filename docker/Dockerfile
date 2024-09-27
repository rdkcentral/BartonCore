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
# Created by Kevin Funderburg on 09/11/2024
#

#
# This build needs access to private repositories. Use the following build
# command to create this image. Ensure that the $HOME folder includes the .ssh
# folder with the necessary ssh credentials to access the private repositories.
#
# Build command:
#    docker build \
#    --build-context home=$HOME \
#    --build-arg BUILDER_USER=$(id -u -n) \
#    --build-arg BUILDER_GID=$(id -g) \
#    --build-arg BUILDER_UID=$(id -u) \
#    -t barton_builder .

###############################################################################
# base stage which provides a base image which has packages to support basic
# download functionality
###############################################################################
FROM ubuntu:24.04 AS base

# Delete the default ubuntu docker user: it can cause issues with user id 1000 on the host.
RUN touch /var/mail/ubuntu && chown ubuntu /var/mail/ubuntu && userdel -r ubuntu

RUN apt-get update && apt-get -y upgrade && DEBIAN_FRONTEND='noninteractive' apt-get install -y \
    build-essential \
    libarchive-tools \
    python3-pip \
    unzip \
    wget \
    zip \
    gcc-11 \
    g++-11

# Set the default gcc and g++ versions to 11 because our current checkout of matter can't handle 13
RUN update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-11 30 && \
    update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-11 30 && \
    update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-13 20 && \
    update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-13 20

###############################################################################
# expands the base stage to allow for building of packages with common tools
###############################################################################
FROM base AS base_builder
RUN apt-get update && apt-get -y upgrade && DEBIAN_FRONTEND='noninteractive' apt-get install -y \
    autoconf \
    autoconf-archive \
    git \
    libtool \
    pkg-config \
    libsystemd-dev

###############################################################################
# Third party build stage
#
# This stage is responsible for building specific third-party libraries that
# need to be versioned precisely for the Barton image build. Since apt cannot
# be relied upon to provide the correct versions, these libraries must be built
# from source.
###############################################################################
FROM base_builder AS third_party_builder

ARG CJSON_VERSION="1.7.14"
ARG CMAKE_VERSION="3.27.7"
ARG CMAKE_VERSION_SHORT="3.27"
ARG CMOCKA_VERSION="1.1.5"
ARG CMOCKA_VERSION_SHORT="1.1"
ARG CURL_VERSION="8.4.0"
ARG GLIB_VERSION="2.70.2"
ARG GLIB_VERSION_SHORT="2.70"
ARG GOOGLETEST_VERSION="1.14.0"
ARG LIBFFI_VERSION="3.3"
ARG LINENOISE_GIT_TAG="97d2850af13c339369093b78abe5265845d78220"
ARG MESON_VERSION="0.56.2"
ARG OPENSSL_VERSION="1.1.1v"
ARG OPENSSL_VERSION_UNDERSCORED="1_1_1v"
ARG OTBR_GIT_TAG="790dc775144e33995cd1cb2c15b348849cacf737"
ARG PCRE_VERSION="8.45"
ARG UUID_VERSION="1.0.3"
ARG ZLIB_VERSION="1.2.11"
ARG XML_VERSION="2.9.8"
ARG MBEDTLS_VERSION="2.28.4"

# Copy over the third party patches
ADD patches /tmp/patches
ARG APPLY_PATCHES_PATH="/tmp/patches/applyPatches.sh"
# Make the apply patches script executable
RUN chmod +x ${APPLY_PATCHES_PATH}

# packages necessary for building 3rd party libraries from source
RUN apt-get update && apt-get -y upgrade && DEBIAN_FRONTEND='noninteractive' apt-get install -y \
    lsb-release \
    ninja-build \
    python3 \
    python3-venv \
    sudo \
    jq

# cmake
RUN wget -O /tmp/cmake.sh "https://cmake.org/files/v${CMAKE_VERSION_SHORT}/cmake-${CMAKE_VERSION}-linux-x86_64.sh" && \
    chmod 755 /tmp/cmake.sh && \
    /tmp/cmake.sh --prefix=/usr/local --exclude-subdir

# meson
RUN cd /tmp && \
    wget -O meson.tar.gz "https://github.com/mesonbuild/meson/releases/download/${MESON_VERSION}/meson-${MESON_VERSION}.tar.gz" && \
    bsdtar -xf meson.tar.gz && \
    cd meson-${MESON_VERSION} && \
    python3 setup.py install && \
    cd /tmp && \
    rm -rf meson-${MESON_VERSION} && \
    rm meson.tar.gz

# cJSON
# Dependencies: none
RUN cd /tmp && \
    wget -O cjson.tar.gz "https://github.com/DaveGamble/cJSON/archive/refs/tags/v${CJSON_VERSION}.tar.gz" && \
    bsdtar -xf cjson.tar.gz && \
    cd cJSON-${CJSON_VERSION} && \
    mkdir build && \
    cd build && \
    cmake -DENABLE_CJSON_TEST=Off .. && \
    make install && \
    cd /tmp && \
    rm -rf cJSON-${CJSON_VERSION} && \
    rm cjson.tar.gz

# cmocka
# Dependencies: none
RUN cd /tmp && \
    wget -O cmocka.tar.gz "https://cmocka.org/files/${CMOCKA_VERSION_SHORT}/cmocka-${CMOCKA_VERSION}.tar.xz" && \
    bsdtar -xf cmocka.tar.gz && \
    cd cmocka-${CMOCKA_VERSION} && \
    mkdir build && \
    cd build && \
    cmake -DCMAKE_BUILD_TYPE=Debug .. && \
    make install && \
    cd /tmp && \
    rm -rf cmocka-${CMOCKA_VERSION} && \
    rm cmocka.tar.gz

# googletest
# Dependencies: none
RUN cd /tmp && \
    wget -O googletest.tar.gz "https://github.com/google/googletest/archive/refs/tags/v${GOOGLETEST_VERSION}.zip" && \
    bsdtar -xf googletest.tar.gz && \
    cd googletest-${GOOGLETEST_VERSION} && \
    mkdir build && \
    cd build && \
    cmake .. && \
    make install && \
    cd /tmp && \
    rm -rf googletest-${GOOGLETEST_VERSION} && \
    rm googletest.tar.gz

# openssl
# Dependencies: none
RUN cd /tmp && \
    wget -O openssl.tar.gz "https://github.com/openssl/openssl/releases/download/OpenSSL_${OPENSSL_VERSION_UNDERSCORED}/openssl-${OPENSSL_VERSION}.tar.gz" && \
    bsdtar -xf openssl.tar.gz && \
    cd openssl-${OPENSSL_VERSION} && \
    ${APPLY_PATCHES_PATH} /tmp/patches/openssl . && \
    ./config -DOPENSSL_NO_HEARTBEATS shared && \
    make -j `nproc` && \
    make install && \
    cd /tmp && \
    rm -rf openssl-${OPENSSL_VERSION} && \
    rm openssl.tar.gz

# zlib
# Dependencies: none
RUN cd /tmp && \
    wget -O zlib.tar.gz "https://github.com/madler/zlib/archive/refs/tags/v${ZLIB_VERSION}.tar.gz" && \
    bsdtar -xf zlib.tar.gz && \
    cd zlib-${ZLIB_VERSION} && \
    ./configure --shared --uname=linux && \
    mkdir build && \
    cd build && \
    cmake .. && \
    make install && \
    cd /tmp && \
    rm -rf zlib-${ZLIB_VERSION} && \
    rm zlib.tar.gz

# curl
# Dependencies: openssl, zlib
RUN cd /tmp && \
    wget -O curl.tar.gz "https://curl.se/download/curl-${CURL_VERSION}.tar.gz" && \
    bsdtar -xf curl.tar.gz && \
    cd curl-${CURL_VERSION} && \
    ./configure --without-mbedtls \
    --with-ssl \
    --with-zlib \
    --enable-ipv6 \
    --without-winssl \
    --without-cyassl \
    --disable-ldap \
    --disable-ldaps \
    --disable-dict \
    --disable-telnet \
    --disable-pop3 \
    --disable-imap \
    --disable-smb \
    --disable-smtp \
    --disable-gopher && \
    mkdir build && \
    cd build && \
    cmake .. && \
    make install && \
    cd /tmp && \
    rm -rf curl-${CURL_VERSION} && \
    rm curl.tar.gz

# libffi
# Dependencies: none
RUN cd /tmp && \
    wget -O libffi.tar.gz "https://github.com/libffi/libffi/releases/download/v${LIBFFI_VERSION}/libffi-${LIBFFI_VERSION}.tar.gz" && \
    bsdtar -xf libffi.tar.gz && \
    cd libffi-${LIBFFI_VERSION} && \
    ./configure && \
    make -j `nproc` && \
    make install && \
    cd /tmp && \
    rm -rf libffi-${LIBFFI_VERSION} && \
    rm libffi.tar.gz

# pcre
# Dependencies: none
RUN cd /tmp && \
    wget -O pcre.tar.gz "https://sourceforge.net/projects/pcre/files/pcre/${PCRE_VERSION}/pcre-${PCRE_VERSION}.tar.gz/download" && \
    bsdtar -xf pcre.tar.gz && \
    mkdir -p pcre-${PCRE_VERSION}/build && \
    cd pcre-${PCRE_VERSION}/build && \
    cmake -DCMAKE_POSITION_INDEPENDENT_CODE=ON .. && \
    make install && \
    cd /tmp && \
    rm -rf pcre-${PCRE_VERSION} && \
    rm pcre.tar.gz

# glib
# Dependecies: libffi, pcre, zlib
RUN cd /tmp && \
    wget -O glib.tar.xz "https://download.gnome.org/sources/glib/${GLIB_VERSION_SHORT}/glib-${GLIB_VERSION}.tar.xz" && \
    bsdtar -xf glib.tar.xz && \
    cd glib-${GLIB_VERSION} && \
    ${APPLY_PATCHES_PATH} /tmp/patches/glib . && \
    meson _build && \
    ninja -C _build install && \
    cd /tmp && \
    rm -rf glib-${GLIB_VERSION} && \
    rm glib.tar.xz

# libuuid
# Dependencies: none
RUN cd /tmp && \
    wget -O libuuid.tar.gz "https://sourceforge.net/projects/libuuid/files/libuuid-${UUID_VERSION}.tar.gz/download" && \
    bsdtar -xf libuuid.tar.gz && \
    cd libuuid-${UUID_VERSION} && \
    ./configure && \
    make -j `nproc` && \
    make install && \
    cd /tmp && \
    rm -rf libuuid-${UUID_VERSION} && \
    rm libuuid.tar.gz

# 'gn' (generate ninja)
RUN cd /tmp && \
    wget -O gn.zip "https://chrome-infra-packages.appspot.com/dl/gn/gn/linux-amd64/+/latest" && \
    unzip gn.zip && \
    cp gn /usr/bin && \
    chmod 755 /usr/bin/gn

# xml2
# Dependencies: none
RUN cd /tmp && \
    wget -O xml.tar.gz "https://github.com/GNOME/libxml2/archive/v${XML_VERSION}.tar.gz" && \
    bsdtar -xf xml.tar.gz && \
    cd libxml2-${XML_VERSION} && \
    autoreconf --force --install && \
    ./configure with_lzma=no with_zlib=no with_python=no && \
    make install && \
    cd /tmp && \
    rm -rf libxml2-${XML_VERSION} && \
    rm xml.tar.gz

# mbedtls
# Dependencies: none
RUN cd /tmp && \
    git clone https://github.com/ARMmbed/mbedtls.git && \
    cd mbedtls && \
    git checkout mbedtls-${MBEDTLS_VERSION} && \
    scripts/config.pl set MBEDTLS_THREADING_PTHREAD && \
    scripts/config.pl set MBEDTLS_THREADING_C && \
    cmake -DUSE_SHARED_MBEDTLS_LIBRARY=On -DENABLE_TESTING=Off -DCMAKE_C_FLAGS=-std=gnu99 && \
    make install && \
    cd /tmp && \
    rm -rf mbedtls

# OTBR
RUN cd /tmp && \
    git clone --depth 1 https://github.com/openthread/ot-br-posix && \
    cd ot-br-posix && \
    git fetch --depth 1 origin ${OTBR_GIT_TAG} && \
    git checkout ${OTBR_GIT_TAG} && \
    ${APPLY_PATCHES_PATH} /tmp/patches/otbr . && \
    cd script && \
    ./bootstrap && \
    ./cmake-build -DOTBR_DBUS=ON -DCMAKE_POSITION_INDEPENDENT_CODE=ON && \
    cd ../build/otbr/ && \
    ninja install && \
    rm -rf /tmp/ot-br-posix

# linenoise
# Dependencies: none
RUN cd /tmp && \
    git clone --depth 1 https://github.com/antirez/linenoise.git && \
    cd linenoise && \
    git fetch --depth 1 origin ${LINENOISE_GIT_TAG} && \
    git checkout ${LINENOISE_GIT_TAG} && \
    ${APPLY_PATCHES_PATH} /tmp/patches/linenoise . && \
    mkdir build && \
    cd build && \
    cmake .. && \
    make install && \
    rm -rf /tmp/linenoise

# matter
# Dependencies: curl, openssl
# Matter has a few git clone commands to private repos, so we need to get the ssh credentials
# from the host machine. The docker builder cannot access the hosts $HOME folder by default, so
# the home folder must be declared as an additional context in the docker build command.
# See the example build command at the top of this file.
RUN --mount=from=home,type=bind,source=.ssh,target=/root/.ssh \
    cd /tmp && \
    git clone \
    --depth 1 \
    --shallow-submodules \
    --recurse-submodules=third_party/mbedtls/ \
    --recurse-submodules=third_party/nlunit-test/repo \
    --recurse-submodules=third_party/nlassert/repo \
    --recurse-submodules=third_party/nlio/repo \
    --recurse-submodules=third_party/comcast \
    --recurse-submodules=third_party/pigweed/repo \
    --recurse-submodules=third_party/jsoncpp \
    ssh://rdkgerrithub.stb.r53.xcal.tv:29418/comcast-matter/sdk && \
    cd sdk/third_party/comcast/config/zilker && \
    cmake -B cmake-build-debug -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Debug -DMATTER_CONF_DIR:PATH=/tmp/matter-zilker && \
    cmake --build cmake-build-debug && \
    cd cmake-build-debug && \
    make install && \
    cd /tmp && \
    rm -rf sdk

###############################################################################
# User creation stage
#
# This stage is responsible for creating a non-root user within the image and
# installing packages useful for development. This stage is primarily intended
# for development purposes, i.e. a devcontainer in VSCode.
###############################################################################
FROM third_party_builder AS user_builder

# Install packages useful for development
RUN apt-get update && apt-get -y upgrade && DEBIAN_FRONTEND='noninteractive' apt-get install -y \
    clangd \
    clang-format

ARG BUILDER_USER=""
ARG BUILDER_GID=""
ARG BUILDER_UID=""

# Create the non-root user
RUN groupadd --gid $BUILDER_GID $BUILDER_USER && \
    useradd --uid $BUILDER_UID --gid $BUILDER_GID -m $BUILDER_USER && \
    echo $BUILDER_USER ALL=\(root\) NOPASSWD:ALL > /etc/sudoers.d/$BUILDER_USER && \
    chmod 0440 /etc/sudoers.d/$BUILDER_USER
