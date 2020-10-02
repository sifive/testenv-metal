#!/bin/sh

SCRIPT_DIR=$(dirname $0)
. ${SCRIPT_DIR}/funcs.sh

if [ $# -ne 4 ]; then
    die "Invalid parameters"
fi

EVENT_NAME="$1"
BUILD_STATUS="$2"
UTEST_STATUS="$3"
SCL_INFO="$4"

if [ ! -s "${BUILD_STATUS}" ]; then
    die "Invalid build status file ${BUILD_STATUS}"
fi
if [ ! -s "${UTEST_STATUS}" ]; then
    die "Invalid unit test status file ${UTEST_STATUS}"
fi
if [ ! -s "${SCL_INFO}" ]; then
    die "Invalid SCL-metal info file ${SCL_INFO}"
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
fi
if [ ${BUILD_FAILURES} -ne 0 ]; then
    MSG="${BUILD_FAILURES} build failures"
    if [ -n "${ERR_MSG}" ]; then
        ERR_MSG="${ERR_MSG}, ${MSG}"
    else
        ERR_MSG="${MSG}"
    fi
fi
if [ ${BUILD_ERRORS} -ne 0 ]; then
    MSG="${BUILD_ERRORS} build errors"
    if [ -n "${ERR_MSG}" ]; then
        ERR_MSG="${ERR_MSG}, ${MSG}"
    else
        ERR_MSG="${MSG}"
    fi
fi
if [ ${UTEST_ABORTS} -ne 0 ]; then
    MSG="${UTEST_ABORTS} test aborts"
    if [ -n "${ERR_MSG}" ]; then
        ERR_MSG="${ERR_MSG}, ${MSG}"
    else
        ERR_MSG="${MSG}"
    fi
fi
if [ ${UTEST_FAILURES} -ne 0 ]; then
    MSG="${UTEST_FAILURES} test failures"
    if [ -n "${ERR_MSG}" ]; then
        ERR_MSG="${ERR_MSG}, ${MSG}"
    else
        ERR_MSG="${MSG}"
    fi
fi
if [ -z "${ERR_MSG}" -a ${BUILD_WARNINGS} -ne 0 ]; then
    WARN_MSG="${BUILD_WARNINGS} build warnings"
fi

echo ""

if [ -n "${ERR_MSG}" ]; then
    echo "::set-env name=SLACK_TITLE::${ERR_MSG} :scream:"
    echo "::set-env name=SLACK_COLOR::#DC143C"
elif [ -n "${WARN_MSG}" ]; then
    echo "::set-env name=SLACK_TITLE::${WARN_MSG} :worried:"
    echo "::set-env name=SLACK_COLOR::#FFA500"
else
    echo "::set-env name=SLACK_TITLE::Success :thumbsup:"
    echo "::set-env name=SLACK_COLOR::#32CD32"
fi

if [ "${EVENT_NAME}" = "push" ]; then
    # commit to this repo
    echo "::set-env name=SLACK_FOOTER::${GITHUB_SHA}"
else
    # commit to subrepo
    GIT_SHA="$(cat ${SCL_INFO} | cut -d: -f1)"
    GIT_NAME="$(cat ${SCL_INFO} | cut -d: -f2)"
    GIT_MSG="$(cat ${SCL_INFO} | cut -d: -f3)"

    echo "::set-env name=SLACK_FOOTER::${GIT_SHA} (${GIT_NAME})"
    echo "::set-env name=SLACK_MESSAGE::${GIT_MSG}"
fi
