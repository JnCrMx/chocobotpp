FROM docker.io/alpine:latest
RUN apk add --no-cache clang14 clang14-dev alpine-sdk ninja cmake git zlib-dev spdlog-dev openssl-dev postgresql14-dev

COPY . /src
RUN mkdir -p /build
RUN /usr/bin/cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
    -S/src -B/build -G Ninja
RUN /usr/bin/cmake --build /build -j --config RelWithDebInfo --target all --
RUN mkdir -p /src/cmake && ln -s /build/_deps/dpp-src/cmake/libdpp-config.cmake /src/cmake
RUN /usr/bin/cmake --install /build --prefix "/install"

FROM docker.io/alpine:latest
RUN apk add --no-cache libstdc++ postgresql14 spdlog
COPY --from=0 /install /usr
CMD ["chocobot"]