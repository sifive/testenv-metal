info () {
    echo -ne "\033[36m"
    if [ "$1" = "-n" ]; then
        shift
        echo -n "$*"
    else
        echo "$*"
    fi
    echo -ne "\033[0m"
}

warning () {
    echo -ne "\033[33;1m" >&2
    if [ "$1" = "-n" ]; then
        shift
        echo -n "$*" >&2
    else
        echo "$*" >&2
    fi
    echo -ne "\033[0m" >&2
}

error () {
    echo -ne "\033[31;1m" >&2
    if [ "$1" = "-n" ]; then
        shift
        echo -n "$*" >&2
    else
        echo "$*" >&2
    fi
    echo -ne "\033[0m" >&2
}

# Die with an error message
die() {
    error $*
    exit 1
}

# Print the absolute path
abspath() {
    echo "$(cd "$(dirname "$1")"; pwd)/$(basename "$1")"
}
