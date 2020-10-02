#!/bin/sh

SCRIPT_DIR=$(dirname $0)
. ${SCRIPT_DIR}/funcs.sh

if [ $# -ne 4 ]; then
    die "Invalid parameters"
fi

GH_EVENT="$1"
GH_SHAS="$2"
BUILD_STATUS="$3"
UTEST_STATUS="$4"

if [ ! -s "${BUILD_STATUS}" ]; then
    die "Invalid build status file ${BUILD_STATUS}"
fi
if [ ! -s "${UTEST_STATUS}" ]; then
    die "Invalid unit test status file ${UTEST_STATUS}"
fi

set -eu

BUILD_SESSIONS=$(cat ${BUILD_STATUS} | cut -d'|' -f1)
BUILD_FAILURES=$(cat ${BUILD_STATUS} | cut -d'|' -f2)
BUILD_ERRORS=$(cat ${BUILD_STATUS} | cut -d'|' -f3)
BUILD_WARNINGS=$(cat ${BUILD_STATUS} | cut -d'|' -f4)
UTEST_SESSIONS=$(cat ${UTEST_STATUS} | cut -d'|' -f1)
UTEST_ABORTS=$(cat ${UTEST_STATUS} | cut -d'|' -f2)
UTEST_TESTS=$(cat ${UTEST_STATUS} | cut -d'|' -f3)
UTEST_FAILURES=$(cat ${UTEST_STATUS} | cut -d'|' -f4)
UTEST_IGNORED=$(cat ${UTEST_STATUS} | cut -d'|' -f5)

echo "::set-env name=BUILD_SESSIONS::${BUILD_SESSIONS}"
echo "::set-env name=BUILD_FAILURES::${BUILD_FAILURES}"
echo "::set-env name=BUILD_ERRORS::${BUILD_ERRORS}"
echo "::set-env name=BUILD_WARNINGS::${BUILD_WARNINGS}"

echo "::set-env name=UTEST_SESSIONS::${UTEST_SESSIONS}"
echo "::set-env name=UTEST_ABORTS::${UTEST_ABORTS}"
echo "::set-env name=UTEST_TESTS::${UTEST_TESTS}"
echo "::set-env name=UTEST_FAILURES::${UTEST_FAILURES}"
echo "::set-env name=UTEST_IGNORED::${UTEST_IGNORED}"

set +e

ERR_MSG=""
WARN_MSG=""
if [ ${BUILD_SESSIONS} -eq 0 ]; then
    ERR_MSG="No build performed"
elif [ ${UTEST_SESSIONS} -eq 0 ]; then
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
if [ ${BUILD_WARNINGS} -ne 0 ]; then
    MSG="${BUILD_WARNINGS} build warnings"
    if [ -z "${ERR_MSG}" ]; then
        WARN_MSG="${MSG}"
    else
        ERR_MSG="${ERR_MSG}, ${MSG}"
    fi
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

if [ "${GH_EVENT}" = "pull_request" ]; then
    SHA="$(echo ${GH_SHAS} | cut -d: -f2)"
else
    SHA="$(echo ${GH_SHAS} | cut -d: -f1)"
fi

FOOTER="${SHA::8}: ${UTEST_TESTS} test passed, ${UTEST_IGNORED} test ignored"
echo "::set-env name=SLACK_FOOTER::${FOOTER}"
