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
$NAME [-h] [-v] [-g] [debug|release|static_analysis] <bsp>

 bsp: the name of a BSP (see bsp/ directory)

 -h:  print this help
 -g:  github mode (filter compiler output)
 -v:  verbose (report all toolchain commands)
EOT
}

filter_issues () {
   egrep -v '^\[' | \
        while read -r line; do
            if (echo "${line}" | grep -q 'warning:'); then
                warning -n "WARNING: "
            elif (echo "${line}" | grep -q 'error:'); then
                error -n "ERROR: "
            fi
            echo "${line}"
        done
}

CMAKE_OPTS=""
NINJA_OPTS=""
SUBDIR=""
BUILD="DEBUG"
SA_DIR=""
GHA=0
XBSP=""
for arg in $*; do
    case $arg in
        -h)
            usage
            ;;
        -g)
            GHA=1
            ;;
        -v|VERBOSE)
            NINJA_OPTS="${NINJA_OPTS} -v"
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
if [ ${GHA} -eq 0 ]; then
    ninja ${NINJA_OPTS}
else
    # silent build mode, discard Ninja output
    # as Ninja redirects toolchain stderr to stdout, need to filter out
    # NINJA_STATUS prolog to get the compiler warnings and errors

    # now use POSIX shell black magic to preserve Ninja exit status while
    # filtering its standard output
    { { { { ninja ${NINJA_OPTS}; echo $? >&3; } | filter_issues >&4; } 3>&1; } \
        | (read xs; exit $xs); } 4>&1
fi
