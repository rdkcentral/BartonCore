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
# Created by Kevin Funderburg on 09/11/2024
#

# Example build command:
#    docker build \
#    --build-arg BUILDER_USER=$(id -u -n) \
#    --build-arg BUILDER_GID=$(id -g) \
#    --build-arg BUILDER_UID=$(id -u) \
#    -t barton_builder_$USER .

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
    unzip \
    wget \
    zip \
    gcc-11 \
    g++-11

# Set the default gcc and g++ versions to 11 because our current checkout of matter can't handle 13
RUN update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-11 30 && \
    update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-11 30 && \
    update-alternatives --install /usr/bin/gcov gcov /usr/bin/gcov-11 30 && \
    update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-13 20 && \
    update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-13 20 && \
    update-alternatives --install /usr/bin/gcov gcov /usr/bin/gcov-13 20

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
# Add support for Python including GObject introspection
###############################################################################
RUN apt-get update && apt-get -y upgrade && DEBIAN_FRONTEND='noninteractive' apt-get install -y \
    python3-full \
    python3-dev \
    python3-mako \
    python3-markdown \
    python3-pip \
    python3-venv \
    flex \
    bison \
    libglib2.0-dev \
    libcairo2-dev \
    libffi-dev \
    gtk-doc-tools \
    libgirepository1.0-dev

# Fix setuptools on v73. v74+ removed some distutils modules that breaks gobject introspection (among a lot of other projects)
RUN pip3 install --upgrade --break-system-packages \
    pygobject \
    pygobject-stubs \
    setuptools==73.0.0 \
    python_stdnum \
    pytest

# pygobject-stubs distributed the stub (pyi) files for standard distribution girs, but we also want its tools so we can make
# out own stubs. Those don't come with the pip package.
RUN cd /tmp && \
    git clone https://github.com/pygobject/pygobject-stubs.git && \
    cd pygobject-stubs/tools && \
    mkdir -p /usr/bin/pygobject-stubs && \
    cp generate.py /usr/bin/pygobject-stubs && \
    cp parse.py /usr/bin/pygobject-stubs && \
    rm -rf /tmp/pygobject-stubs

# Create a symlink for the versioned dist-packages directory so the container can determine
# the correct PYTHONPATH at runtime
RUN bash -c "set -o pipefail && \
    PYTHON_VERSION=$(python3 --version | awk '{print $2}' | awk -F. '{print $1"."$2}') && \
    mkdir -p /usr/local/lib/python3.x && \
    ln -s /usr/local/lib/python${PYTHON_VERSION}/dist-packages /usr/local/lib/python3.x/dist-packages"

###############################################################################
# Third party build stage
#
# This stage is responsible for building specific third-party libraries that
# need to be versioned precisely for the Barton image build. Since apt cannot
# be relied upon to provide the correct versions, these libraries must be built
# from source.
###############################################################################
FROM base_builder AS third_party_builder

ARG LINENOISE_GIT_TAG="d895173d679be70bcd8b23041fff3e458e1a3506"
ARG OTBR_GIT_TAG="790dc775144e33995cd1cb2c15b348849cacf737"

# Copy over the third party patches
ADD patches /tmp/patches
ARG APPLY_PATCHES_PATH="/tmp/patches/applyPatches.sh"
# Make the apply patches script executable
RUN chmod +x ${APPLY_PATCHES_PATH}

RUN apt-get update && apt-get -y upgrade && DEBIAN_FRONTEND='noninteractive' apt-get install -y \
    lsb-release \
    ninja-build \
    sudo \
    jq \
    cmake \
    meson \
    libcjson-dev \
    libcmocka-dev \
    libgtest-dev \
    libgmock-dev \
    libssl-dev \
    libcurl4-openssl-dev \
    uuid-dev \
    libxml2-dev \
    libmbedtls-dev \
    libdbus-1-dev \
    libavahi-client-dev \
    libreadline-dev \
    checkinstall \
    zlib1g-dev \
    socat

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
COPY otbr-agent.conf /etc/dbus-1/system.d/

# Simulated RCP for OpenThread
RUN cd /tmp && \
    git clone --depth 1 --recursive https://github.com/openthread/openthread.git && \
    cd openthread && \
    ./script/cmake-build simulation && \
    cp build/simulation/examples/apps/ncp/ot-rcp /usr/local/sbin && \
    rm -rf /tmp/openthread

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

# Download, install, and clean up OpenSSL 1.1.1v, specifically for use in our project
RUN cd /tmp && \
    wget https://github.com/openssl/openssl/releases/download/OpenSSL_1_1_1v/openssl-1.1.1v.tar.gz && \
    tar -xzvf openssl-1.1.1v.tar.gz && \
    cd openssl-1.1.1v && \
    ./config --prefix=/usr/local/openssl --openssldir=/usr/local/openssl && \
    make && \
    make install && \
    rm -rf /tmp/openssl-1.1.1v.tar.gz /tmp/openssl-1.1.1v

# Download, build, and install libcurl, but link against OpenSSL 1.1.1v specifically
RUN cd /tmp && \
    wget https://curl.se/download/curl-7.82.0.tar.gz && \
    tar -xzvf curl-7.82.0.tar.gz && \
    cd curl-7.82.0 && \
    ./configure --with-ssl=/usr/local/openssl && \
    make && \
    make install && \
    rm -rf /tmp/curl-7.82.0.tar.gz /tmp/curl-7.82.0

# Ensure the system uses OpenSSL 3 by default
ENV LD_LIBRARY_PATH=/usr/lib/x86_64-linux-gnu:/usr/local/openssl/lib:$LD_LIBRARY_PATH

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
    clang-format \
    silversearcher-ag \
    fd-find \
    gdb \
    rr \
    vim \
    pipx \
    gh \
    fish \
    strace \
    tcpdump \
    lcov \
    rsync \
    bash-completion

# Make some /usr/ stuff open permissions. These are individual containers,
# so this will let people install barton outputs.
RUN chmod -R +777 /usr/local && \
    chmod +777 /usr/lib && \
    chmod -R +777 /usr/share/gir-1.0

ARG BUILDER_USER=""
ARG BUILDER_GID=""
ARG BUILDER_UID=""

# Create the non-root user
RUN groupadd --gid $BUILDER_GID $BUILDER_USER && \
    useradd --uid $BUILDER_UID --gid $BUILDER_GID -m $BUILDER_USER && \
    echo $BUILDER_USER ALL=\(root\) NOPASSWD:ALL > /etc/sudoers.d/$BUILDER_USER && \
    chmod 0440 /etc/sudoers.d/$BUILDER_USER

# Set up pcap permissions
RUN groupadd pcap && usermod -a -G pcap $BUILDER_USER \
    && chgrp pcap /usr/bin/tcpdump \
    && chmod 750 /usr/bin/tcpdump \
    && setcap cap_net_raw,cap_net_admin=eip /usr/bin/tcpdump

# Setup our entrypoint
COPY entrypoint.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh
ENTRYPOINT ["/entrypoint.sh"]
CMD ["bash"]

USER $BUILDER_USER

RUN pipx install stack-pr && \
    pipx ensurepath

# Add bashrc_term_mods. We're looking to inject this before
# key variables are unset
COPY bashrc_term_mods /tmp/bashrc_term_mods
RUN boundary=$(($(sed -n '/unset color_prompt/=' ~/.bashrc)-1)) && \
    sed -i "${boundary}r/tmp/bashrc_term_mods" ~/.bashrc
