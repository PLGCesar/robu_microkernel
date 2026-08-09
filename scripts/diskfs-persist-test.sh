#!/bin/sh
# Two-boot proof that apps/diskfs/diskfs.c actually persists data to real
# storage, not just within a single boot's in-memory file-table cache:
# boot once against a fresh disk image, create a file via the shell,
# cleanly shut the VM down (test_exit=, which drives arch_test_exit() ->
# isa-debug-exit -> a real process exit, so the block layer flushes
# normally -- unlike SIGKILL/pkill, which this script deliberately avoids),
# then boot again against the SAME image file and confirm the file is
# still there. `make run` boots via a GRUB/Multiboot2 ISO (built through a
# throwaway Docker container) rather than a direct -kernel/-initrd boot,
# and shell startup under it is noticeably slower (PS/2 + ANSI VGA driver
# init adds real page-fault volume) -- timings below are generous for that.
#
# Uses `touch` + `ls`, not `echo ... > file` -- minibox-shell's own `>`
# redirection was unreliable to drive non-interactively during this same
# investigation (a pre-existing shell quirk, unrelated to diskfs), while
# `touch`/`ls` round-tripped reliably every time.
set -e
cd "$(dirname "$0")/.."

IMG=$(mktemp /tmp/diskfs-persist-XXXXXX.img)
LOG1=$(mktemp /tmp/diskfs-persist-boot1-XXXXXX.log)
LOG2=$(mktemp /tmp/diskfs-persist-boot2-XXXXXX.log)
trap 'rm -f "$IMG" "$LOG1" "$LOG2"' EXIT

truncate -s 16M "$IMG"

# `make run` now boots via a GRUB ISO built through a throwaway Docker
# container (see Makefile) instead of a direct -kernel/-initrd boot -- a
# fixed pre-command sleep can no longer distinguish "still building" from
# "still booting", and the two take wildly different amounts of time (the
# ISO step alone can take anywhere from ~0s, if already up to date, to
# ~1 minute on a cold Docker pull). Building first, untimed, means the
# piped `make run` below only ever has to wait out actual boot + shell
# startup, which is comparatively predictable.
make build/robu_kernel.iso > /dev/null 2>&1

echo "=== boot 1: creating /mnt/disk0/persist.txt ==="
{ sleep 45; printf 'touch /mnt/disk0/persist.txt\n'; sleep 8; } | \
    make run QEMU_DISK="$IMG" \
        QEMU_APPEND="root=root_task starter=hello_initsys test_exit=0 test_exit_delay=35" \
        > "$LOG1" 2>&1 || true

if ! grep -q 'touch /mnt/disk0/persist.txt' "$LOG1"; then
    echo "FAIL: boot 1 never appears to have received the touch command (see $LOG1)"
    cat "$LOG1"
    exit 1
fi
if ! grep -q '\[boot\] exiting with code 0' "$LOG1"; then
    echo "FAIL: boot 1 did not exit cleanly via isa-debug-exit (see $LOG1)"
    exit 1
fi
echo "DISKFS_WROTE_OK"

echo "=== boot 2: verifying /mnt/disk0/persist.txt survived ==="
{ sleep 45; printf 'ls /mnt/disk0\n'; sleep 8; } | \
    make run QEMU_DISK="$IMG" \
        QEMU_APPEND="root=root_task starter=hello_initsys test_exit=0 test_exit_delay=35" \
        > "$LOG2" 2>&1 || true

if ! grep -q '\[boot\] exiting with code 0' "$LOG2"; then
    echo "FAIL: boot 2 did not exit cleanly via isa-debug-exit (see $LOG2)"
    exit 1
fi
if ! grep -q 'persist\.txt' "$LOG2"; then
    echo "FAIL: persist.txt not found in boot 2's \`ls /mnt/disk0\` output (see $LOG2)"
    exit 1
fi
echo "DISKFS_VERIFY_OK"
echo "=== PASS: diskfs persisted a file across two clean boots ==="
