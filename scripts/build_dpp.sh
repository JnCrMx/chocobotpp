#!/bin/sh

apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    ca-certificates \
    cmake \
    g++ \
    git \
    libssl-dev \
    ninja-build \
    tar \
    xz-utils \
    zlib1g-dev

DPP_VERSION="v10.0.26"

git clone https://github.com/brainboxdotcc/DPP.git -b ${DPP_VERSION} /src

cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DBUILD_SHARED_LIBS=OFF -DDPP_BUILD_TEST=OFF -DDPP_CORO=ON \
    -S/src -B/build -G Ninja

cmake --build /build --config RelWithDebInfo --target all --parallel $(nproc) --
cmake --install /build --prefix "/install"

tar -C /install -cavf /out/dpp-${DPP_VERSION}-$(arch).tar.gz .
