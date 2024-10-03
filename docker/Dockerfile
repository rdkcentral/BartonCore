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

ARG LINENOISE_GIT_TAG="97d2850af13c339369093b78abe5265845d78220"
ARG OTBR_GIT_TAG="790dc775144e33995cd1cb2c15b348849cacf737"

# Copy over the third party patches
ADD patches /tmp/patches
ARG APPLY_PATCHES_PATH="/tmp/patches/applyPatches.sh"
# Make the apply patches script executable
RUN chmod +x ${APPLY_PATCHES_PATH}

RUN apt-get update && apt-get -y upgrade && DEBIAN_FRONTEND='noninteractive' apt-get install -y \
    lsb-release \
    ninja-build \
    python3 \
    python3-venv \
    sudo \
    jq \
    cmake \
    meson \
    libglib2.0-dev \
    libcjson-dev \
    libcmocka-dev \
    libgtest-dev \
    libgmock-dev \
    libssl-dev \
    libcurl4-openssl-dev \
    uuid-dev \
    libxml2-dev \
    libmbedtls-dev

# 'gn' (generate ninja)
RUN cd /tmp && \
    wget -O gn.zip "https://chrome-infra-packages.appspot.com/dl/gn/gn/linux-amd64/+/latest" && \
    unzip gn.zip && \
    cp gn /usr/bin && \
    chmod 755 /usr/bin/gn

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
