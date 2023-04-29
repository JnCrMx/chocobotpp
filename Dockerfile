FROM docker.io/alpine:3.17.3
RUN apk add --no-cache clang14 clang14-dev alpine-sdk ninja cmake git zlib-dev zlib-static spdlog-dev openssl-dev openssl-libs-static postgresql14-dev imagemagick-dev

ARG DPP_VERSION=v10.0.23
ARG PQXX_VERSION=7.7.4

RUN mkdir -p /third_party/dpp/src
ADD https://github.com/brainboxdotcc/DPP/archive/refs/tags/${DPP_VERSION}.tar.gz /third_party/dpp.tar.gz
RUN tar -C /third_party/dpp/src -xaf /third_party/dpp.tar.gz --strip-components=1 && \
    /usr/bin/cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
    -DBUILD_SHARED_LIBS=OFF -DDPP_BUILD_TEST=OFF \
    -S/third_party/dpp/src -B/third_party/dpp/build -G Ninja && \
    /usr/bin/cmake --build /third_party/dpp/build --config RelWithDebInfo --target all --parallel $(($(nproc) / 2)) -- && \
    /usr/bin/cmake --install /third_party/dpp/build --prefix "/third_party/dpp/install"

RUN mkdir -p /third_party/pqxx/src
ADD https://github.com/jtv/libpqxx/archive/refs/tags/${PQXX_VERSION}.tar.gz /third_party/pqxx.tar.gz
RUN tar -C /third_party/pqxx/src -xaf /third_party/pqxx.tar.gz --strip-components=1 && \
    /usr/bin/cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
    -DSKIP_BUILD_TEST=ON -DBUILD_TEST=OFF \
    -S/third_party/pqxx/src -B/third_party/pqxx/build -G Ninja && \
    /usr/bin/cmake --build /third_party/pqxx/build --config RelWithDebInfo --target all --parallel $(($(nproc) / 2)) -- && \
    /usr/bin/cmake --install /third_party/pqxx/build --prefix "/third_party/pqxx/install"

COPY . /src
RUN mkdir -p /build
RUN /usr/bin/cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
    -DUSE_INSTALLED_DPP=ON -DUSE_INSTALLED_PQXX=ON \
    -DCMAKE_SYSTEM_PREFIX_PATH="/third_party/dpp/install;/third_party/pqxx/install" \
    -S/src -B/build -G Ninja
RUN /usr/bin/cmake --build /build --config RelWithDebInfo --target all --parallel $(($(nproc) / 2)) --
RUN mkdir -p /src/cmake && ln -s /build/_deps/dpp-src/cmake/dpp-config.cmake /src/cmake
RUN /usr/bin/cmake --install /build --prefix "/install"

FROM docker.io/alpine:3.17.3
RUN apk add --no-cache libstdc++ postgresql14 spdlog imagemagick imagemagick-c++
COPY --from=0 /install /usr
CMD ["chocobot"]
