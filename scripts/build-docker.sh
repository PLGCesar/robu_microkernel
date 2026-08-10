#!/bin/bash
set -e
cd "$(dirname "$0")/.."

if command -v docker >/dev/null 2>&1; then
    ENGINE=docker
    echo "Trying Docker..."
elif command -v podman >/dev/null 2>&1; then
    ENGINE=podman
    echo "Docker not found. Trying Podman..." 
else
    echo "build-docker.sh: neither docker nor podman found on PATH" >&2
    exit 1
fi

"$ENGINE" run --rm --platform=linux/amd64 -v "$(pwd)":/work -w /work ubuntu:24.04 \
    bash -c '
        set -e
        apt-get update -qq
        apt-get install -y -qq --no-install-recommends \
            clang lld llvm make flex bison mtools xorriso grub-pc-bin grub-common \
            qemu-system-x86 meson libc++-dev libc++abi-dev git >/dev/null
        git config --global --add safe.directory /work
        make BUILD_SYS=clang "$@"
    ' _ "$@"
