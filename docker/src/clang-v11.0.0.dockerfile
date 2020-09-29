FROM alpine:3.12
LABEL description="Store Git repository for LLVM/Clang 11 toolchain"
LABEL maintainer="Emmanuel Blot <emmanuel.blot@sifive.com>"
RUN apk update
RUN apk add curl
WORKDIR /toolchain
RUN curl -LO https://github.com/llvm/llvm-project/releases/download/llvmorg-11.0.0-rc3/llvm-project-11.0.0rc3.tar.xz
RUN [ "e6fcece3f9f74d37974a2766f45160e8debd11dc5d57e6547dcaf5e75b4db783" = \
      "$(sha256sum llvm-project-11.0.0rc3.tar.xz | cut -d' ' -f1)" ] && \
    tar xf llvm-project-11.0.0rc3.tar.xz && \
    mv llvm-project-11.0.0rc3 llvm && rm llvm-project-11.0.0rc3.tar.xz
WORKDIR /

# docker build -f clang-v11.dockerfile -t clang:v11.0.0-rc3 .
