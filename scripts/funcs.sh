if [ "$(uname -s)" = "Darwin" ]; then
_xecho () {
    if [ "$1" = "-n" -o  "$1" = "-ne" ]; then
        shift
        EOL=""
    else
        EOL="\n"
    fi
    printf -- "$*${EOL}"
}
else
_xecho () {
    echo $*
}
fi

info () {
    _xecho -ne "\033[36m"
    if [ "$1" = "-n" ]; then
        shift
        _xecho -n "$*"
    else
        _xecho "$*"
    fi
    _xecho -ne "\033[0m"
}

warning () {
    _xecho -ne "\033[33;1m"
    if [ "$1" = "-n" ]; then
        shift
        _xecho -n "$*"
    else
        _xecho "$*"
    fi
    _xecho -ne "\033[0m"
}

error () {
    _xecho -ne "\033[31;1m"
    if [ "$1" = "-n" ]; then
        shift
        _xecho -n "$*"
    else
        _xecho "$*"
    fi
    _xecho -ne "\033[0m"
}

# Die with an error message
die() {
    error "$*" >&2
    exit 1
}

# Print the absolute path
abspath() {
    echo "$(cd "$(dirname "$1")"; pwd)/$(basename "$1")"
}

if [ -z "${GITHUB_ENV}" -a -z "${CI}" ]; then
    # when the scripts are run outside a GitHub Actions context,
    # this value is not defined
    export GITHUB_ENV=$(mktemp)
    echo "GITHUB_ENV ${GITHUB_ENV} GITHUB_CI ${GITHUB_CI}"
fi
