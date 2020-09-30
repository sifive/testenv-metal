#!/bin/sh

GH_USER="sifive-eblot"
GH_BRANCH="gh_remote_event"

CURL_LOG=""

cleanup () {
    if  [ -n "${CURL_LOG}" ]; then
        rm -f "${CURL_LOG}"
    fi
}

# Die with an error message
die() {
    echo "$*" >&2
    exit 1
}

usage () {
    NAME=`basename $0`
    cat <<EOT
$NAME [-h] <-b branch> <-u user> <-t token_file> <-w workflow>

  branch:     git branch to run the workflow on
  user:       github username
  token_file: private file with github token
  workflow:   workflow name to trigger (w/o .yml suffix)

-h:  print this help
EOT
}

GH_BRANCH=""
GH_USER=""
GH_WKFLOW=""
GH_TOKEN_FILE=""
while [ $# -gt 0 ]; do
    case "$1" in
        -h)
            usage
            ;;
        -b)
            shift
            GH_BRANCH="$1"
            ;;
        -u)
            shift
            GH_USER="$1"
            ;;
        -t)
            shift
            GH_TOKEN_FILE="$1"
            ;;
        -w)
            shift
            GH_WKFLOW="$1"
            ;;
        *)
            die "Unknown argument: $1"
            ;;
    esac
    shift
done

GH_TOKEN=""
if [ -z "${GH_BRANCH}" ]; then
    die "Branch not specified"
fi
if [ -z "${GH_WKFLOW}" ]; then
    die "Workflow not specified"
fi
if [ -z "${GH_USER}" ]; then
    die "User not specified"
fi
if [ -z "${GH_TOKEN_FILE}" ]; then
    die "Token file not specified"
fi
if [ ! -r "${GH_TOKEN_FILE}" ]; then
    die "Cannot read token file"
fi
ls -l "${GH_TOKEN_FILE}" | cut -d' ' -f1 | cut -c5- | grep -q 'r'
if [ $? -ne 0 ]; then
    GH_TOKEN=$(cat "${GH_TOKEN_FILE}")
else
    die "Token file permission error: should be private"
fi

set -eu

PAYLOAD="{\"ref\": \"${GH_BRANCH}\"}"
URL="https://api.github.com/repos/sifive/testenv-metal/actions/workflows/${GH_WKFLOW}.yml/dispatches"

CURL_LOG=$(mktemp)
trap cleanup EXIT

HTTP_CODE=$(curl -XPOST -s -o ${CURL_LOG} -w "%{http_code}" \
            -u "${GH_USER}:${GH_TOKEN}" \
            -H "Accept: application/vnd.github.everest-preview+json" \
            -H "Content-Type: application/json" \
            ${URL} --data "${PAYLOAD}")

if [ ${HTTP_CODE} -ne 204 ]; then
    echo "Remote trigger failed (error: ${HTTP_CODE})" >&2
    cat ${CURL_LOG} >&2
    exit 1
else
    echo "Ok."
fi
