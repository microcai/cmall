#!/bin/bash

pushd . > '/dev/null';
SCRIPT_PATH="${BASH_SOURCE[0]:-$0}";

while [ -h "$SCRIPT_PATH" ];
do
    cd "$( dirname -- "$SCRIPT_PATH"; )";
    SCRIPT_PATH="$( readlink -f -- "$SCRIPT_PATH"; )";
done

cd "$( dirname -- "$SCRIPT_PATH"; )" > '/dev/null';
SCRIPT_PATH="$( pwd; )";
popd  > '/dev/null';

export CMAKE_AR=llvm-ar
export CC=clang-cl
export CXX=clang-cl
LLVMAR=`which llvm-ar`
LINK=`which lld-link`

cmake   -DCMAKE_BUILD_TYPE:STRING=Release \
        -DUSE_BUNDLED_ODB_FILE=ON \
        -DMDBX_WITHOUT_MSVC_CRT=OFF \
        -DENABLE_IOURING=OFF \
        -DMSVC=ON \
        -DCMAKE_AR=${LLVMAR} \
        -DMSVC_LIB_EXE=${LINK} \
        -DENABLE_SYSTEM_OPENSSL=OFF \
        -DWIN32=ON \
        -DCMAKE_SIZEOF_VOID_P=8 \
        -DENABLE_BUILD_WERROR=OFF \
        -DENABLE_STATIC_LINK_TO_GCC=OFF \
        -DUSE_HTTP=OFF \
        -DML64=OFF \
        -DENABLE_LLD=ON \
        "${SCRIPT_PATH}"


cmake   -DCMAKE_BUILD_TYPE:STRING=Release \
        -DUSE_BUNDLED_ODB_FILE=ON \
        -DMDBX_WITHOUT_MSVC_CRT=OFF \
        -DENABLE_IOURING=OFF \
        -DMSVC=ON \
        -DCMAKE_AR=${LLVMAR} \
        -DMSVC_LIB_EXE=${LINK} \
        -DENABLE_SYSTEM_OPENSSL=OFF \
        -DWIN32=ON \
        -DCMAKE_SIZEOF_VOID_P=8 \
        -DENABLE_BUILD_WERROR=OFF \
        -DENABLE_STATIC_LINK_TO_GCC=OFF \
        -DUSE_HTTP=OFF \
        -DML64=OFF \
        -DENABLE_LLD=ON \
        "${SCRIPT_PATH}"
