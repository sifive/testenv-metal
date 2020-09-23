#-------------------------------------------------------------------------------
# Build an small image base for compiling/building bare metal target software
# Note that toolchain is not contained in this image
#-------------------------------------------------------------------------------
FROM alpine:3.12

ENV CLANG10PATH=/usr/local/clang10
ENV BU235PATH=/usr/local/riscv-elf-binutils
ENV PATH=$PATH:${CLANG10PATH}/bin:${BU235PATH}/bin

WORKDIR /

LABEL description="Light development environment"
LABEL maintainer="Emmanuel Blot <emmanuel.blot@sifive.com>"

RUN apk update
RUN apk add ninja cmake git curl python3

# docker build -f freedom-dev.dockerfile -t freedom-dev:tmp .
# docker run --name freedom-dev_tmp -it freedom-dev:tmp /bin/sh -c "exit"
# docker export freedom-dev_tmp | docker import - freedom-dev:latest
# docker rm freedom-dev_tmp
# docker rmi freedom-dev:tmp
