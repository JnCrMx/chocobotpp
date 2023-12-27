FROM docker.io/ubuntu:mantic AS builder

RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    ca-certificates \
    cmake \
    curl \
    g++ \
    git \
    libmagick++-dev \
    libpq-dev \
    libspdlog-dev \
    libssl-dev \
    ninja-build \
    && rm -rf /var/lib/apt/lists/*

ARG DPP_VERSION=v10.0.26
ARG PQXX_VERSION=7.7.5

RUN mkdir -p /third_party/dpp/install && curl https://files.jcm.re/dpp-${DPP_VERSION}-$(arch).tar.gz | tar -C /third_party/dpp/install -xz
RUN mkdir -p /third_party/pqxx/install && curl https://files.jcm.re/libpqxx-${PQXX_VERSION}-$(arch).tar.gz | tar -C /third_party/pqxx/install -xz

COPY . /src
RUN mkdir -p /build && \
    /usr/bin/cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_CXX_FLAGS_RELWITHDEBINFO="-O2" \
    -DUSE_INSTALLED_DPP=ON -DUSE_INSTALLED_PQXX=ON \
    -DCMAKE_SYSTEM_PREFIX_PATH="/third_party/dpp/install;/third_party/pqxx/install" \
    -S/src -B/build -G Ninja
RUN /usr/bin/cmake --build /build --config RelWithDebInfo --target all --parallel $(($(nproc) / 2)) --
RUN mkdir -p /src/cmake && ln -s /build/_deps/dpp-src/cmake/dpp-config.cmake /src/cmake \
    && /usr/bin/cmake --install /build --prefix "/install"

FROM docker.io/ubuntu:mantic AS runtime
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    ca-certificates \
    curl \
    fortunes \
    gsfonts \
    libmagick++-6.q16-8 \
    libspdlog1.10 \
    libssl3 \
    libpq5 \
    tzdata \
    && rm -rf /var/lib/apt/lists/*
COPY --from=builder /install /usr
CMD ["chocobot"]
