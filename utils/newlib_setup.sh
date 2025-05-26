#!/bin/bash

set -e

CYAN="\033[0;36m"
WHITE="\033[0;37m"

function cyan_echo()
{
  echo -e "$CYAN$1$WHITE"
}

num_jobs=$(nproc --all)
script_path=$(dirname "$(realpath "$0")")
brebos_path="$script_path/.."
tools_path="$brebos_path/tools"
newlib_config="$brebos_path/src/newlib-config"
sysroot="$brebos_path/sysroot"
newlib_build_path="$tools_path/newlib-build"
toolchain_path="$brebos_path/toolchain_"

export PREFIX="$toolchain_path/gcc"
export TARGET=i686-brebos
export PATH="$PREFIX/bin:$PATH"

mkdir -p "$tools_path"
mkdir -p "$sysroot"
mkdir -p "$newlib_build_path"

cd "$tools_path"

cyan_echo "Downloading automake and autoconf"
wget https://ftp.gnu.org/gnu/automake/automake-1.11.tar.gz https://ftp.gnu.org/gnu/autoconf/autoconf-2.64.tar.gz \
https://ftp.gnu.org/gnu/autoconf/autoconf-2.65.tar.gz

cyan_echo "Extracting"
tar -xf automake-1.11.tar.gz
tar -xf autoconf-2.64.tar.gz
tar -xf autoconf-2.65.tar.gz

cyan_echo "Building automake and autoconf"
mkdir -p build-65
cd build-65
./../automake-1.11/configure --prefix="$(pwd)"/../bin-65
make -j "$num_jobs" && make install
./../autoconf-2.65/configure --prefix="$(pwd)"/../bin-65
make -j "$num_jobs" && make install

cd ..
mkdir -p build-64
cd build-64
./../automake-1.11/configure --prefix="$(pwd)"/../bin-64
make -j "$num_jobs" && make install
./../autoconf-2.65/configure --prefix="$(pwd)"/../bin-64
make -j "$num_jobs" && make install

cyan_echo "Downloading newlib"
cd "$tools_path"
wget ftp://sourceware.org/pub/newlib/newlib-2.5.0.tar.gz
tar -xf newlib-2.5.0.tar.gz

cyan_echo "Configuring newlib"
cd newlib-2.5.0
cp "$newlib_config"/config.sub ./
cp "$newlib_config"/newlib/configure.host ./newlib/
cp "$newlib_config"/newlib/libc/sys/configure.in ./newlib/libc/sys

cd newlib/libc/sys
export PATH="$tools_path"/bin-64/bin/:$PATH
autoconf
export PATH="$tools_path"/bin-65/bin/:$PATH

cp -r "$newlib_config"/newlib/libc/sys/brebos ./
autoconf
cd ./brebos
autoreconf

cyan_echo "Building newlib"
cd "$newlib_build_path"
"$tools_path"/newlib-2.5.0/configure --prefix=/usr --target=$TARGET
make all -j "$num_jobs"
make DESTDIR="$sysroot" install

cyan_echo "Last details"
cp -ar "${sysroot:?}"/usr/"$TARGET"/* "$sysroot"/usr/ # Move everything to /usr so that gcc and binutils can find it
rm -rf "${sysroot:?}"/usr/"$TARGET" # Remove the empty directory
# Replace the dummy crt0 that newlib built out of the dummy ctr0.c i provided with the right one, compiled from asm
nasm -f elf -F dwarf -O0 -g -o "$sysroot/usr/lib/crt0.o" "$newlib_config/../../src/programs/start_program.s"
