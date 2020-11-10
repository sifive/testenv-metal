#!/bin/sh

SCRIPT_DIR=$(dirname $0)
. ${SCRIPT_DIR}/funcs.sh

if [ $# -ne 5 ]; then
    die "Invalid parameters"
fi

GH_EVENT="$1"
GH_REF="$2"
GH_SHAS="$3"
BUILD_STATUS="$4"
UTEST_STATUS="$5"

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

echo "BUILD_SESSIONS=${BUILD_SESSIONS}" >> $GITHUB_ENV
echo "BUILD_FAILURES=${BUILD_FAILURES}" >> $GITHUB_ENV
echo "BUILD_ERRORS=${BUILD_ERRORS}" >> $GITHUB_ENV
echo "BUILD_WARNINGS=${BUILD_WARNINGS}" >> $GITHUB_ENV

echo "UTEST_SESSIONS=${UTEST_SESSIONS}" >> $GITHUB_ENV
echo "UTEST_ABORTS=${UTEST_ABORTS}" >> $GITHUB_ENV
echo "UTEST_TESTS=${UTEST_TESTS}" >> $GITHUB_ENV
echo "UTEST_FAILURES=${UTEST_FAILURES}" >> $GITHUB_ENV
echo "UTEST_IGNORED=${UTEST_IGNORED}" >> $GITHUB_ENV

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
    echo "SLACK_TITLE=${ERR_MSG} :scream:" >> $GITHUB_ENV
    echo "SLACK_COLOR=#DC143C" >> $GITHUB_ENV
elif [ -n "${WARN_MSG}" ]; then
    echo "SLACK_TITLE=${WARN_MSG} :worried:" >> $GITHUB_ENV
    echo "SLACK_COLOR=#FFA500" >> $GITHUB_ENV
else
    echo "SLACK_TITLE=Success :thumbsup:" >> $GITHUB_ENV
    echo "SLACK_COLOR=#32CD32" >> $GITHUB_ENV
fi

if [ "${GH_EVENT}" = "pull_request" ]; then
    SHORT_SHA="$(echo ${GH_SHAS} | cut -d: -f2 | cut -c-8)"
    REF=$(echo ${GH_REF} | cut -c2-)
    echo "SLACK_MESSAGE=${REF}" >> $GITHUB_ENV
else
    SHORT_SHA="$(echo ${GH_SHAS} | cut -d: -f1 | cut -c-8)"
fi

FOOTER="${SHORT_SHA}: ${UTEST_TESTS} test passed, ${UTEST_IGNORED} test ignored"
echo "SLACK_FOOTER=${FOOTER}" >> $GITHUB_ENV
