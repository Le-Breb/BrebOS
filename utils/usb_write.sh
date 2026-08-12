#!/usr/bin/env bash

# Written by Claude Code

# Writes os.iso to a USB stick, with device auto-detection/confirmation and a post-write verification
# so silent "wrong device" / "incomplete write" failures don't masquerade as "the OS didn't update".
set -euo pipefail

ISO="${1:-os.iso}"
DEV_ARG="${2:-}"
if [ ! -f "$ISO" ]; then
    echo "error: $ISO not found (build it first with 'make')" >&2
    exit 1
fi

# Auto-detect USB disks (whole disks only, not partitions)
mapfile -t usb_disks < <(lsblk -dn -o NAME,TRAN,SIZE,MODEL | awk '$2=="usb"{print}')

if [ "${#usb_disks[@]}" -eq 0 ]; then
    echo "error: no USB disk detected (lsblk found nothing with TRAN=usb)" >&2
    exit 1
fi

if [ -n "$DEV_ARG" ]; then
    # Explicit device given on the command line: validate it's one of the detected USB disks.
    dev_name="${DEV_ARG#/dev/}"
    match=
    for disk in "${usb_disks[@]}"; do
        if [ "$(awk '{print $1}' <<< "$disk")" = "$dev_name" ]; then
            match="$disk"
            break
        fi
    done
    if [ -z "$match" ]; then
        echo "error: $DEV_ARG is not among the detected USB disks:" >&2
        printf '  %s\n' "${usb_disks[@]}" >&2
        exit 1
    fi
elif [ "${#usb_disks[@]}" -eq 1 ]; then
    dev_name=$(awk '{print $1}' <<< "${usb_disks[0]}")
else
    echo "Multiple USB disks found:"
    for i in "${!usb_disks[@]}"; do
        printf '  [%d] %s\n' "$((i+1))" "${usb_disks[$i]}"
    done
    read -rp "Select target disk [1-${#usb_disks[@]}]: " choice
    if ! [[ "$choice" =~ ^[0-9]+$ ]] || [ "$choice" -lt 1 ] || [ "$choice" -gt "${#usb_disks[@]}" ]; then
        echo "error: invalid selection" >&2
        exit 1
    fi
    dev_name=$(awk '{print $1}' <<< "${usb_disks[$((choice-1))]}")
fi

DEV="/dev/$dev_name"

echo "Target device: $DEV"
echo "  $(lsblk -dn -o NAME,TRAN,SIZE,MODEL,SERIAL "$DEV" 2>/dev/null || true)"

# /proc/mounts reflects the live kernel mount table with no lag, unlike lsblk
# which reads the udev database and can be stale right after (re)partitioning
# -- that lag is what let the desktop auto-mounter win the race last time.
unmount_partitions() {
    local part
    for part in $(awk -v d="$DEV" '$1 ~ "^"d {print $1}' /proc/mounts); do
        echo "Unmounting $part"
        sudo umount "$part" || true
    done
}

read -rp "About to overwrite $DEV with $ISO. Type 'yes' to continue: " confirm
if [ "$confirm" != "yes" ]; then
    echo "Aborted."
    exit 1
fi

# Unmount any mounted partitions of the target device so nothing races the write
unmount_partitions

echo "Writing..."
sudo dd if="$ISO" of="$DEV" bs=4M status=progress conv=fsync
sync

# Let the kernel re-read the new partition table.
sudo partprobe "$DEV" 2>/dev/null || true
sudo udevadm settle 2>/dev/null || true

echo "Verifying..."
iso_size=$(stat -c%s "$ISO")
if ! sudo cmp -n "$iso_size" "$ISO" "$DEV"; then
    echo "error: verification failed, $DEV does not match $ISO after write" >&2
    exit 1
fi
echo "Verified: $DEV matches $ISO"

# The desktop's auto-mounter reacts to the new partition table asynchronously
# and can grab a partition (e.g. the ISO's EFI partition) even after we just
# unmounted -- retry unmount+power-off a few times to win that race.
echo "Powering off $DEV, safe to remove."
for attempt in 1 2 3 4 5; do
    unmount_partitions
    if sudo udisksctl power-off -b "$DEV" 2>/dev/null; then
        break
    fi
    if [ "$attempt" -eq 5 ]; then
        echo "warning: could not cleanly power off $DEV, falling back to eject" >&2
        sudo eject "$DEV" || true
    else
        sleep 1
    fi
done