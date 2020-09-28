# Test environment to build SCL metal and exercise SCL metal with several HW configurations

![Status](https://github.com/sifive-eblot/freedom-metal/workflows/SCL-metal/badge.svg)

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

For now, freedom-metal and scl-metal are build with

* LLVM/clang v10
* CMake
* Ninja

1. One or more BSPs should be selected. The supported BSPs can listed as
    ````sh
    ls -1 bsp/
    ````

2. Build RV32 and RV64 QEMU BSPs
    ````sh
    docker/bin/dock.sh build scripts/buildall.shm-g -r qemu-sifive_e_rv32 qemu-sifive_e_rv64
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

[Unity](https://github.com/ThrowTheSwitch/Unity) test framework is used for running unit test
sessions.

[QEMU](https://www.qemu.org) is used to run unit tests.

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

A GitHub Actions [script](.github/workflows/build_test.yml) is used to build and run unit tests
for several QEMU virtual targets.

## Directory tree structure

```text
.
├── CMakeLists.txt   # Top level CMake file to build the project
├── bsp              # BSP directory, with DTS file and BSP-specific header files
├── cmake            # CMake configuration and macros
│   ├── Platform     #   RISC-V platform definition
│   ├── files        #   CMakeFiles.txt copied to the existing metal and scl-metal directories*
│   ├── macros.cmake #   Useful CMake macros to help buildint the project
│   └── riscv.cmake  #   RISV-C toolchain configuration
├── docker           # All docker files
│   ├── bin          #   Docker script to run build and test scripts from Docker containers
│   ├── conf         #   Docker configuration, define versionned image for each tasks
│   └── src          #   Dockerfiles to build the images, the toolchains, ...
├── metal            # freedom-metal framework
├── scl-metal        # scl-metal submodule
├── scripts          # Useful scripts to automate tasks
│   ├── build.sh     # Script to build the project for a single BSP
│   ├── buildall.sh  # Script to build all the BSPs in all possible configurations
│   ├── funcs.sh     # Common functions used by other scripts
│   ├── utest.sh     # Script to execute the unit test of a single BSP
│   └── utestall.sh  # Script to execute all built unit tests
├── tests            # Unit test files (SCL-metal tests and QEMU tests)
│   ├── hello        # Simplest test to check an application may be executed
│   ├── qemu         # HCA tests which are not yet part of SCL
│   └── scl-metal    # test-scl-metal submodule
└── unity            # Unit test framework
```

`*`: as this project is forked from the official `freedom-metal` and `scl-metal` repositories,
which do not yet use CMake (nor Ninja) to build binaries, a couple of `CMakeFiles.txt` files are
maintained out-of-tree and copied into the existing directories when CMake is invoked. It should
ease maintenance and integration. `CMakeFiles.txt` files should not be committed into the
sub-module repositories.

## Miscellaneous

### Docker

Docker images are built on top of Alpine Linux, as it is the smallest distribution, and enables to
build the smallest image files.

### Shell scripts

All shell scripts should be POSIX-compliant, *i.e.* should not use bash-isms.
