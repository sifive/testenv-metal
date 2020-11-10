#!/bin/sh
#------------------------------------------------------------------------------
# Build all BSPs/builds combinations.
#
# This script only exists to circumvent current GitHub actions limitations,
# where the strategy matrix feature does not allow to perform global actions
# before running the matrix combinations
#------------------------------------------------------------------------------

SCRIPT_DIR=$(dirname $0)
TESTDIR=""
BUILDS="debug release"

. ${SCRIPT_DIR}/funcs.sh

usage() {
    NAME=`basename $0`
    cat <<EOT
$NAME [-h] [-a] [-g] [-e qemu_dir] <test_dir> ...

 test_dir: top level directory to see for tests

 -h:  print this help
 -a:  abort on first failed build (default: resume)
 -e:  the directory which contains qemu
 -g:  github mode (emit results as env. var.)
EOT
}

ABORT=0
GHA=0
OPTS=""
while [ $# -gt 0 ]; do
    case "$1" in
        -a)
            ABORT=1
            ;;
        -e)
            shift
            QEMUPATH="$1"
            test -d "${QEMUPATH}" || die "Invalid QEMU directory ${QEMUPATH}"
            OPTS="-e $1"
            ;;
        -g)
            GHA=1
            ;;
        -h)
            usage
            exit 0
            ;;
        -*)
            ;;
        *)
            TESTDIR="${TESTDIR} ${1}"
            ;;
    esac
    shift
done

test -n "${TESTDIR}" || die "No test direcory specified"

TOTAL=0
ABORTS=0
TOTAL_TESTS=0
TOTAL_FAILURES=0
TOTAL_IGNORED=0
for testdir in ${TESTDIR}; do
    dtsdirs=$(cd ${testdir} && find . -type d -maxdepth 1) || \
        die "Unable to find unit tests from ${testdir}"
    for dts in ${dtsdirs}; do
        dts=$(echo "${dts}" | cut -c3-)  # skip leading ./
        if [ -z "${dts}" ]; then
            continue
        fi
        for build in ${BUILDS}; do
            if [ -d ${testdir}/${dts}/${build} ]; then
                udts=$(echo "${dts}" | tr [:lower:] [:upper:])
                ubuild=$(echo "${build}" | tr [:lower:] [:upper:])
                info "Testing ${udts} in ${ubuild}"
                TOTAL="$(expr ${TOTAL} + 1)"
                if [ ${GHA} -ne 0 ]; then
                    RESULT_LOG="${testdir}/${dts}/${build}/results.log"
                    RESULT="-r ${RESULT_LOG}"
                    rm -f "${RESULT_LOG}"
                else
                    RESULT=""
                fi
                ${SCRIPT_DIR}/utest.sh ${OPTS} -d "bsp/${dts}/dts/qemu.dts"\
                    ${RESULT} ${testdir}/${dts}/${build}
                if [ $? -ne 0 ]; then
                    error "Test failed (${udts} in ${ubuild})"
                    if [ ${ABORT} -gt 0 ]; then
                        exit $?
                    else
                        ABORTS=$(expr ${ABORTS} + 1)
                    fi
                fi
                if [ ${GHA} -ne 0 -a -s "${RESULT_LOG}" ]; then
                    results=$(cat "${RESULT_LOG}")
                    tests=$(echo "${results}" | cut -d: -f1)
                    failures=$(echo "${results}" | cut -d: -f2)
                    ignored=$(echo "${results}" | cut -d: -f3)
                    TOTAL_TESTS="$(expr ${TOTAL_TESTS} + ${tests})"
                    TOTAL_FAILURES="$(expr ${TOTAL_FAILURES} + ${failures})"
                    TOTAL_IGNORED="$(expr ${TOTAL_IGNORED} + ${ignored})"
                    rm -f "${RESULT_LOG}"
                fi
            fi
        done
    done
done

if [ ${ABORTS} -ne 0 ]; then
    warning "${ABORTS} test sesssions failed"
fi

if [ ${GHA} -ne 0 ]; then
    echo ""
    echo "UTEST_SESSIONS=${TOTAL}" >> $GITHUB_ENV
    echo "UTEST_ABORTS=${ABORTS}" >> $GITHUB_ENV
    echo "UTEST_TESTS=${TOTAL_TESTS}" >> $GITHUB_ENV
    echo "UTEST_FAILURES=${TOTAL_FAILURES}" >> $GITHUB_ENV
    echo "UTEST_IGNORED=${TOTAL_IGNORED}" >> $GITHUB_ENV
    echo ""
fi
