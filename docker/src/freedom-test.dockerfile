#-------------------------------------------------------------------------------
# Build an small image base to perform unit test with a QEMU virtual machine
# Note that QEMU VM is not contained in this image
#-------------------------------------------------------------------------------
FROM alpine:3.12

ENV QEMUPATH=/usr/local/qemu-fdt
ENV PATH=$PATH:${QEMUPATH}/bin

WORKDIR /

LABEL description="Lightweigth test environment for QEMU"
LABEL maintainer="Emmanuel Blot <emmanuel.blot@sifive.com>"

RUN apk update
RUN apk add glib pixman libgcc dtc

# docker build -f freedom-test.dockerfile -t freedom-test:tmp .
# docker run --name freedom-test_tmp -it freedom-test:tmp /bin/sh -c "exit"
# docker export freedom-test_tmp | docker import - freedom-test:latest
# docker rm freedom-test_tmp
# docker rmi freedom-test:tmp
# docker tag freedom-test:a3.12-v1.0 sifive/freedom-test:a3.12-v1.0
