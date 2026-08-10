#!/bin/bash
set -e

cd "$(dirname "$0")/.."

YELLOW='\033[0;33m'
BLUE='\033[0;34m'
RESET='\033[0m'

OS="$(uname -s)"
KERNEL="$(uname -r)"

if [ "$OS" = "Linux" ]; then
    printf 'OS is %b%s%b, %b%s%b Kernel Version %s\n' \
        "$YELLOW" "$OS" "$RESET" \
        "$BLUE" "$OS" "$RESET" \
        "$KERNEL"

    exec make "$@"
else
    printf 'OS is %b%s%b, %b%s%b Kernel Version %s\n' \
        "$BLUE" "$OS" "$RESET" \
        "$BLUE" "$OS" "$RESET" \
        "$KERNEL"

    exec "$(dirname "$0")/build-docker.sh" "$@"
fi