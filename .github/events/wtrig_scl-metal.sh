#!/bin/sh

URL="https://api.github.com/repos/sifive-eblot/freedom-metal/actions/workflows/build_test.yml/dispatches"
GH_USER="sifive-eblot"
GH_BRANCH="gh_remote_event"

CURL_LOG=""

cleanup () {
    if  [ -n "${CURL_LOG}" ]; then
        rm -f "${CURL_LOG}"
    fi
}

if [ -z "${FWE_TOKEN}" ]; then
    echo "Missing token" >&2
    exit 1
fi

if [ -z "${SCL_REF}" ]; then
    echo "Missing SCL ref" >&2
    exit 1
fi

set -eu

PAYLOAD="{\"ref\": \"${GH_BRANCH}\", \"inputs\": {\"scl_ref\": \"${SCL_REF}\"}}"

CURL_LOG=$(mktemp)
trap cleanup EXIT

HTTP_CODE=$(curl -XPOST -s -o ${CURL_LOG} -w "%{http_code}" \
            -u "${GH_USER}:${FWE_TOKEN}" \
            -H "Accept: application/vnd.github.everest-preview+json" \
            -H "Content-Type: application/json" \
            ${URL} --data "${PAYLOAD}")

if [ ${HTTP_CODE} -ne 204 ]; then
    echo "Remote trigger failed (error: ${HTTP_CODE})" >&2
    cat ${CURL_LOG} >&2
    exit 1
fi
