#!/bin/sh

apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    ca-certificates \
    cmake \
    g++ \
    git \
    libssl-dev \
    libpq-dev \
    ninja-build \
    tar \
    xz-utils

PQXX_VERSION="7.9.2"

git clone https://github.com/jtv/libpqxx.git -b ${PQXX_VERSION} /src

cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DSKIP_BUILD_TEST=ON -DBUILD_TEST=OFF \
    -S/src -B/build -G Ninja

cmake --build /build --config RelWithDebInfo --target all --parallel ${PARALLEL:-$(nproc)} --
cmake --install /build --prefix "/install"

tar -C /install -cavf /out/libpqxx-${PQXX_VERSION}-$(arch).tar.gz .
