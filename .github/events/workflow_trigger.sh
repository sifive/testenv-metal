#!/bin/sh

# curl -XPOST -u "${{ secrets.PAT_USERNAME}}:${{secrets.PAT_TOKEN}}" -H "Accept: application/vnd.github.everest-preview+json" -H "Content-Type: application/json" https://api.github.com/repos/YOURNAME/APPLICATION_NAME/actions/workflows/build.yaml/dispatches --data '{"ref": "master"}'

URL="https://api.github.com/repos/sifive-eblot/freedom-metal/actions/workflows/build_test.yml/dispatches"
GH_USER="sifive-eblot"

if [ -z "${FWE_TOKEN}" ]; then
    echo "Missing token" >&2
    exit 1
fi

if [ -z "${SCL_REF}" ]; then
    echo "Missing SCL ref" >&2
    exit 1
fi

curl -XPOST -v \
     -u "${GH_USER}:${FWE_TOKEN}" \
     -H "Accept: application/vnd.github.everest-preview+json" \
     -H "Content-Type: application/json" \
     ${URL} \
     --data '{"ref": "gh_remote_event", "inputs": {"scl_ref": "${SCL_REF}"}}'
