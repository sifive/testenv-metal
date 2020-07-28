FROM clang:v10.0.1
ENV CLANG10PATH=/usr/local/clang10
LABEL description="Build a LLVM/Clang 10 toolchain for RISC-V targets"
LABEL maintainer="Emmanuel Blot <emmanuel.blot@sifive.com"
RUN apk update
RUN apk add build-base ninja cmake file python3-dev libedit-dev swig
WORKDIR /toolchain/llvm/build
RUN cmake -G Ninja -Wno-dev \
   -DCMAKE_INSTALL_PREFIX=${CLANG10PATH} \
   -DCMAKE_BUILD_TYPE=Release \
   -DLLVM_ENABLE_PROJECTS="clang;clang-tools-extra;lld" \
   -DLLVM_ENABLE_SPHINX=False \
   -DLLVM_INCLUDE_TESTS=False \
   -DLLVM_TARGETS_TO_BUILD=RISCV \
   -DLLVM_INSTALL_UTILS=ON \
   -DLLVM_DEFAULT_TARGET_TRIPLE=riscv64-elf \
   -DCMAKE_CROSSCOMPILING=ON \
   -DLLVM_OPTIMIZED_TABLEGEN=ON \
   ../llvm
RUN ninja
RUN ninja install
RUN mkdir -p ${CLANG10PATH}/share/man/man1 ${CLANG10PATH}/share/man/man7
RUN cp ../lld/docs/ld.lld.1 ../llvm/docs/llvm-objdump.1 ${CLANG10PATH}/share/man/man1/
RUN cp ../llvm/docs/re_format.7 ${CLANG10PATH}/share/man/man7/
ENV PATH=${PATH}:${CLANG10PATH}/bin
RUN file ${CLANG10PATH}/bin/* | grep ELF | cut -d: -f1 | xargs strip
WORKDIR /


# there should be at least 2GB/RAM per core to build

# because LLVM C++ library build process needs the LLVM native .a libraries,
# we need a two-stage process:
# * build a full clang-riscv image required to build the toolchain, then
# * build a .a -stripped version of the clang-riscv image useful to build
#   target application, saving image storage footprint
# This dockerfile is dedicated to build the first, full and large one. This
# on is therefore never pushed to the docker hub, as it only lasts for the
# time required to build the toolchain itself

# docker build -f llvm-riscv-v10.0.1.docker -t llvm-riscv:a3.12-v10.0.1 .
