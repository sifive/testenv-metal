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
SUBDIR=""
BUILD="DEBUG"
SA_DIR=""
GHA=0
QUIET=0
for arg in $*; do
    case $arg in
        -q)
            QUIET=1
            ;;
        -v|VERBOSE)
            NINJA_OPTS="${NINJA_OPTS} -v"
            QUIET=0
            ;;
        *DEBUG|*RELEASE|*debug|*release)
            BUILD=$(echo "${arg}" | tr [:upper:] [:lower:])
            ;;
        STATIC_ANALYSIS)
            CMAKE_OPTS="${CMAKE_OPTS} -DSTATIC_ANALYSIS=1"
            SA_DIR="sa_"
            ;;
    esac
done
CMAKE_OPTS="${CMAKE_OPTS} -DCMAKE_BUILD_TYPE=${BUILD}"
SUBDIR=$(echo "${SA_DIR}${BUILD}" | tr [:upper:] [:lower:])

set -eu

rm -rf build/${XBSP}/${SUBDIR}
mkdir -p build/${XBSP}/${SUBDIR}
cd build/${XBSP}/${SUBDIR}
cmake -G Ninja ../../.. ${CMAKE_OPTS}
if [ ${QUIET} -eq 0 ]; then
    ninja ${NINJA_OPTS}
else
    # silent build mode
    ninja ${NINJA_OPTS} >/dev/null
fi
