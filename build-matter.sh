#!/usr/bin/env bash

set -e

MY_DIR=$(realpath $(dirname $0))
BUILD_DIR=${MY_DIR}/build
MATTER_BUILD_DIR=${BUILD_DIR}/matter
MATTER_INSTALL_DIR=${BUILD_DIR}/matter-install
MATTER_INSTALL_BIN_DIR=${MATTER_INSTALL_DIR}/bin

if [ -e ${MATTER_BUILD_DIR} ]; then
    echo "Matter build dir already exists at: ${MATTER_BUILD_DIR}. Delete it if you want to rebuild Matter."
    exit 0
fi

rm -rf ${MATTER_INSTALL_DIR}
mkdir -p ${BUILD_DIR}
cd ${BUILD_DIR}
git clone \
    --branch main \
    --depth 1 \
    ssh://rdkgerrithub.stb.r53.xcal.tv:29418/comcast-matter/sdk \
    matter

cd ${MATTER_BUILD_DIR}
./scripts/checkout_submodules.py --shallow --platform linux

# Ensure that the older OpenSSL version is used for the build
export PKG_CONFIG_PATH=/usr/local/openssl/lib/pkgconfig:$PKG_CONFIG_PATH

cd third_party/comcast/config/zilker
cmake -B cmake-build-debug -G "Unix Makefiles" -DCMAKE_INSTALL_PREFIX=${MATTER_INSTALL_DIR} -DCMAKE_BUILD_TYPE=Debug -DMATTER_CONF_DIR:PATH=~/.brtn-ds/matter
cmake --build cmake-build-debug
cd cmake-build-debug
make install
cd ${MATTER_BUILD_DIR}
git reset --hard

# Build and install the Matter example apps for use as test targets
cd ${MATTER_BUILD_DIR}
mkdir -p ${MATTER_INSTALL_BIN_DIR}
export TMPDIR=${BUILD_DIR}/tmp
export TERM=xterm
. ./scripts/activate.sh

./scripts/build/build_examples.py --target linux-x64-light-rpc build && cp out/linux-x64-light-rpc/chip-lighting-app ${MATTER_INSTALL_BIN_DIR} && rm -rf out
./scripts/build/build_examples.py --target linux-x64-lock build && cp out/linux-x64-lock/chip-lock-app ${MATTER_INSTALL_BIN_DIR} && rm -rf out
./scripts/build/build_examples.py --target linux-x64-thermostat build && cp out/linux-x64-thermostat/thermostat-app ${MATTER_INSTALL_BIN_DIR} && rm -rf out
./scripts/build/build_examples.py --target linux-x64-chip-tool build && cp out/linux-x64-chip-tool/chip-tool ${MATTER_INSTALL_BIN_DIR} && rm -rf out

# Clean up
rm -rf .environment
