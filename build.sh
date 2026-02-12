#!/bin/bash

if [ "$1" = "debug" ]; then
    BUILD_DIR=build-debug
    BUILD_TYPE=Debug
elif [ "$1" = "release" ]; then
     BUILD_DIR=build-release
     BUILD_TYPE=Release
else
    echo "Build type expected as \"release\" or \"debug\" (i.g. \"build.sh release\")"
    exit 1
fi

mkdir -p ${BUILD_DIR}
pushd ${BUILD_DIR}
cmake -Wno-dev -DCMAKE_BUILD_TYPE=${BUILD_TYPE} ..
make -j4
popd