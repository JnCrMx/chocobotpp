FROM manjarolinux/base:latest
RUN pacman -Sy --needed --noconfirm clang ninja cmake git zlib spdlog postgresql-libs

COPY . /src
RUN mkdir -p /build
RUN /usr/bin/cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_C_COMPILER=/usr/bin/clang -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -S/src -B/build -G Ninja
RUN /usr/bin/cmake --build /build --config RelWithDebInfo --target all --
RUN mkdir -p /src/cmake && ln -s /build/_deps/dpp-src/cmake/libdpp-config.cmake /src/cmake
RUN /usr/bin/cmake --install /build --prefix "/install"

FROM manjarolinux/base:latest
RUN pacman -Sy --needed --noconfirm spdlog postgresql-libs
COPY --from=0 /install /usr
