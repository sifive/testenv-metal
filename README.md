# Freedom Metal

![Status](https://github.com/<owner>/<repo>/workflows/build_test/badge.svg)

PoC to build w/ LLVM & CMake/Ninja

## Installation

The recommended way is to use a Docker environment.

### Docker environment

The following command should pull in all the required containers and test that
the LLVM toolchain is available.

It is expected to take some time on the first run (about 1.8 GB to download).

````sh
docker/bin/dock.sh clang --version
````

Subsequent runs should start immediately as the container material is preserved
in Docker images.

## Building

1. Start a shell in a Docker interactive session
    ````sh
    docker/bin/dock.sh /bin/sh
    ````

2. Create a build directory
    ````sh
    mkdir -p build
    cd build
    ````
    This build directory is created in the host environement, as the whole
    project directory is mapped from the host.

3. Select a BSP from the BSPs directory
    ````sh
    ls -1 ../bsp
    ````

4. Run CMake to create the build environment
    ````sh
    cmake -G Ninja -DXBSP=qemu-sifive_e_rv64 [-DCMAKE_BUILD_TYPE=DEBUG] ..
    ````

    Note: build type is optional, either `DEBUG` or `RELEASE`.

5. Build the project
    ````sh
    ninja
    ````
    Use `-v` option if you want to see the compiler command lines

    Expect some warnings in SCL-metal tests for now

6. Exit the Docker shell (if/when required)
    ````sh
    exit
    ````

    The output files are still available in the directory that has been created
   from the Docker session.
