FROM alpine:3.12
LABEL description="Store Git repository for LLVM/Clang 11 toolchain"
LABEL maintainer="Emmanuel Blot <emmanuel.blot@sifive.com>"
RUN apk update
RUN apk add curl
WORKDIR /toolchain
RUN curl -LO https://github.com/llvm/llvm-project/archive/llvmorg-11.0.0-rc3.tar.gz
RUN [ "82ce06e7c2b6a688dd0aa8f0aaa20f44850b1e692cf6c59bf7eebdc28440abea" = \
      "$(sha256sum llvm-project-11.0.0-rc3.tar.xz | cut -d' ' -f1)" ] && \
    tar xf llvm-project-11.0.0-rc3.tar.xz && \
    mv llvm-project-11.0.0-rc3 llvm && rm llvm-project-11.0.0-rc3.tar.xz
WORKDIR /

# docker build -f clang-v11.0.0.dockerfile -t clang:v11.0.0-rc3 .
