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
    echo -ne "\033[33;1m"
    if [ "$1" = "-n" ]; then
        shift
        echo -n "$*"
    else
        echo "$*"
    fi
    echo -ne "\033[0m"
}

error () {
    echo -ne "\033[31;1m"
    if [ "$1" = "-n" ]; then
        shift
        echo -n "$*"
    else
        echo "$*"
    fi
    echo -ne "\033[0m"
}

# Die with an error message
die() {
    error $* >&2
    exit 1
}

# Print the absolute path
abspath() {
    echo "$(cd "$(dirname "$1")"; pwd)/$(basename "$1")"
}
