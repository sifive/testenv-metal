#!/bin/sh
#------------------------------------------------------------------------------
# Build all BSPs/builds combinations.
#
# This script only exists to circumvent current GitHub actions limitations,
# where the strategy matrix feature does not allow to perform global actions
# before running the matrix combinations
#------------------------------------------------------------------------------

SCRIPT_DIR=$(dirname $0)
DTS=""
BUILDS="debug release"

. ${SCRIPT_DIR}/funcs.sh

usage() {
    NAME=`basename $0`
    cat <<EOT
$NAME [-h] [-a] [-g] [-s] [dts] ...

 dts: the name of a dts file (w/o path or extension)

 -h:  print this help
 -a:  abort on first failed build (default: resume)
 -g:  github mode (filter compiler output, emit results as env. var.)
 -s:  run static analyzer in addition to regular builds
EOT
}

SA=0
ABORT=0
GHA=0
OPTS=""
for arg in $*; do
    case ${arg} in
        -a)
            ABORT=1
            ;;
        -g)
            GHA=1
            OPTS="${OPTS} -g"
            ;;
        -h)
            usage
            exit 0
            ;;
        -s)
            SA=1
            ;;
        -*)
            ;;
        *)
            DTS="${DTS} ${arg}"
            ;;
    esac
done
if [ $SA -gt 0 ]; then
    BUILDS="${BUILDS} static_analysis"
fi

test -n "${DTS}" || die "No target specified"

FAILURES=0
TOTAL=0
for dts in ${DTS}; do
    for build in ${BUILDS}; do
        udts=$(echo "${dts}" | tr [:lower:] [:upper:])
        ubuild=$(echo "${build}" | tr [:lower:] [:upper:])
        info "Building [${udts} in ${ubuild}]"
        TOTAL="$(expr ${TOTAL} + 1)"
        ${SCRIPT_DIR}/build.sh ${OPTS} ${dts} ${build}
        if [ $? -ne 0 ]; then
            error "Build failed (${udts} in ${ubuild})"
            if [ ${ABORT} -gt 0 ]; then
                exit $?
            else
                FAILURES="$(expr ${FAILURES} + 1)"
            fi
        fi
    done
done

if [ ${FAILURES} -ne 0 ]; then
    warning "WARNING: ${FAILURES} build failed"
fi

if [ ${GHA} -ne 0 ]; then
    echo ""
    echo "::set-env name=BUILD_TOTAL::${TOTAL}"
    echo "::set-env name=BUILD_FAILURES::${FAILURES}"
    echo ""
fi
