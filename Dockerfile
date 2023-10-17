FROM debian:bookworm

RUN apt update -y && apt upgrade -y

RUN apt install -y clang locales-all ninja-build cmake pkgconf git file

# Build the bcm2835 library that we depend on
WORKDIR /build/bcm2835
ADD http://www.airspayce.com/mikem/bcm2835/bcm2835-1.73.tar.gz .
RUN tar -zvxf bcm2835-1.73.tar.gz && cd bcm2835-1.73 && ./configure && make -j 4 && make install




WORKDIR /build/creature-controller
COPY . .

WORKDIR /build/creature-controller
RUN mkdir build && cd build && cmake .. && make -j 4

