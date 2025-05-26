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
toolchain_path="$brebos_path/toolchain_"
sysroot="$brebos_path/sysroot"
binutils_build_path="$tools_path/binutils-build"
gcc_build_path="$tools_path/gcc-build"
binutils_config="$brebos_path/src/binutils-config"
gcc_config="$brebos_path/src/gcc-config"

export PREFIX="$toolchain_path/gcc"
export TARGET=i686-brebos
export PATH="$PREFIX/bin:$PATH"

echo "${PREFIX:?}"
rm -r "${PREFIX:?}"/* # Clear previous toolchain

cyan_echo "Downloading automake and autoconf"
cd "$tools_path"
wget https://ftp.gnu.org/gnu/automake/automake-1.15.1.tar.gz https://ftp.gnu.org/gnu/autoconf/autoconf-2.69.tar.gz

cyan_echo "Extracting"
tar -xf automake-1.15.1.tar.gz
tar -xf autoconf-2.69.tar.gz

mkdir -p bin-69

cyan_echo "Building automake and autoconf"
mkdir -p build-69
cd build-69
./../automake-1.15.1/configure --prefix="$(pwd)"/../bin-69
make -j "$num_jobs" && make install
./../autoconf-2.69/configure --prefix="$(pwd)"/../bin-69
make -j "$num_jobs" && make install

export PATH="$tools_path"/bin-69/bin/:$PATH

cyan_echo "Configuring binutils for Brebos"
cd "$tools_path/binutils-2.44"
cp -r "$binutils_config"/. ./
#cp "$binutils_config"/config.sub ./
#cp "$binutils_config"/bfd/config.bfd ./bfd/
#cp "$binutils_config"/gas/congigure.tgt ./gas/
#cp "$binutils_config"/ld/configure.tgt ./ld/
#cp "$binutils_config"/ld/Makefile.am ./ld/
#cp "$binutils_config"/ld/emulparams/elf_i386_brebos.sh ./ld/emulparams/
#cp "$binutils_config"/ld/emulparams/elf_x86_64_brebos.sh ./ld/emulparams/
cd ld
automake

cyan_echo "Building binutils for Brebos"
cd "$binutils_build_path"
rm -rf ./*
../binutils-2.44/configure --target="$TARGET" --prefix="$PREFIX" --with-sysroot="$sysroot"  --disable-werror --enable-shared
make -j "$num_jobs"
make install


cyan_echo "Configuring GCC for Brebos"
cd "$tools_path/gcc-15.1.0"
cp -r "$gcc_config/." ./
#cp "$gcc_config/fixincludes/mkfixinc.sh" ./fixincludes/
#cp "$gcc_config/config.sub" ./
#cp "$gcc_config/libstdc++-v3/crossconfig.m4" ./libstdc++-v3/
#cp "$gcc_config/libgcc/config.host" ./libgcc/
#cp "$gcc_config/gcc/config/brebos.h" ./gcc/config/
cd libstdc++-v3
autoconf

cyan_echo "Building GCC for Brebos"
cd "$gcc_build_path"
rm -rf ./*
../gcc-15.1.0/configure --target="$TARGET" --prefix="$PREFIX" --with-sysroot="$sysroot" --enable-languages=c,c++ --enable-shared
make all-gcc -j "$num_jobs"
make all-target-libgcc -j "$num_jobs"
make install-gcc install-target-libgcc

# libstdc++-v3 not being built because not used for now and missing dlfcn header
#make all-target-libstdc++-v3 -j "$num_jobs"
#make install-target-libstdc++-v3