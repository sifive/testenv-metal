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
QEMUOPT=""
while [ $# -gt 0 ]; do
    case "$1" in
        -a)
            ABORT=1
            ;;
        -e)
            shift
            QEMUPATH="$1"
            test -d "${QEMUPATH}" || die "Invalid QEMU directory ${QEMUPATH}"
            QEMUOPT="-e $1"
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

FAILURES=0
TOTAL=0
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
                ${SCRIPT_DIR}/utest.sh ${QEMUOPT} -d "bsp/${dts}/dts/qemu.dts"\
                    ${testdir}/${dts}/${build}
                if [ $? -ne 0 ]; then
                    error "Test failed (${udts} in ${ubuild})"
                    if [ ${ABORT} -gt 0 ]; then
                        exit $?
                    else
                        FAILURES=$(expr ${FAILURES} + 1)
                    fi
                fi
            fi
        done
    done
done

if [ ${FAILURES} -ne 0 ]; then
    warning "${FAILURES} test sesssions failed"
fi

if [ ${GHA} -ne 0 ]; then
    echo ""
    echo "::set-env name=UTEST_TOTAL::${TOTAL}"
    echo "::set-env name=UTEST_FAILURES::${FAILURES}"
    echo ""
fi
