#!/bin/sh

set -e

CYAN="\033[0;36m"
WHITE="\033[0;37m"

cyan_echo()
{
  echo "$CYAN$1$WHITE"
}

NUM_JOBS=$(nproc --all)
SCRIPT_PATH=$(dirname "$0")
BREBOS=$(realpath "$SCRIPT_PATH"/..)
MLIBC_CONFIG="$BREBOS"/src/mlibc-config
SYSROOT_DIR="$BREBOS"/sysroot
TOOLCHAIN_SOURCES_DIR="$BREBOS/toolchain_sources"
TOOLCHAIN_DIR="$BREBOS"/toolchain

mkdir -p "$SYSROOT_DIR" "$TOOLCHAIN_SOURCES_DIR" "$TOOLCHAIN_DIR"


mlibc_config()
{
    cyan_echo "mlibc configuration"
    cd "$BREBOS"

    git clone https://github.com/managarm/mlibc
    cd mlibc
    cp "$MLIBC_CONFIG"/brebos-cross.txt ./
    ln -s "$MLIBC_CONFIG"/sysdeps/brebos ./sysdeps/brebos
    ln -s "$(pwd)"/sysdeps/linux/include/abi-bits ./sysdeps/brebos/include/abi-bits
    sed -i \
            -e "s/host_machine.system() == 'demo'/host_machine.system() == 'brebos'/g" \
            -e 's/sysdeps\/demo/sysdeps\/brebos/g' \
            -e 's/demo-sysdeps/brebos-sysdeps/g' \
            meson.build
}

mlibc_first_headers_install()
{
    cyan_echo "mlibc first headers install"
    cd "$BREBOS"/mlibc

    meson \
              setup \
              --cross-file=brebos-cross.txt \
              --prefix=/usr \
              -Dheaders_only=true \
              headers-build
    DESTDIR=${SYSROOT_DIR} ninja -C headers-build install
}

binutils_setup_and_build()
{
    cyan_echo "binutils setup and build"
    cd "$TOOLCHAIN_SOURCES_DIR"

    wget https://ftp.gnu.org/gnu/binutils/binutils-2.45.1.tar.xz
    tar xf binutils-2.45.1.tar.xz
    cd binutils-2.45.1
    cp -r "$BREBOS"/src/binutils-config/* ./
    mkdir build
    cd build
    ../configure \
          --target=i686-brebos \
          --prefix=/usr \
          --with-sysroot="$SYSROOT_DIR" \
          --disable-werror \
          --enable-default-execstack=no \
          --enable-shared
    make -j"$NUM_JOBS"
    DESTDIR="$TOOLCHAIN_DIR" make install
}

autoconf_setup_and_build()
{
    cyan_echo "autoconf setup and build"
    cd "$TOOLCHAIN_SOURCES_DIR"

    wget https://ftp.gnu.org/gnu/autoconf/autoconf-2.69.tar.gz
    tar -xf autoconf-2.69.tar.gz

    cd autoconf-2.69/
    mkdir build && cd build
    ../configure --prefix="$(pwd)/../install"
    make -j"$(NUM_JOBS)"
    make install
}

gcc_setup_and_build()
{
    cyan_echo "GCC setup and build"
    cd "$TOOLCHAIN_SOURCES_DIR"

    export LD_LIBRARY_PATH="$TOOLCHAIN_DIR/usr/x86_64-pc-linux-gnu/i686-brebos/lib:${LD_LIBRARY_PATH}"
    export PATH="$TOOLCHAIN_DIR/usr/bin:${PATH}"
    export LD_LIBRARY_PATH="$TOOLCHAIN_DIR/usr/lib:${LD_LIBRARY_PATH}"
    
    wget https://ftp.gnu.org/gnu/gcc/gcc-15.1.0/gcc-15.1.0.tar.gz

    cyan_echo "Extracting GCC"
    tar xf gcc-15.1.0.tar.gz
    
    cyan_echo "Configuring GCC"
    cd ./gcc-15.1.0
    cp -r "$BREBOS"/src/gcc-config/. ./

    cyan_echo "Building GCC"
    mkdir build && cd build
    ../configure \
        --target=i686-brebos \
        --prefix=/usr \
        --with-sysroot="$SYSROOT_DIR" \
        --enable-languages=c,c++ \
        --enable-shared \
        --enable-host-shared \
        --disable-multilib
    make all-gcc -j "$NUM_JOBS"
    make all-target-libgcc -j "$NUM_JOBS"
    DESTDIR="$TOOLCHAIN_DIR" make install-gcc install-target-libgcc -j"$NUM_JOBS"
}

libstdcpp_build()
{
    cyan_echo "libstdc++-v3 build"
    cd "$TOOLCHAIN_SOURCES_DIR"

    export LD_LIBRARY_PATH="$TOOLCHAIN_DIR/usr/x86_64-pc-linux-gnu/i686-brebos/lib:${LD_LIBRARY_PATH}"
    export PATH="$TOOLCHAIN_DIR/usr/bin:${PATH}"
    export LD_LIBRARY_PATH="$TOOLCHAIN_DIR/usr/lib:${LD_LIBRARY_PATH}"
    export PATH="$TOOLCHAIN_SOURCES_DIR/autoconf-2.69/install/bin:$PATH"

    cd ./gcc-15.1.0/libstdc++-v3
    autoconf
    cd ../build

    make all-target-libstdc++-v3 -j "$NUM_JOBS"
    DESTDIR="$TOOLCHAIN_DIR" make install-target-libstdc++-v3

    # Strip debug symbols out of libstdc++, otherwise it produces massive ELFs when linked against
    cp "$TOOLCHAIN_DIR/usr/i686-brebos/lib/libstdc++.a" "$TOOLCHAIN_DIR/usr/i686-brebos/lib/libstdc++.a.dbg"
    i686-brebos-strip "$TOOLCHAIN_DIR/usr/i686-brebos/lib/libstdc++.a" --strip-debug
}

mlibc_build()
{
    cyan_echo "mlibc build"
    cd "$BREBOS"/mlibc

    meson \
                setup \
                --buildtype=release \
                --cross-file=brebos-cross.txt \
                --prefix=/usr \
                -Ddefault_library=both \
                -Dno_headers=true \
                build
    DESTDIR="$SYSROOT_DIR" ninja -C build install
}

mlibc_config
mlibc_first_headers_install
binutils_setup_and_build
gcc_setup_and_build
mlibc_build
autoconf_setup_and_build
libstdcpp_build
