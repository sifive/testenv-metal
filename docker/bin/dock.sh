#!/bin/sh

# Die with an error message
die() {
    echo "$*" >&2
    exit 1
}

SCRIPT_DIR=$(dirname $0)
NAME=`basename $PWD`
IMGENV="$1"
test -n "${IMGENV}" || die "Missing Docker environment"
IMGID=$(docker images -q "${IMGENV}")
test -n "${IMGID}" || echo "Docker environment ${IMGENV} not available locally"

echo "Using Docker environment \"${IMGENV}\""
shift

cmd="$1"
if [ -n "$cmd" -a -f "${SCRIPT_DIR}/${cmd}.sh" ]; then
    shift
    ARGS="sh /tmp/${NAME}/${SCRIPT_DIR}/${cmd}.sh $*"
else
    ARGS="$*"
fi

DIR=`dirname $0`
VOLUMES=`${DIR}/dockvol.sh`
[ $? -eq 0 ] || exit 1

OPTS=""
for volume in ${VOLUMES}; do
    OPTS="${OPTS} --volumes-from ${volume}"
done

IMGPATH=/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin
IMGPATH=${IMGPATH}:/usr/local/clang10/bin:/usr/local/riscv-elf-binutils/bin

# Development (transparent) mode"
# - build with user id so that output files belongs to the effective user
DOCKOPTS="--user $(id -u):$(id -g)"

docker run \
    --rm \
    --name ${NAME} \
    ${OPTS} \
    --env PATH=${IMGPATH} \
    --mount type=bind,source=${PWD},target=/tmp/${NAME} \
    ${DOCKOPTS} \
    --workdir=/tmp/${NAME} ${IMGENV} \
    ${ARGS}
DOCKER_RC=$?

exit ${DOCKER_RC}
