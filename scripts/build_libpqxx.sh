#!/bin/sh

apk add --no-cache git clang16 clang16-dev alpine-sdk ninja cmake postgresql14-dev

PQXX_VERSION=7.7.5

git clone https://github.com/jtv/libpqxx.git -b ${PQXX_VERSION} /src

cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
    -DSKIP_BUILD_TEST=ON -DBUILD_TEST=OFF \
    -S/src -B/build -G Ninja

cmake --build /build --config RelWithDebInfo --target all --parallel $(nproc) --
cmake --install /build --prefix "/install"

tar -C /install -cavf /out/libpqxx-${PQXX_VERSION}-$(arch).tar.gz .
