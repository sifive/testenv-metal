FROM llvm-riscv:a3.12-v10.0.1 as source

FROM alpine:3.12
LABEL description="RISC-V toolchain"
LABEL maintainer="Emmanuel Blot <emmanuel.blot@sifive.com>"
ENV CLANG10PATH=/usr/local/clang10
WORKDIR ${CLANG10PATH}

COPY --from=source ${CLANG10PATH}/bin ${CLANG10PATH}/bin
COPY --from=source ${CLANG10PATH}/lib/*.so ${CLANG10PATH}/lib/
COPY --from=source ${CLANG10PATH}/lib/clang ${CLANG10PATH}/lib/clang
COPY --from=source ${CLANG10PATH}/lib/cmake ${CLANG10PATH}/lib/cmake
COPY --from=source ${CLANG10PATH}/libexec ${CLANG10PATH}/libexec
COPY --from=source ${CLANG10PATH}/share ${CLANG10PATH}/share
COPY --from=source ${CLANG10PATH}/include ${CLANG10PATH}/include
WORKDIR /

# because LLVM C++ library build process needs the LLVM native .a libraries,
# we need a two-stage process:
# * build a full clang-riscv image required to build the toolchain, then
# * build a .a -stripped version of the clang-riscv image useful to build
#   target application, saving image storage footprint
# This dockerfile is dedicated to build the second, enlightened one.

# docker build -f clang-riscv-v10.0.1.dockerfile -t clang-riscv:a3.12-v10.0.1 .
# docker tag clang-riscv:a3.12-v10.0.1 ${DOCKERHUB_USER}/clang-riscv:a3.12-v10.0.1
