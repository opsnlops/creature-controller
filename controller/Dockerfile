FROM debian:trixie AS build

# Set up the build environment. Keep the package index refresh in the same
# layer as installation so a cached Debian base never uses stale indexes.
RUN apt update -y && apt upgrade -y && \
    apt install -y clang locales-all libssl-dev ninja-build cmake git \
                   pkgconf libbson-dev libpthreadpool-dev libutf8proc-dev \
                   file zlib1g-dev libasound2-dev libcurl4-openssl-dev && \
    rm -rf /var/lib/apt/lists/*

# Copy the source code into the build directory
COPY src/ /build/creature-controller/src
COPY cmake/ /build/creature-controller/cmake
COPY lib/ /build/creature-controller/lib
COPY tests/ /build/creature-controller/tests
COPY CMakeLists.txt /build/creature-controller/
COPY LICENSE /build/creature-controller/LICENSE
COPY README.md /build/creature-controller/README.md

# Create the build directory and run the build
WORKDIR /build/creature-controller
RUN cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -B build -G Ninja

# Run the build in a different layer so we avoid grabbing the code over
# and over again
RUN cd /build/creature-controller/build && ninja -j$(nproc)

# Run tests (exclude device-dependent serial tests that need real hardware)
RUN cd /build/creature-controller/build && ctest --output-on-failure -E SerialOutput


FROM debian:trixie-slim AS runtime

RUN apt update -y && apt upgrade -y && \
    apt install -y locales-all file zlib1g libsasl2-2 libicu76 libutf8proc3 libasound2t64 libuuid1 util-linux \
                   libuv1 libcurl4 libprotobuf32t64 && \
    rm -rf /var/lib/apt/lists

WORKDIR /app
COPY --from=build /build/creature-controller/build/creature-controller .

CMD ["/app/creature-controller"]


# Small build for creating a deb package
FROM build AS package

# Make a package
RUN mkdir -p /package
RUN cd /build/creature-controller/build/ && cpack -G DEB && cp *.deb /package
