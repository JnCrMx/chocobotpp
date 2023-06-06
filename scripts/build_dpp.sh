#!/bin/sh

apk add --no-cache git clang16 clang16-dev alpine-sdk ninja cmake zlib-dev zlib-static openssl-dev openssl-libs-static

DPP_VERSION="v10.0.24"

git clone https://github.com/brainboxdotcc/DPP.git -b ${DPP_VERSION} /src

cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
    -DBUILD_SHARED_LIBS=OFF -DDPP_BUILD_TEST=OFF \
    -S/src -B/build -G Ninja

cmake --build /build --config RelWithDebInfo --target all --parallel $(nproc) --
cmake --install /build --prefix "/install"

tar -C /install -cavf /out/dpp-${DPP_VERSION}-$(arch | sed s/x86_64/amd64/).tar.gz .
