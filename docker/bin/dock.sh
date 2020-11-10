#!/bin/sh

# Die with an error message
die() {
    echo "$*" >&2
    exit 1
}

SCRIPT_DIR=$(dirname $0)
NAME=$(basename $PWD)
LOCAL_ENV=$(basename ${GITHUB_ENV})
OPTS=""

if [ "$1" = "-t" ]; then
    OPTS="-ti"
    shift
fi

CONF="$1"
CONFPATH="$(dirname $0)/../conf/${CONF}.conf"
test -f "${CONFPATH}" || die "Invalid Docker configuration '${CONF}'"

volumes() {
    VOLUMES=""
    for cnr in $*; do
        name=`echo $cnr | cut -d@ -f1`
        path=`echo $cnr | cut -d@ -f2`
        localname=`echo ${name} | tr ':' '_' | cut -d/ -f2`
        volume="${localname}-vol"
        if [ ! $(docker ps -q -a -f name=${volume}) ]; then
            echo "Instanciating missing volume $volume" >&2
            docker create -v ${path} --name ${volume} ${name} \
                /bin/true > /dev/null
            if [ $? -ne 0 ]; then
                exit 1
            fi
        fi
        if [ -n "${VOLUMES}" ]; then
            VOLUMES="${VOLUMES} ${volume}"
        else
            VOLUMES="${volume}"
        fi
    done

    echo "${VOLUMES}"
}

. "${CONFPATH}"

echo "Using Docker environment \"${BASE}\""
shift

cmd="$1"
if [ -n "$cmd" -a -f "${SCRIPT_DIR}/${cmd}.sh" ]; then
    shift
    ARGS="sh /tmp/${NAME}/${SCRIPT_DIR}/${cmd}.sh $*"
else
    ARGS="$*"
fi

VOLUMES=$(volumes ${CONTAINERS})
[ $? -eq 0 ] || exit 1

for volume in ${VOLUMES}; do
    OPTS="${OPTS} --volumes-from ${volume}"
done

IMGPATH=/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin
IMGPATH=${IMGPATH}:/usr/local/clang11/bin:/usr/local/riscv-elf-binutils/bin
IMGPATH=${IMGPATH}:/usr/local/qemu-fdt/bin

# Development (transparent) mode"
# - build with user id so that output files belongs to the effective user
DOCKOPTS="--user $(id -u):$(id -g)"

for symlink in $(cd "${PWD}" && find . -type l); do
    REALPATH="$(cd ${symlink} && pwd -P)"
    LOCPATH="$(echo $symlink | cut -c3-)"
    LNOPT="--mount type=bind,source=${REALPATH},target=/tmp/${NAME}/${LOCPATH}"
    DOCKOPTS="${DOCKOPTS} ${LNOPT}"
done

docker run \
    --rm \
    --name ${NAME} \
    ${OPTS} \
    --env PATH=${IMGPATH} \
    --env GITHUB_ENV=/tmp/${NAME}/${LOCAL_ENV} \
    --mount type=bind,source=${PWD},target=/tmp/${NAME} \
    ${DOCKOPTS} \
    --workdir=/tmp/${NAME} ${BASE} \
    ${ARGS}
DOCKER_RC=$?

if [ -s "${LOCAL_ENV}" ]; then
    cat ${LOCAL_ENV} >> ${GITHUB_ENV}
else
    echo "Unable to locate environment result file ${LOCAL_ENV}" >&2
fi
å
exit ${DOCKER_RC}
