FROM llvm-riscv:a3.12-v11.0.0-rc3 as source

FROM alpine:3.12
LABEL description="RISC-V toolchain"
LABEL maintainer="Emmanuel Blot <emmanuel.blot@sifive.com>"
ENV CLANG11PATH=/usr/local/clang11
WORKDIR ${CLANG11PATH}

COPY --from=source ${CLANG11PATH}/bin ${CLANG11PATH}/bin
COPY --from=source ${CLANG11PATH}/lib/*.so ${CLANG11PATH}/lib/
COPY --from=source ${CLANG11PATH}/lib/clang ${CLANG11PATH}/lib/clang
COPY --from=source ${CLANG11PATH}/lib/cmake ${CLANG11PATH}/lib/cmake
COPY --from=source ${CLANG11PATH}/libexec ${CLANG11PATH}/libexec
COPY --from=source ${CLANG11PATH}/share ${CLANG11PATH}/share
COPY --from=source ${CLANG11PATH}/include ${CLANG11PATH}/include
WORKDIR /

# because LLVM C++ library build process needs the LLVM native .a libraries,
# we need a two-stage process:
# * build a full clang-riscv image required to build the toolchain, then
# * build a .a -stripped version of the clang-riscv image useful to build
#   target application, saving image storage footprint
# This dockerfile is dedicated to build the second, enlightened one.

# docker build -f clang-riscv-v11.dockerfile -t clang-riscv:a3.12-v11.0.0-rc3 .
# docker tag clang-riscv:a3.12-v11.0.0-rc3 sifive/clang-riscv:a3.12-v11.0.0-rc3
