#!/usr/bin/env bash

set -e

MY_DIR=$(realpath $(dirname $0))
pushd ${MY_DIR}

# this will fast exit if already built
./build-matter.sh

cmake -B build -DCMAKE_BUILD_TYPE=Debug -DBDS_MATTER_LIB=ZilkerMatter $@
cmake --build build

popd
