# Freedom Metal

Quick and dirty project to build w/ LLVM & CMake/Ninja

## Installation

The recommended way is to use a Docker environment.

### Docker environment

The following command should pull in all the required containers and test that
the LLVM toolchain is available.

It is expected to take some time on the first run.

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
    mkdir build
    cd build
    ````
    Note that this build directory is created in the host environement, as
   the whole project directory is mapped from the host.

3. Run CMake to create the build environment
    ````sh
    cmake -G Ninja ..
    ````

4. Build the project
    ````sh
    ninja
    ````
    Use `-v` option if you want to see the compiler command lines

5. Exit the Docker shell (if required)

    The output files are still available in the directory that has been created
   from the Docker session.
