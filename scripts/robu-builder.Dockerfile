FROM ubuntu:24.04

RUN apt-get update -qq && apt-get install -y -qq --no-install-recommends \
    clang lld llvm make flex bison mtools xorriso grub-pc-bin grub-common \
    qemu-system-x86 meson libc++-dev libc++abi-dev git \
    && rm -rf /var/lib/apt/lists/*
