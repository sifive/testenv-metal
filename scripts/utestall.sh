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

usage() {
    NAME=`basename $0`
    cat <<EOT
$NAME [-h] [-a] [-s] [-q qemu_dir] <test_dir> ...

 test_dir: top level directory to see for tests

 -h:  print this help
 -a:  abort on first failed build (default: resume)
 -q:  the directory which contains qemu
EOT
}

ABORT=0
QEMUOPT=""
for arg in $*; do
    case ${arg} in
        -a)
            ABORT=1
            ;;
        -q)
            shift
            QEMUOPT="-q $1"
            test -d "${QEMUPATH}" || die "Invalid QEMU directory ${QEMUPOATH}"
            ;;
        -h)
            usage
            exit 0
            ;;
        -*)
            ;;
        *)
            TESTDIR="${TESTDIR} ${arg}"
            ;;
    esac
done

test -n "${TESTDIR}" || usage

FAILURE=0
for testdir in ${TESTDIR}; do
    dtsdirs=$(cd ${testdir} && find . -type d -depth 1)
    for dts in ${dtsdirs}; do
        dts=$(echo "${dts}" | cut -c3-)
        for build in ${BUILDS}; do
            if [ -d ${testdir}/${dts}/${build} ]; then
                udts=$(echo "${dts}" | tr [:lower:] [:upper:])
                ubuild=$(echo "${build}" | tr [:lower:] [:upper:])
                echo "[Testing ${udts} in ${ubuild}]"
                ${SCRIPT_DIR}/utest.sh ${QEMUOPT} -d "bsp/${dts}/dts/qemu.dts" ${testdir}/${dts}/${build}
                if [ $? -ne 0 ]; then
                    echo "Test failed (${udts} in ${ubuild})" >&2
                    if [ ${ABORT} -gt 0 ]; then
                        exit $?
                    else
                        FAILURE=1
                    fi
                fi
            fi
        done
    done
done

if [ ${FAILURE} -ne 0 ]; then
    echo "At least one test failed" >&2
    exit 1
fi
