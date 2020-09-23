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

# Die with an error message
die() {
    echo "$*" >&2
    exit 1
}

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
            QEMUPATH="$1"
            test -d "${QEMUPATH}" || die "Invalid QEMU directory ${QEMUPATH}"
            QEMUOPT="-q $1"
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

test -n "${TESTDIR}" || die "No test direcory specified"

FAILURE=0
for testdir in ${TESTDIR}; do
    dtsdirs=$(cd ${testdir} && find . -type d -maxdepth 1)
    for dts in ${dtsdirs}; do
        dts=$(echo "${dts}" | cut -c3-)
        for build in ${BUILDS}; do
            if [ -d ${testdir}/${dts}/${build} ]; then
                udts=$(echo "${dts}" | tr [:lower:] [:upper:])
                ubuild=$(echo "${build}" | tr [:lower:] [:upper:])
                echo ""
                echo "\033[36m[Testing ${udts} in ${ubuild}]\033[39m"
                ${SCRIPT_DIR}/utest.sh ${QEMUOPT} -d "bsp/${dts}/dts/qemu.dts" ${testdir}/${dts}/${build}
                if [ $? -ne 0 ]; then
                    echo "\033[31mTest failed (${udts} in ${ubuild})\033[39m" >&2
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
    echo "\033[31mAt least one test failed\033[39m" >&2
    exit 1
fi
