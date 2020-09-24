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

SCRIPT_DIR=$(dirname $0)
. ${SCRIPT_DIR}/funcs.sh

usage () {
    NAME=`basename $0`
    cat <<EOT
$NAME [-h] [-v] [-q] [debug|release|static_analysis] <bsp>

 bsp: the name of a BSP (see bsp/ directory)

 -h:  print this help
 -q:  quiet (do not report build steps)
 -v:  verbose (report all toolchain commands)
EOT
}

CMAKE_OPTS=""
NINJA_OPTS=""
SUBDIR=""
BUILD="DEBUG"
SA_DIR=""
GHA=0
QUIET=0
XBSP=""
for arg in $*; do
    case $arg in
        -h)
            usage
            ;;
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
        STATIC_ANALYSIS|static_analysis)
            CMAKE_OPTS="${CMAKE_OPTS} -DSTATIC_ANALYSIS=1"
            SA_DIR="sa_"
            ;;
        -*)
            ;;
        *)
            if [ -z "${XBSP}" ]; then
                XBSP="$arg"
            fi
    esac
done
test -n "${XBSP}" || die "XBSP should be specified"

CMAKE_OPTS="${CMAKE_OPTS} -DXBSP=${XBSP} -DCMAKE_BUILD_TYPE=${BUILD}"
SUBDIR=$(echo "${SA_DIR}${BUILD}" | tr [:upper:] [:lower:])

set -eu

rm -rf build/${XBSP}/${SUBDIR}
mkdir -p build/${XBSP}/${SUBDIR}
cd build/${XBSP}/${SUBDIR}
cmake -G Ninja ../../.. ${CMAKE_OPTS}
if [ ${QUIET} -eq 0 ]; then
    ninja ${NINJA_OPTS}
else
    # silent build mode, discard Ninja output
    # as Ninja redirect toolchain stderr to stdout, need to filter out
    # NINJA_STATUS prolog to get the compiler warnings and errors
    ninja ${NINJA_OPTS} | egrep -v '^\[' | \
        while read -r line; do
            if (echo "${line}" | grep -q 'warning:'); then
                warning -n "WARNING: "
            elif (echo "${line}" | grep -q 'error:'); then
                error -n "ERROR: "
            fi
            echo "${line}"
    done
fi
