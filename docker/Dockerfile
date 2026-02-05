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

###############################################################################
# Example workflow for building the image and pushing it to the GitHub Container
# Registry
#
# 1. Build the image:
#    `docker build -t ghcr.io/rdkcentral/barton_builder:some-tag .`
#
# 2. Authenticate with the GitHub Container Registry:
#    `echo $GITHUB_PAT | docker login ghcr.io -u $GITHUB_ACTOR --password-stdin`
#
#    where $GITHUB_PAT is your GitHub Personal Access Token and $GITHUB_ACTOR is
#    your GitHub username
#
# 3. Push the image to the GitHub Container Registry:
#    `docker push ghcr.io/rdkcentral/barton_builder:some-tag`
###############################################################################

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
# Fix pygobject-stubs to 2.14.0. 2.15.0+ requires libgirepository2.0 which requires some migration.
RUN pip3 install --upgrade --break-system-packages \
    pygobject==3.50.0 \
    pygobject-stubs==2.14.0 \
    setuptools==73.0.0 \
    python_stdnum \
    pytest \
    pytest-forked

# pygobject-stubs distributed the stub (pyi) files for standard distribution girs, but we also want its tools so we can make
# out own stubs. Those don't come with the pip package.
RUN cd /tmp && \
    git clone https://github.com/pygobject/pygobject-stubs.git && \
    cd pygobject-stubs/tools && \
    git checkout v2.14.0 && \
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
ARG OTBR_GIT_TAG="e851c2a80fb036e6987bedac399184a729f638d4"
ARG FFF_GIT_TAG="5111c61e1ef7848e3afd3550044a8cf4405f4199"

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

# Fake Function Framework (FFF)
#
# NOTICE: Currently, FFF_GIT_TAG points to a specific commit SHA from master instead
# of the v1.1 tag (from 2019) as significant development has been made since the
# last official release.
# This SHA represents a stable point in the master branch with all needed features.
RUN cd /tmp && \
    git clone --depth 1 https://github.com/meekrosoft/fff.git && \
    cd fff && \
    git fetch --depth 1 origin ${FFF_GIT_TAG} && \
    git checkout ${FFF_GIT_TAG} && \
    mkdir -p /usr/local/include/fff && \
    cp /tmp/fff/fff.h /usr/local/include/fff/ && \
    rm -rf /tmp/fff

# 'gn' (generate ninja)
RUN cd /tmp && \
    wget -O gn.zip "https://chrome-infra-packages.appspot.com/dl/gn/gn/linux-amd64/+/latest" && \
    unzip gn.zip && \
    mv gn /usr/bin && \
    chmod 755 /usr/bin/gn && \
    rm gn.zip

# OTBR
RUN cd /tmp && \
    git clone --depth 1 https://github.com/openthread/ot-br-posix && \
    cd ot-br-posix && \
    git fetch --depth 1 origin ${OTBR_GIT_TAG} && \
    git checkout ${OTBR_GIT_TAG} && \
    ${APPLY_PATCHES_PATH} /tmp/patches/otbr . && \
    cd script && \
    OTBR_MDNS=avahi ./bootstrap && \
    ./cmake-build \
    -DOTBR_DUA_ROUTING=ON \
    -DOTBR_DNSSD_DISCOVERY_PROXY=ON \
    -DOTBR_SRP_ADVERTISING_PROXY=ON \
    -DOTBR_BORDER_ROUTING=ON \
    -DOTBR_TREL=ON \
    -DOTBR_NAT64=ON \
    -DOT_TREL=ON \
    -DOT_DNS_CLIENT=ON \
    -DOT_DNSSD_SERVER=ON \
    -DOT_SRP_CLIENT=ON \
    -DOT_SRP_SERVER=ON \
    -DOT_THREAD_VERSION=1.4 \
    -DCMAKE_BUILD_TYPE=Release  \
    -DOT_DHCP6_CLIENT=ON \
    -DOT_DHCP6_SERVER=ON \
    -DOTBR_DHCP6_PD=OFF \
    -DOTBR_DNS_UPSTREAM_QUERY=ON \
    -DBUILD_TESTING=OFF \
    -DOTBR_COVERAGE=OFF \
    -DOT_MLR=ON   \
    -DOTBR_DBUS=ON \
    -DOT_PLATFORM=posix \
    -DOT_ECDSA=ON   \
    -DOT_FIREWALL=OFF \
    -DOT_CHANNEL_MANAGER=OFF \
    -DOT_CHANNEL_MONITOR=OFF \
    -DOTBR_BACKBONE_ROUTER=ON \
    -DOT_BACKBONE_ROUTER_DUA_NDPROXYING=ON \
    -DOT_BACKBONE_ROUTER_MULTICAST_ROUTING=ON \
    -DOTBR_MDNS=avahi \
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
    -DOTBR_TELEMETRY_DATA_API=ON \
    && \
    cd ../build/otbr/ && \
    ninja install && \
    cp -a /tmp/ot-br-posix/third_party/openthread/repo/include/openthread /usr/local/include && \
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
    rm -rf /tmp/linenoise /tmp/patches

# Download, install, and clean up OpenSSL 1.1.1v, specifically for use in our project
RUN cd /tmp && \
    wget https://github.com/openssl/openssl/releases/download/OpenSSL_1_1_1v/openssl-1.1.1v.tar.gz && \
    tar -xzvf openssl-1.1.1v.tar.gz && \
    cd openssl-1.1.1v && \
    ./config --prefix=/usr/local/openssl --openssldir=/usr/local/openssl && \
    make && \
    make install && \
    rm -rf /tmp/openssl-1.1.1v.tar.gz /tmp/openssl-1.1.1v
COPY openssl.conf /etc/ld.so.conf.d/
RUN ldconfig

# Download, build, and install libcurl, but link against OpenSSL 1.1.1v specifically
RUN cd /tmp && \
    wget https://curl.se/download/curl-7.82.0.tar.gz && \
    tar -xzvf curl-7.82.0.tar.gz && \
    cd curl-7.82.0 && \
    ./configure --with-ssl=/usr/local/openssl && \
    make && \
    make install && \
    rm -rf /tmp/curl-7.82.0.tar.gz /tmp/curl-7.82.0

RUN ls /tmp/

# Ensure the system uses OpenSSL 3 by default
ENV LD_LIBRARY_PATH=/usr/lib/x86_64-linux-gnu:/usr/local/openssl/lib:$LD_LIBRARY_PATH

###############################################################################
# Development build stage
#
# This stage is responsible for installing packages and other tasks useful for
# development.
###############################################################################
FROM third_party_builder AS dev_builder

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

RUN pip3 install --upgrade --break-system-packages \
    stack-pr

# Make some /usr/ stuff open permissions. These are individual containers,
# so this will let people install barton outputs.
RUN chmod -R +777 /usr/local && \
    chmod +777 /usr/lib && \
    chmod -R +777 /usr/share/gir-1.0

# Set up pcap permissions group with a GID that is unlikely to conflict with any existing groups
RUN groupadd -g 10000 pcap && \
    chgrp pcap /usr/bin/tcpdump && \
    chmod 750 /usr/bin/tcpdump && \
    setcap cap_net_raw,cap_net_admin=eip /usr/bin/tcpdump

# Add bashrc_term_mods. We're looking to inject this before user creation/customization
COPY bashrc_term_mods /tmp/bashrc_term_mods

# This script will set up the user specific environment. It is purposefully not run yet,
# in favor of user creation at run time in the entrypoint script on the CLI, or in the extended
# Dockerfile.devcontainer to bake the user into the image for the devcontainer.
COPY setupUser.sh /setupUser.sh
RUN chmod +x /setupUser.sh

# Setup our entrypoint
COPY entrypoint.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh
ENTRYPOINT ["/entrypoint.sh"]
CMD ["bash"]
