#!/bin/bash

# Toolchain Installer for Debian-like systems. If you're running
# something else, you're pretty much on your own.

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

. $DIR/util.sh

pushd "$DIR" > /dev/null

    if [ ! -d tarballs ]; then
        mkdir tarballs
    fi
    pushd tarballs > /dev/null
        $INFO "wget" "Pulling source packages..."
        grab "gcc"  "http://www.netgull.com/gcc/releases/gcc-6.4.0" "gcc-6.4.0.tar.gz" || bail
        #grab "mpc"  "http://www.multiprecision.org/mpc/download" "mpc-0.9.tar.gz"
        #grab "mpfr" "http://www.mpfr.org/mpfr-3.0.1" "mpfr-3.0.1.tar.gz"
        #grab "gmp"  "ftp://ftp.gmplib.org/pub/gmp-5.0.1" "gmp-5.0.1.tar.gz"
        grab "binutils" "http://ftp.gnu.org/gnu/binutils" "binutils-2.27.tar.gz" || bail
        grab "newlib" "http://b.dakko.us/~klange/mirrors" "newlib-1.19.0.tar.gz" || bail
        grab "freetype" "http://download.savannah.gnu.org/releases/freetype" "freetype-2.4.9.tar.gz" || bail
        grab "zlib" "http://zlib.net/fossils" "zlib-1.2.8.tar.gz" || bail
        grab "libpng" "http://b.dakko.us/~klange/mirrors" "libpng-1.5.13.tar.gz" || bail
        grab "pixman" "http://www.cairographics.org/releases" "pixman-0.26.2.tar.gz" || bail
        grab "cairo" "http://www.cairographics.org/releases" "cairo-1.12.2.tar.xz" || bail
        grab "ncurses" "http://b.dakko.us/~klange/mirrors" "ncurses-5.9.tar.gz" || bail
        grab "vim" "ftp://ftp.vim.org/pub/vim/unix" "vim-7.3.tar.bz2" || bail
        $INFO "wget" "Pulled source packages."
        rm -rf "binutils-2.27" "freetype-2.4.9" "gcc-6.4.0" "gmp-5.0.1" "libpng-1.5.13" "mpc-0.9" "mpfr-3.0.1" "newlib-1.19.0" "zlib-1.2.7" "pixman-0.28.2" "ncurses-5.9" "vim73"
        $INFO "tar"  "Decompressing..."
        deco "gcc"  "gcc-6.4.0.tar.gz" || bail
        #deco "mpc"  "mpc-0.9.tar.gz"
        #deco "mpfr" "mpfr-3.0.1.tar.gz"
        #deco "gmp"  "gmp-5.0.1.tar.gz"
        deco "binutils" "binutils-2.27.tar.gz" || bail
        deco "newlib" "newlib-1.19.0.tar.gz" || bail
        deco "freetype" "freetype-2.4.9.tar.gz" || bail
        deco "zlib" "zlib-1.2.8.tar.gz" || bail
        deco "libpng" "libpng-1.5.13.tar.gz" || bail
        deco "pixman" "pixman-0.26.2.tar.gz" || bail
        deco "cairo" "cairo-1.12.2.tar.xz" || bail
        deco "ncurses" "ncurses-5.9.tar.gz" || bail
        deco "vim" "vim-7.3.tar.bz2" || bail
        $INFO "tar"  "Decompressed source packages."
        $INFO "patch" "Patching..."
        patc "gcc"  "gcc-6.4.0" || bail
        #patc "mpc"  "mpc-0.9"
        #patc "mpfr" "mpfr-3.0.1"
        #patc "gmp"  "gmp-5.0.1"
        patc "binutils" "binutils-2.27" || bail
        patc "newlib" "newlib-1.19.0" || bail
        patc "freetype" "freetype-2.4.9" || bail
        patc "libpng" "libpng-1.5.13" || bail
        patc "pixman" "pixman-0.26.2" || bail
        patc "cairo" "cairo-1.12.2" || bail
        patc "ncurses" "ncurses-5.9" || bail
        patc "vim" "vim73" || bail
        $INFO "patch" "Patched third-party software."
        $INFO "--" "Running additional bits..."
        installNewlibStuff "newlib-1.19.0" || bail
    popd > /dev/null

    if [ ! -d build ]; then
        mkdir build
    fi
    if [ ! -d local ]; then
        mkdir local
    fi

popd > /dev/null
