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
REPORTLOG=""

. ${SCRIPT_DIR}/funcs.sh

# Cleanup function on exit
cleanup() {
    if [ -n "${REPORTLOG}" -a -f "${REPORTLOG}" ]; then
        rm "${REPORTLOG}"
    fi
    echo "GITHUB_ENV ${GITHUB_ENV}"
    if [ ${FAKE_GITHUB_ENV} -ne 0 ]; then
        if [ -f "${GITHUB_ENV}" ]; then
            echo "deleting FAKE GITHUB_ENV"
            cat ${GITHUB_ENV}
            rm "${GITHUB_ENV}"
        fi
    fi
}

trap cleanup EXIT

usage() {
    NAME=`basename $0`
    cat <<EOT
$NAME [-h] [-a] [-g] [-r] [-s] [dts] ...

 dts: the name of a dts file (w/o path or extension)

 -h:  print this help
 -a:  abort on first failed build (default: resume)
 -g:  github mode (filter compiler output, emit results as env. var.)
 -r:  create a summary report
 -s:  run static analyzer in addition to regular builds
EOT
}

SA=0
ABORT=0
GHA=0
OPTS=""
while [ $# -gt 0 ]; do
    case "$1" in
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
        -r)
            REPORTLOG=$(mktemp)
            OPTS="${OPTS} -r ${REPORTLOG}"
            ;;
        -s)
            SA=1
            ;;
        -*)
            ;;
        *)
            DTS="${DTS} ${1}"
            ;;
    esac
    shift
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
        echo ""
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

WARNCOUNT=0
ERRCOUNT=0
if [ -n "${REPORTLOG}" ]; then
    # Discard all ANSI escape sequences
    cat "${REPORTLOG}" | sed 's/\x1b\[[0-9;]*m//g' | \
        sort -u > "${REPORTLOG}.tmp"
    mv "${REPORTLOG}.tmp" "${REPORTLOG}"
    if [ -s "${REPORTLOG}" ]; then
        echo ""
        warning "Warnings:"
        IFS=$'\n'; cat "${REPORTLOG}" | grep " warning:" | \
            for warn in $(sed -e 's/^.*\[\(.*\)\]/\1/' | \
                                sed -e 's/-Werror,//g' | \
                                sort | uniq -c); do
                warning " ${warn}"
                count="$(echo "${warn}" | cut -d- -f1 | tr -d [:space:])"
                if [ -n "${count}" ]; then
                    WARNCOUNT=$(expr ${WARNCOUNT} + ${count})
                fi
                # for some reason (subshell?) global WARNCOUNT is not updated
                # in Alpine shell whereas it is on other shells; use a temp.
                # file to store the result
                echo "${WARNCOUNT}" > .warncount
            done
        if [ -f .warncount ]; then
            WARNCOUNT="$(cat .warncount)"
            rm -f .warncount
        fi
        warning " ${WARNCOUNT} total"
        ERRCOUNT=$(cat "${REPORTLOG}" | grep " error:" | \
                   wc -l | tr -d [:space:])
        if [ ${ERRCOUNT} -gt 0 ]; then
            echo ""
            error "Errors: ${ERRCOUNT} total"
        fi
    fi
fi

if [ ${GHA} -ne 0 ]; then
    echo ""
    echo "BUILD_SESSIONS=${TOTAL}" >> $GITHUB_ENV
    echo "BUILD_FAILURES=${FAILURES}" >> $GITHUB_ENV
    echo "BUILD_WARNINGS=${WARNCOUNT}" >> $GITHUB_ENV
    echo "BUILD_ERRORS=${ERRCOUNT}" >> $GITHUB_ENV
    echo ""
fi
