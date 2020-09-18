#!/bin/sh

# Die with an error message
die() {
    echo "$*" >&2
    exit 1
}

test -n "${XBSP:=$1}" || die "XBSP should be specified"

CMAKE_OPTS="-DXBSP=${XBSP}"
for arg in $*; do
    case $arg in
        *DEBUG|*RELEASE)
            BUILD=$2
            ;;
        STATIC_ANALYSIS)
            CMAKE_OPTS="${CMAKE_OPTS} -DSTATIC_ANALYSIS=1"
            ;;
    esac
    CMAKE_OPTS="${CMAKE_OPTS} -DCMAKE_BUILD_TYPE=${BUILD:-DEBUG}"
done

set -eu

rm -rf build/${XBSP}
mkdir -p build/${XBSP}
cd build/${XBSP}
cmake -G Ninja ../.. ${CMAKE_OPTS}
ninja
