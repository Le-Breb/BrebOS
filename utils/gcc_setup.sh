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
toolchain_path="$brebos_path/toolchain"
sysroot="$brebos_path/sysroot"
binutils_build_path="$tools_path/binutils-build"
gcc_build_path="$tools_path/gcc-build"

mkdir -p "$tools_path"
mkdir -p "$sysroot"
mkdir -p "$gcc_build_path"
mkdir -p "$binutils_build_path"
mkdir -p "$toolchain_path/gcc"

cd "$tools_path"

export PREFIX="$toolchain_path/gcc"
export TARGET=i686-elf
export PATH="$PREFIX/bin:$PATH"

cyan_echo "Downloading binutils"
wget https://ftp.gnu.org/gnu/binutils/binutils-2.44.tar.xz

cyan_echo "Extracting binutils"
tar xf binutils-2.44.tar.xz

cyan_echo "Building binutils"
cd "$binutils_build_path"
../binutils-2.44/configure --target=$TARGET --prefix="$PREFIX" --with-sysroot --disable-nls --disable-werror
make -j "$num_jobs"
make install

cd  "$tools_path"

cyan_echo "Downloading GCC"
wget https://ftp.gnu.org/gnu/gcc/gcc-15.1.0/gcc-15.1.0.tar.gz

cyan_echo "Extracting GCC"
tar xf gcc-15.1.0.tar.gz

cyan_echo "Building GCC"
cd "$gcc_build_path"
../gcc-15.1.0/configure --target=$TARGET --prefix="$PREFIX" --disable-nls --enable-languages=c,c++ --without-headers --disable-hosted-libstdcxx
make all-gcc -j "$num_jobs"
make all-target-libgcc -j "$num_jobs"
make all-target-libstdc++-v3 -j "$num_jobs"
make install-gcc
make install-target-libgcc
make install-target-libstdc++-v3

cd "$PREFIX/bin/"

ln i686-elf-ar i686-brebos-ar
ln i686-elf-as i686-brebos-as
ln i686-elf-gcc i686-brebos-gcc
ln i686-elf-gcc i686-brebos-cc
ln i686-elf-ranlib i686-brebos-ranlib
