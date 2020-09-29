FROM clang:v11.0.0-rc3 as clang
FROM newlib:v3.3.0 as newlib

FROM llvm-riscv:a3.12-v11.0.0-rc3 as builder
RUN apk update
RUN apk add build-base ninja cmake git patch vim python3 curl
COPY --from=clang /toolchain/llvm /toolchain/llvm
COPY --from=newlib /toolchain/newlib /toolchain/newlib
WORKDIR /toolchain

ENV CLANG11PATH=/usr/local/clang11
ENV xlen=64
ENV xtarget="riscv${xlen}-unknown-elf"
ENV prefix=${CLANG11PATH}

RUN ln -s /usr/bin/python3 /usr/bin/python

ADD clang-riscv-v11.sh /
RUN sh /clang-riscv-v11.sh

WORKDIR /

FROM alpine:3.12
LABEL description="RISC-V 64-bit environment"
LABEL maintainer="Emmanuel Blot <emmanuel.blot@sifive.com>"
ENV CLANG11PATH=/usr/local/clang11
ENV xlen=64
ENV xtarget="riscv${xlen}-unknown-elf"
COPY --from=builder ${CLANG11PATH}/${xtarget} \
     ${CLANG11PATH}/${xtarget}
WORKDIR /

# docker build -f clang-riscv64-v11.dockerfile -t clang-riscv64:a3.12-v11.0.0-rc3 .
# docker tag clang-riscv64:a3.12-v11.0.0-rc3 sifive/clang-riscv64:a3.12-v11.0.0-rc3


