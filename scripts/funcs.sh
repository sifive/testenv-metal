info () {
    echo -ne "\033[36m"
    echo $*
    echo -ne "\033[0m"
}

warning () {
    echo -ne "\033[33m" >&2
    echo "$*" >&2
    echo -ne "\033[0m" >&2
}

error () {
    echo -ne "\033[31m" >&2
    echo "$*" >&2
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
