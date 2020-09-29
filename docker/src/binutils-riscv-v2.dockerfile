FROM alpine:3.12 as builder
LABEL description="Build binutils for RISC-V targets"
LABEL maintainer="Emmanuel Blot <emmanuel.blot@sifive.com>"
RUN apk update
RUN apk add build-base gmp-dev mpfr-dev file curl
WORKDIR /toolchain
RUN curl -LO https://ftp.gnu.org/gnu/binutils/binutils-2.35.1.tar.xz
RUN [ "3ced91db9bf01182b7e420eab68039f2083aed0a214c0424e257eae3ddee8607" = \
      "$(sha256sum binutils-2.35.1.tar.xz | cut -d' ' -f1)" ] && \
      tar xvf binutils-2.35.1.tar.xz
RUN mkdir /toolchain/build
WORKDIR /toolchain/build
RUN ../binutils-2.35.1/configure \
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

# docker build -f binutils-riscv-v2.dockerfile -t binutils-riscv:a3.12-v2.35.1 .
# docker tag binutils-riscv:a3.12-v2.35.1 sifive/binutils-riscv:a3.12-v2.35.1
