FROM alpine:3.12
LABEL description="Store Git repository for newlib & C runtime libraries"
LABEL maintainer="Emmanuel Blot <emmanuel.blot@free.fr>"
RUN apk update
RUN apk add curl
WORKDIR /toolchain
RUN curl -LO ftp://sourceware.org/pub/newlib/newlib-3.3.0.tar.gz && \
     [ "58dd9e3eaedf519360d92d84205c3deef0b3fc286685d1c562e245914ef72c66" = \
       "$(sha256sum newlib-3.3.0.tar.gz | cut -d' ' -f1)" ] && \
     tar xf newlib-3.3.0.tar.gz && \
     mv newlib-3.3.0 newlib
WORKDIR /

# docker build -f host/docker/src/newlib-v3.docker -t newlib:v3.1.0 .
