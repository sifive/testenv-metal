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
$NAME [-h] [-a] [-C] [-g] [-r report] [-v] [debug|release|static_analysis] <bsp>

 bsp: the name of a BSP (see bsp/ directory)

 -h:  print this help
 -a:  build all targets, ignore optional property
 -C:  do not clean existing build directory
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

# generate a JSON file with all build commands so that an IDE knows how to build
CMAKE_OPTS="-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"
NINJA_OPTS=""
SUBDIR=""
BUILD="DEBUG"
SA_DIR=""
GHA=0
REPORTLOG=""
XBSP=""
CLEAN=1
FORCEALL=0
while [ $# -gt 0 ]; do
    case "$1" in
        -h)
            usage
            exit 0
            ;;
        -a)
            FORCEALL=1
            CMAKE_OPTS="${CMAKE_OPTS} -DBUILD_OPTIONAL_TARGETS=1"
            ;;
        -C)
            CLEAN=0
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
            ;;
    esac
    shift
done
test -n "${XBSP}" || die "XBSP should be specified"

CMAKE_OPTS="${CMAKE_OPTS} -DXBSP=${XBSP} -DCMAKE_BUILD_TYPE=${BUILD}"
SUBDIR=$(echo "${SA_DIR}${BUILD}" | tr [:upper:] [:lower:])

if [ ${CLEAN} -ne 0 ]; then
    rm -rf build/${XBSP}/${SUBDIR}
fi
mkdir -p build/${XBSP}/${SUBDIR} || die "Cannot create build dir"
cd build/${XBSP}/${SUBDIR} || die "Invalid build dir"
cmake -G Ninja ../../.. ${CMAKE_OPTS} || die "Unable to run cmake"
OPT_TARGETS=$(ninja help | grep "/all:"  | cut -d: -f1)
# make sure "all" is run first
for target in all ${OPT_TARGETS}; do
    info "Build target '${target}'"
    if [ ${GHA} -eq 0 ]; then
        ninja ${NINJA_OPTS} ${target}
    else
        # silent build mode, discard Ninja output
        # as Ninja redirects toolchain stderr to stdout, need to filter out
        # NINJA_STATUS prolog to get the compiler warnings and errors

        # now use POSIX shell black magic to preserve Ninja exit status while
        # filtering its standard output
        { { { { ninja ${NINJA_OPTS} ${target}; echo $? >&3; } | \
                filter_issues >&4; } 3>&1; } | (read xs; exit $xs); } 4>&1
    fi
done
