# Freedom Metal

![Status](https://github.com/sifive-eblot/freedom-metal/workflows/SCL-metal/badge.svg)

PoC to build freedom-metal and scl-metal w/ LLVM & CMake/Ninja

## Installation

The recommended way is to use a Docker environment.

To run the command natively (w/o Docker), remove `docker/bin/dock.sh` and the first argument from
the following example.

### Docker environment

The following command should pull in all the required containers and test that the LLVM toolchain
is available.

It is expected to take some time on the first run (about 1.8 GB to download).

````sh
docker/bin/dock.sh build clang --version
````

Subsequent runs should start immediately as the container material is preserved in Docker images.

## Building

1. One or more BSPs should be selected. The supported BSPs can listed as
    ````sh
    ls -1 bsp/
    ````

2. Build RV32 and RV64 QEMU BSPs
    ````sh
    docker/bin/dock.sh build scripts/buildall.sh qemu-sifive_e_rv32 qemu-sifive_e_rv64
    ````

    Alternatively, it is possible to build for a single build type and a single target, as in:
     ````sh
     docker/bin/dock.sh build scripts/build.sh debug qemu-sifive_e_rv32
     ````

    The output files are still available in the host directories, below `build/`.

3. Rebuild the project in development mode
    ````sh
    docker/bin/dock.sh build '(cd build/qemu-sifive_e_rv32/debug && ninja)'
    ````

## Testing

QEMU is used to run unit tests.

A special QEMU for RISC-V targets is required: one that support the `sifive_fdt` generic targets
that instantiates the whole virtual machine from a DTB file. It is available through a Docker
image, or can be rebuilt to run it natively on any host.

1. Tests all generated unit test executables
    ````sh
    docker/bin/dock.sh utest scripts/utestall.sh build/
    ````

2. Alternatively, it is possible to test a single build type of a single target, as in:
    ````sh
    docker/bin/dock.sh utest scripts/utest.sh -d bsp/qemu-sifive_e_rv64/dts/qemu.dts \
       build/qemu-sifive_e_rv32/debug
    ````

    Note that to run QEMU natively (w/o Docker), it is possible to use the `-e` option to specify
    the directory where QEMU-FDT resides.

## CI/CD/CT

A GitHub Actions [script](.github/workflows/build_test.yaml) is used to build and run unit tests
for several QEMU virtual targets.
