#!/bin/sh
#------------------------------------------------------------------------------
# Build freedom-metal unit tests
#
# Dependencies:
#  * LLVM/clang toolchain v10 supporting RISC-V
#  * binutils for RISC-V
#  * cmake 3.5+
#  * ninja
#------------------------------------------------------------------------------

# Die with an error message
die() {
    echo "$*" >&2
    exit 1
}

test -n "${XBSP:=$1}" || die "XBSP should be specified"

CMAKE_OPTS="-DXBSP=${XBSP}"
NINJA_OPTS=""
for arg in $*; do
    case $arg in
        *DEBUG|*RELEASE)
            BUILD=$2
            ;;
        STATIC_ANALYSIS)
            CMAKE_OPTS="${CMAKE_OPTS} -DSTATIC_ANALYSIS=1"
            ;;
        -v|VERBOSE)
            NINJA_OPTS="${NINJA_OPTS} -v"
            ;;
    esac
done
CMAKE_OPTS="${CMAKE_OPTS} -DCMAKE_BUILD_TYPE=${BUILD:-DEBUG}"

set -eu

rm -rf build/${XBSP}
mkdir -p build/${XBSP}
cd build/${XBSP}
cmake -G Ninja ../.. ${CMAKE_OPTS}
ninja ${NINJA_OPTS}
