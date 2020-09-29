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
$NAME [-h] [-v] [-g] [-r report] [debug|release|static_analysis] <bsp>

 bsp: the name of a BSP (see bsp/ directory)

 -h:  print this help
 -g:  github mode (filter compiler output)
 -r:  copy all warnings and errors messages into a log file
 -v:  verbose (report all toolchain commands)
EOT
}

filter_issues () {
    if [ -n "${SA_DIR}" ]; then
        # static analyser does not report a warning source, so fake one to
        # report the warning kind
        epilog=" [-Wstatic_analysis]"
    else
        epilog=""
    fi
    egrep -v '^\[' | \
        while read -r line; do
            if (echo "${line}" | grep -q 'warning:'); then
                warning -n "WARNING: "
                if [ -n "${REPORTLOG}" ]; then
                    echo "${line}${epilog}" >> "${REPORTLOG}";
                fi
            elif (echo "${line}" | grep -q 'error:'); then
                error -n "ERROR: "
                if [ -n "${REPORTLOG}" ]; then
                    echo "${line}" >> "${REPORTLOG}";
                fi
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
REPORTLOG=""
XBSP=""
while [ $# -gt 0 ]; do
    case "$1" in
        -h)
            usage
            ;;
        -g)
            GHA=1
            ;;
        -r)
            shift
            REPORTLOG="$1"
            ;;
        -v|VERBOSE)
            NINJA_OPTS="${NINJA_OPTS} -v"
            ;;
        *DEBUG|*RELEASE|*debug|*release)
            BUILD=$(echo "$1" | tr [:lower:] [:upper:])
            ;;
        STATIC_ANALYSIS|static_analysis)
            CMAKE_OPTS="${CMAKE_OPTS} -DSTATIC_ANALYSIS=1"
            SA_DIR="sa_"
            ;;
        -*)
            ;;
        *)
            if [ -z "${XBSP}" ]; then
                XBSP="$1"
            fi
    esac
    shift
done
test -n "${XBSP}" || die "XBSP should be specified"

CMAKE_OPTS="${CMAKE_OPTS} -DXBSP=${XBSP} -DCMAKE_BUILD_TYPE=${BUILD}"
SUBDIR=$(echo "${SA_DIR}${BUILD}" | tr [:upper:] [:lower:])

rm -rf build/${XBSP}/${SUBDIR}
mkdir -p build/${XBSP}/${SUBDIR} || die "Cannot create build dir"
cd build/${XBSP}/${SUBDIR} || die "Invalid build dir"
cmake -G Ninja ../../.. ${CMAKE_OPTS} || die "Unable to run cmake"
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
