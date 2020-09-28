FROM alpine:3.12 as builder
LABEL description="Build binutils for RISC-V targets"
LABEL maintainer="Emmanuel Blot <emmanuel.blot@sifive.com>"
RUN apk update
RUN apk add build-base gmp-dev mpfr-dev file curl
WORKDIR /toolchain
RUN curl -LO https://ftp.gnu.org/gnu/binutils/binutils-2.35.tar.xz && \
   [ "1b11659fb49e20e18db460d44485f09442c8c56d5df165de9461eb09c8302f85" = \
      "$(sha256sum binutils-2.35.tar.xz | cut -d' ' -f1)" ] && \
      tar xvf binutils-2.35.tar.xz
RUN mkdir /toolchain/build
WORKDIR /toolchain/build
RUN ../binutils-2.35/configure \
    --prefix=/usr/local/riscv-elf-binutils \
    --target=riscv64-unknown-elf \
    --disable-shared \
    --disable-nls \
    --with-gmp \
    --with-mpfr \
    --disable-cloog-version-check \
    --enable-multilibs \
    --enable-interwork \
    --enable-lto \
    --disable-werror \
    --disable-debug
RUN make
RUN make install
WORKDIR /

FROM alpine:3.12
LABEL description="RISC-V binutils"
LABEL maintainer="Emmanuel Blot <emmanuel.blot@sifive.com>"
COPY --from=builder /usr/local/riscv-elf-binutils /usr/local/riscv-elf-binutils
ENV PATH=$PATH:/usr/local/riscv-elf-binutils/bin
WORKDIR /

# docker build -f host/docker/src/binutils-riscv-v2.35.dockerfile -t binutils-riscv:a3.12-v2.35 .
# docker tag binutils-riscv:a3.12-v2.35 iroazh/binutils-riscv:a3.12-v2.35
