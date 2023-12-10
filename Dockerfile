FROM docker.io/alpine:3.18 AS builder
RUN apk add --no-cache clang16 clang16-dev alpine-sdk ninja cmake git zlib-dev zlib-static spdlog-dev openssl-dev openssl-libs-static postgresql14-dev imagemagick-dev boost1.82-dev curl

ARG DPP_VERSION=v10.0.26
ARG PQXX_VERSION=7.7.5

RUN mkdir -p /third_party/dpp/install
RUN curl https://files.jcm.re/dpp-${DPP_VERSION}-$(arch).tar.gz | tar -C /third_party/dpp/install -xz

RUN mkdir -p /third_party/pqxx/install
RUN curl https://files.jcm.re/libpqxx-${PQXX_VERSION}-$(arch).tar.gz | tar -C /third_party/pqxx/install -xz

COPY . /src
RUN mkdir -p /build
RUN /usr/bin/cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_CXX_FLAGS_RELWITHDEBINFO="-O2" \
    -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
    -DUSE_INSTALLED_DPP=ON -DUSE_INSTALLED_PQXX=ON \
    -DCMAKE_SYSTEM_PREFIX_PATH="/third_party/dpp/install;/third_party/pqxx/install" \
    -S/src -B/build -G Ninja
RUN /usr/bin/cmake --build /build --config RelWithDebInfo --target all --parallel $(($(nproc) / 2)) --
RUN mkdir -p /src/cmake && ln -s /build/_deps/dpp-src/cmake/dpp-config.cmake /src/cmake
RUN /usr/bin/cmake --install /build --prefix "/install"

FROM docker.io/alpine:3.18 AS runtime
RUN apk add --no-cache libstdc++ postgresql14 spdlog imagemagick imagemagick-c++ boost1.82 fortune font-noto
COPY --from=builder /install /usr
CMD ["chocobot"]
