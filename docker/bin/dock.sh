#!/bin/sh

ARGS=$*

DIR=`dirname $0`
VOLUMES=`${DIR}/dockvol.sh`
[ $? -eq 0 ] || exit 1

OPTS=""
for volume in ${VOLUMES}; do
    OPTS="${OPTS} --volumes-from ${volume}"
done

DEVENV="iroazh/freedom-dev:a3.12"

IMGPATH=/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin
IMGPATH=${IMGPATH}:/usr/local/clang10/bin:/usr/local/riscv-elf-binutils/bin

echo "Using Docker environment \"${DEVENV}\""
NAME=`basename $PWD`

# Development (transparent) mode"
# - build with user id so that output files belongs to the effective user
DOCKOPTS="--user $(id -u):$(id -g)"

docker run \
    --tty \
    --interactive \
    --rm \
    --name ${NAME} \
    ${OPTS} \
    --env PATH=${IMGPATH} \
    --mount type=bind,source=${PWD},target=/tmp/${NAME} \
    ${DOCKOPTS} \
    --workdir=/tmp/${NAME} ${DEVENV} \
    ${*}
DOCKER_RC=$?

exit ${DOCKER_RC}
