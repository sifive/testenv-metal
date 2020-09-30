#!/bin/sh

SCRIPT_DIR=$(dirname $0)
. ${SCRIPT_DIR}/funcs.sh

if [ $# -ne 2 ]; then
    die "Invalid parameters"
fi

BUILD_STATUS="$1"
UTEST_STATUS="$2"

if [ ! -s "${BUILD_STATUS}" ]; then
    die "Invalid build status file ${BUILD_STATUS}"
fi
if [ ! -s "${UTEST_STATUS}" ]; then
    die "Invalid build status file ${UTEST_STATUS}"
fi

set -eu

BUILD_TOTAL=$(cat ${BUILD_STATUS} | cut -d'|' -f1)
BUILD_FAILURES=$(cat ${BUILD_STATUS} | cut -d'|' -f2)
BUILD_ERRORS=$(cat ${BUILD_STATUS} | cut -d'|' -f3)
BUILD_WARNINGS=$(cat ${BUILD_STATUS} | cut -d'|' -f4)
UTEST_TOTAL=$(cat ${UTEST_STATUS} | cut -d'|' -f1)
UTEST_ABORTS=$(cat ${UTEST_STATUS} | cut -d'|' -f2)
UTEST_TESTS=$(cat ${UTEST_STATUS} | cut -d'|' -f3)
UTEST_FAILURES=$(cat ${UTEST_STATUS} | cut -d'|' -f4)
UTEST_IGNORED=$(cat ${UTEST_STATUS} | cut -d'|' -f5)

echo "::set-env name=BUILD_TOTAL::${BUILD_TOTAL}"
echo "::set-env name=BUILD_FAILURES::${BUILD_FAILURES}"
echo "::set-env name=BUILD_ERRORS::${BUILD_ERRORS}"
echo "::set-env name=BUILD_WARNINGS::${BUILD_WARNINGS}"

echo "::set-env name=UTEST_TOTAL::${UTEST_TOTAL}"
echo "::set-env name=UTEST_ABORTS::${UTEST_ABORTS}"
echo "::set-env name=UTEST_TESTS::${UTEST_TESTS}"
echo "::set-env name=UTEST_FAILURES::${UTEST_FAILURES}"
echo "::set-env name=UTEST_IGNORED::${UTEST_IGNORED}"

set +e

ERR_MSG=""
WARN_MSG=""
if [ ${BUILD_TOTAL} -eq 0 ]; then
    ERR_MSG="No build performed"
elif [ ${UTEST_TOTAL} -eq 0 ]; then
    ERR_MSG="No unit tests performed"
elif [ ${BUILD_FAILURES} -ne 0 ]; then
    ERR_MSG="${BUILD_FAILURES} build failures"
elif [ ${BUILD_ERRORS} -ne 0 ]; then
    ERR_MSG="${BUILD_ERRORS} build errors"
elif [ ${UTEST_ABORTS} -ne 0 ]; then
    ERR_MSG="${UTEST_ABORTS} unit test aborts"
elif [ ${UTEST_FAILURES} -ne 0 ]; then
    ERR_MSG="${UTEST_FAILURES} unit test failures"
elif [ ${BUILD_WARNINGS} -ne 0 ]; then
    WARN_MSG="${BUILD_WARNINGS} build warnings"
fi

echo ""

if [ -n "${ERR_MSG}" ]; then
    echo "::set-env name=SLACK_TITLE::${ERR_MSG}"
    echo "::set-env name=SLACK_COLOR::red"
    EMOJI=scream
elif [ -n "${WARN_MSG}" ]; then
    echo "::set-env name=SLACK_TITLE::${WARN_MSG}"
    echo "::set-env name=SLACK_COLOR::orange"
    EMOJI=worried
else
    echo "::set-env name=SLACK_TITLE::Success"
    echo "::set-env name=SLACK_COLOR::green"
    EMOJI=thumbsup
fi

GIT_INFO=$(cd scl-metal && git log -1 --pretty=%H:%B | head -1)
GIT_SHA="$(echo ${GIT_INFO} | cut -d: -f1)"
GIT_MSG="$(echo ${GIT_INFO} | cut -d: -f2)"

echo "::set-env name=SLACK_FOOTER::${GIT_SHA}"
echo "::set-env name=SLACK_MESSAGE::${GIT_MSG}"
echo "::set-env name=SLACK_ICON_EMOJI::\${{ :${EMOJI}: }}"
