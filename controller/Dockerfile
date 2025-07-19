FROM debian:bookworm AS build

# Set up the build environment
RUN apt update -y && apt upgrade -y
RUN apt install -y gcc g++ locales-all libssl-dev ninja-build cmake git \
                   pkgconf libbson-dev libpthreadpool-dev libutf8proc-dev \
                   file zlib1g-dev libsdl2-mixer-dev libsdl2-dev

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
RUN cmake -DCMAKE_BUILD_TYPE=Release -B build -G Ninja

# Run the build in a different layer so we avoid grabbing the code over
# and over again
RUN cd /build/creature-controller/build && ninja


FROM debian:bookworm-slim AS runtime

RUN apt update -y && apt upgrade -y
RUN apt install -y locales-all file zlib1g libsasl2-2 libicu72 libsdl2-mixer-2.0-0 flac locales-all libutf8proc2 \
                   libuuid1 util-linux pipewire libuv1 libcurl4 libprotobuf32 && \
    rm -rf /var/lib/apt/lists

WORKDIR /app
COPY --from=build /build/creature-controller/build/creature-controller .

CMD ["/app/creature-controller"]


# Small build for creating a deb package
FROM build AS package

# Make a package
RUN mkdir -p /package
RUN cd /build/creature-controller/build/ && cpack -G DEB && cp *.deb /package
