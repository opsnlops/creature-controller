FROM debian:trixie AS build

# Set up the build environment. Keep the package index refresh in the same
# layer as installation so a cached Debian base never uses stale indexes.
RUN apt update -y && apt upgrade -y && \
    apt install -y clang locales-all libssl-dev ninja-build cmake git \
                   dpkg-dev pkgconf help2man \
                   file zlib1g-dev libasound2-dev libcurl4-openssl-dev libavahi-client-dev && \
    rm -rf /var/lib/apt/lists/*

# Copy the source code into the build directory
COPY src/ /build/creature-controller/src
COPY cmake/ /build/creature-controller/cmake
COPY lib/ /build/creature-controller/lib
COPY config/ /build/creature-controller/config
COPY packaging/ /build/creature-controller/packaging
COPY docs/ /build/creature-controller/docs
COPY tests/ /build/creature-controller/tests
COPY CMakeLists.txt /build/creature-controller/
COPY LICENSE /build/creature-controller/LICENSE
COPY README.md /build/creature-controller/README.md

# The creature configurations live outside this build context
COPY --from=creature-configs . /build/config

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
    apt install -y locales-all file zlib1g libasound2t64 util-linux libcurl4 libavahi-client3 && \
    rm -rf /var/lib/apt/lists

WORKDIR /app
COPY --from=build /build/creature-controller/build/creature-controller .

CMD ["/app/creature-controller"]


# Small build for creating a deb package
FROM build AS packages

# Make a package
RUN mkdir -p /package
RUN cd /build/creature-controller/build/ && cpack -G DEB && cpack --config CPackAudioRouterConfig.cmake -G DEB && cp *.deb /package

# Validate the router in a clean Debian installation before exporting packages.
FROM debian:trixie-slim AS audio-router-package-test
COPY --from=packages /package/creature-audio-router_*.deb /tmp/creature-audio-router.deb
COPY packaging/audio-router/check-package.sh /tmp/check-package.sh
RUN /tmp/check-package.sh /tmp/creature-audio-router.deb && touch /tmp/package-validated

# Validate the controller the same way.
FROM debian:trixie-slim AS controller-package-test
COPY --from=packages /package/creature-controller_*.deb /tmp/creature-controller.deb
COPY packaging/controller/check-package.sh /tmp/check-package.sh
RUN /tmp/check-package.sh /tmp/creature-controller.deb && touch /tmp/package-validated

FROM packages AS package
COPY --from=audio-router-package-test /tmp/package-validated /package/.router-validated
COPY --from=controller-package-test /tmp/package-validated /package/.controller-validated
