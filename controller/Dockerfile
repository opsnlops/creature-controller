FROM debian:bullseye AS build

RUN apt update -y && apt upgrade -y

RUN apt install -y clang locales-all ninja-build cmake pkgconf git file

WORKDIR /build/creature-controller
COPY . .

WORKDIR /build/creature-controller
RUN mkdir build && cd build && cmake .. && make -j 4


FROM debian:bullseye-slim AS runtime

RUN apt update -y && apt upgrade -y
RUN apt install -y locales-all file


WORKDIR /app
COPY --from=build /build/creature-controller/build/creature-controller .

CMD ["/bin/bash"]
