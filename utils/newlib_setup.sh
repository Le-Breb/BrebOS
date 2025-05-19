#!/bin/bash

set -e

CYAN="\033[0;36m"
WHITE="\033[0;37m"

function cyan_echo()
{
  echo -e "$CYAN$1$WHITE"
}

num_jobs=$(nproc --all)
script_path=$(dirname "$(realpath $0)")
tools_path="$script_path/../tools"
newlib_config="$script_path/../src/newlib-config"
sysroot="$script_path/../sysroot"
newlib_build="$script_path/../newlib-build"

echo "$script_path"
cd "$script_path"

mkdir -p "$tools_path"
rm -rf "$tools_path"/*
mkdir -p "$sysroot"
rm -rf "$sysroot"/*
mkdir -p "$newlib_build"
rm -rf "$newlib_build"/*

cd ../tools

cyan_echo "Downloading automake and autoconf"
wget https://ftp.gnu.org/gnu/automake/automake-1.11.tar.gz
wget https://ftp.gnu.org/gnu/autoconf/autoconf-2.64.tar.gz
wget https://ftp.gnu.org/gnu/autoconf/autoconf-2.65.tar.gz

cyan_echo "Extracting"
tar -xf automake-1.11.tar.gz
tar -xf autoconf-2.64.tar.gz
tar -xf autoconf-2.65.tar.gz

mkdir bin-65
mkdir bin-64

cyan_echo "Building automake and autoconf"
mkdir build
cd build
./../automake-1.11/configure --prefix="$(pwd)"/../bin-65
make -j "$num_jobs" && make install
./../autoconf-2.65/configure --prefix="$(pwd)"/../bin-65
make -j "$num_jobs" && make install

rm -rf ./*
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

cd "$newlib_build"
"$tools_path"/newlib-2.5.0/configure --prefix=/usr --target=i686-brebos
make all -j "$num_jobs"
make DESTDIR="$sysroot" install

rm -rf "$tools_path"