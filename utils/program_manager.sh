#!/bin/sh

# Written with Claude Code

set -e

SCRIPT_DIR=$(unset CDPATH && cd -- "$(dirname -- "$0")" && pwd)
BREBOS=$(realpath "$SCRIPT_DIR/..")
PROGRAMS_DIR="$BREBOS/src/programs"

usage() {
  echo "Usage: $0 add|remove <program_name>"
}

if [ $# -ne 2 ]; then
  usage
  exit 1
fi

ACTION=$1
NAME=$2
TARGET_DIR="$PROGRAMS_DIR/$NAME"

case "$ACTION" in
  add)
    if [ -d "$TARGET_DIR" ]; then
      echo "$TARGET_DIR already exists"
      exit 2
    fi
    mkdir -p "$TARGET_DIR"
    cp "$PROGRAMS_DIR/program_Makefile" "$TARGET_DIR/Makefile"
    cat > "$TARGET_DIR/main.cpp" << 'EOF'
int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[])
{
    return 0;
}
EOF
    echo "Successfully created and configured $TARGET_DIR"
    ;;
  remove)
    if [ ! -d "$TARGET_DIR" ]; then
      echo "$TARGET_DIR does not exist"
      exit 2
    fi
    rm -r -- "$TARGET_DIR"
    echo "Successfully removed $TARGET_DIR"
    ;;
  *)
    usage
    exit 1
    ;;
esac