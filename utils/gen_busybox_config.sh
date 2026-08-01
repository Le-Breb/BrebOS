#!/bin/sh

# Written with Claude Code

# Regenerates busybox/.config so that exactly the applets listed in
# src/busybox_config/programs.list are enabled ("busybox coreutils" and any
# other busybox applet), leaving every other setting untouched. The shell
# (sh/ash) is always kept on and is not controlled by programs.list.
#
# This is the same sed + "make oldconfig" technique busybox's own
# make_single_applets.sh uses to toggle one applet at a time, just applied to
# a whole list at once.

set -e

SCRIPT_DIR=$(unset CDPATH && cd -- "$(dirname -- "$0")" && pwd)
BREBOS=$(realpath "$SCRIPT_DIR/..")
BUSYBOX_DIR="$BREBOS/busybox"
PROGRAMS_LIST="$BREBOS/src/busybox_config/programs.list"
BASELINE_CONFIG="$BREBOS/src/busybox_config/.config"

[ -d "$BUSYBOX_DIR" ] || {
  echo "gen_busybox_config.sh: $BUSYBOX_DIR not found (run utils/toolchain_setup.sh first)" >&2
  exit 1
}

# Seed busybox/.config with the tracked baseline (non-applet settings: build
# as individual binaries, shell selection, etc.) the first time around.
[ -f "$BUSYBOX_DIR/.config" ] || cp "$BASELINE_CONFIG" "$BUSYBOX_DIR/.config"

cd "$BUSYBOX_DIR"

# Universe of all pluggable busybox applet config symbols, taken straight
# from the "//applet:IF_<SYMBOL>(...)" annotations in the source tree. Shell
# selection (ash/hush) is excluded on purpose: it's fixed in .config and not
# controlled by programs.list.
all_apps=$(grep -rhoP '^//applet:IF_\K[A-Z0-9_]+(?=\()' --include='*.c' . |
  grep -vE '^(ASH|HUSH|SH_IS_ASH|SH_IS_HUSH|BASH_IS_ASH|BASH_IS_HUSH)$' |
  sort -u)

requested=$(sed 's/#.*//' "$PROGRAMS_LIST" | tr -s '[:space:]' '\n' | grep -v '^$' | tr '[:lower:]' '[:upper:]')

cfg=$(cat .config)

# Disable every known applet first.
for app in $all_apps; do
  cfg=$(printf '%s\n' "$cfg" | sed "s/^CONFIG_${app}=y\$/# CONFIG_${app} is not set/")
done

# Enable the requested ones.
for app in $requested; do
  if ! printf '%s\n' "$all_apps" | grep -qx "$app"; then
    echo "gen_busybox_config.sh: warning: unknown busybox applet '$(echo "$app" | tr '[:upper:]' '[:lower:]')' in programs.list, skipping" >&2
    continue
  fi
  cfg=$(printf '%s\n' "$cfg" | sed "/^# CONFIG_${app} is not set\$/d")
  cfg="$cfg
CONFIG_${app}=y"
done

printf '%s\n' "$cfg" >.config

yes '' | make ARCH=i386 CROSS_COMPILE=i686-brebos- CC=i686-brebos-gcc oldconfig >/dev/null
