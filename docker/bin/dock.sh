#!/bin/sh

SCRIPT_DIR=$(dirname $0)
NAME=`basename $PWD`
DEVENV="iroazh/freedom-dev:a3.12"
echo "Using Docker environment \"${DEVENV}\""

cmd="$1"
if [ -n "$cmd" -a -f "${SCRIPT_DIR}/${cmd}.sh" ]; then
    shift
    ARGS="sh /tmp/${NAME}/${SCRIPT_DIR}/${cmd}.sh $*"
else
    ARGS="$*"
fi

echo ARGS: $ARGS
#ls "${SCRIPT_DIR}/${cmd}.sh"

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
    --workdir=/tmp/${NAME} ${DEVENV} \
    ${ARGS}
DOCKER_RC=$?

exit ${DOCKER_RC}
