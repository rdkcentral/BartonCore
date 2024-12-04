#!/usr/bin/env bash

set -e

MY_DIR=$(realpath $(dirname $0))
pushd ${MY_DIR}

cmake -B build -DCMAKE_BUILD_TYPE=Debug -DBDS_MATTER_LIB=ZilkerMatter $@
cmake --build build --parallel $(($(nproc) - 1))

popd
