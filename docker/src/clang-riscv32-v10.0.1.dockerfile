FROM clang:v10.0.1 as clang
FROM newlib:v3.1.0 as newlib

FROM llvm-riscv:a3.12-v10.0.1 as builder
RUN apk update
RUN apk add build-base ninja cmake git patch vim python3 curl
COPY --from=clang /toolchain/llvm /toolchain/llvm
COPY --from=newlib /toolchain/newlib /toolchain/newlib
WORKDIR /toolchain

ENV CLANG10PATH=/usr/local/clang10
ENV xlen=32
ENV xtarget="riscv${xlen}-unknown-elf"
ENV prefix=${CLANG10PATH}

RUN ln -s /usr/bin/python3 /usr/bin/python

ADD clang-riscv-v10.sh /
RUN sh /clang-riscv-v10.sh

WORKDIR /

FROM alpine:3.12
LABEL description="RISC-V 32-bit environment"
LABEL maintainer="Emmanuel Blot <emmanuel.blot@sifive.com>"
ENV CLANG10PATH=/usr/local/clang10
ENV xtarget="riscv64-unknown-elf"
ENV xcpudir=cortex-m4f
COPY --from=builder ${CLANG10PATH}/${xtarget} \
     ${CLANG10PATH}/${xtarget}
WORKDIR /

# docker build -f clang-riscv32-v10.0.1.dockerfile -t clang-riscv32:a3.12-v10.0.1 .
# docker tag clang-riscv32:a3.12-v10.0.1 iroazh/clang-riscv32:a3.12-v10.0.1


