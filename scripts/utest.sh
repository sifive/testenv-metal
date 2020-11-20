#!/bin/sh
#------------------------------------------------------------------------------
# Execute freedom-metal unit tests with a QEMU virtual machine
#
# Dependencies:
#  * qemu VM with FDT support
#  * riscv64-unknown-elf-readelf (from RISC-V ELF binutils)
#  * dtc (device tree compiler)
#  * POSIX shell, find, sed, grep, cut
#------------------------------------------------------------------------------

TMPDIR=""
SCRIPT_DIR=$(dirname $0)
. ${SCRIPT_DIR}/funcs.sh

# Cleanup function on exit
cleanup() {
    if [ -n "${TMPDIR}" -a -d "${TMPDIR}" ]; then
        rm -r "${TMPDIR}"
    fi
}

TOTAL_TESTS=0
TOTAL_FAILURES=0
TOTAL_IGNORED=0

# Show script usage
usage() {
    NAME=`basename $0`
    cat <<EOT
$NAME [-h] <-d DTS> [-q QEMU_DIR] [-r results] [unit_test|unit_test_dir] ...
EOT
}

# cat /etc/passwd | filter_tests
# exit 0

READELF=$(which riscv64-unknown-elf-readelf 2>/dev/null)
test -n "${READELF}" || die "Unable to locate readelf for RISC-V"
DTC=$(which dtc 2>/dev/null)
test -n "${DTC}" || die "Unable to locate dtc"

DTS=""
UNIT_TESTS=""
QEMUPATH=""
RESULT_FILE=""
while [ $# -gt 0 ]; do
    case "$1" in
        -h)
            usage
            exit 0
            ;;
        -d)
            shift
            DTS="$1"
            test -f "${DTS}" || die "Unable to find DTS ${DTS}"
            ;;
        -e)
            shift
            QEMUPATH="$1"
            test -d "${QEMUPATH}" || die "Invalid QEMU directory ${QEMUPATH}"
            ;;
        -r)
            shift
            RESULT_FILE="$1"
            ;;
        *.elf)
            test -f $1 || die "UT $1 does not exist"
            UNIT_TESTS="${UNIT_TESTS} $1"
            ;;
        *)  if [ -d $1 ]; then
                for ut in $(find $1 -type f -name "*.elf"); do
                    UNIT_TESTS="${UNIT_TESTS} ${ut}"
                done
            fi
            ;;
    esac
    shift
done

test -n "${DTS}" || die "DTS should be specified"
if  [ -z "${UNIT_TESTS}" ]; then
    warning "- no test to execute"
    echo ""
    exit 0
fi

UNIT_TESTS=$(echo "${UNIT_TESTS}" | sed 's/^ *//g')
FIRST_UT=$(echo "${UNIT_TESTS}" | cut -d' ' -f1)
RV=$(${READELF} -h ${FIRST_UT} | grep Class | cut -d: -f2 | sed 's/^ *ELF//')
if [ -z "${QEMUPATH}" ]; then
    QEMU="$(which qemu-system-riscv${RV})"
else
    QEMU="${QEMUPATH}/qemu-system-riscv${RV}"
fi
test -x "${QEMU}" || die "Unable to locate QEMU for RV${RV}"

TMPDIR=$(mktemp -d)
trap cleanup EXIT

${DTC} ${DTS} > ${TMPDIR}/qemu.dtb

# log QEMU version
${QEMU} --version | head -1

QEMU_GLOBAL_OPTS="-machine sifive_fdt -nographic -dtb ${TMPDIR}/qemu.dtb"
for ut in ${UNIT_TESTS}; do
    info "- running UT [$(basename ${ut})]"
    QEMU_OPTS="${QEMU_GLOBAL_OPTS} -kernel ${ut}"
    # now use POSIX shell black magic to preserve Ninja exit status while
    # filtering its standard output
    { { { { ${QEMU} ${QEMU_OPTS}; echo $? >&3; } | \
            tee ${TMPDIR}/output.log >&4; } 3>&1; } \
        | (read xs; exit $xs); } 4>&1
    EXEC_STATUS=$?
    if [ ${EXEC_STATUS} -ne 0 ]; then
        error "UT failed ($(basename ${ut})) [$EXEC_STATUS]"
    else
        echo ""
    fi
    results=$(cat ${TMPDIR}/output.log | sed 's/\x1b\[[0-9;]*m//g' |
              egrep "[0-9]+ Tests [0-9]+ Failures [0-9]+ Ignored")
    if [ -n "${results}" ]; then
        tests=$(echo "${results}" | cut -d' ' -f1)
        failures=$(echo "${results}" | cut -d' ' -f3)
        ignored=$(echo "${results}" | cut -d' ' -f5)
        TOTAL_TESTS="$(expr ${TOTAL_TESTS} + ${tests})"
        TOTAL_FAILURES="$(expr ${TOTAL_FAILURES} + ${failures})"
        TOTAL_IGNORED="$(expr ${TOTAL_IGNORED} + ${ignored})"
    else
        if [ ${EXEC_STATUS} -gt 125 ]; then
            # the test session failed w/o any execution trace,
            # it may be a serious issue, such as a QEMU VM crash
            # abort immediately
            exit ${EXEC_STATUS}
        fi
    fi
    rm -f ${TMPDIR}/output.log
done

if [ -n "${RESULT_FILE}" ]; then
    rm -f "${RESULT_FILE}"
    echo "${TOTAL_TESTS}:${TOTAL_FAILURES}:${TOTAL_IGNORED}" > ${RESULT_FILE}
fi
