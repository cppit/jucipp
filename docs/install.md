# juCi++ Installation Guide

- Installation
  - Linux
    - [Debian testing/Linux Mint/Ubuntu](#debian-testinglinux-mintubuntu)
    - [Debian stable](#debian-stable)
    - [Arch Linux](#arch-linux)
  - OS X
    - [Homebrew](#os-x-with-homebrew-httpbrewsh)
  - Windows
    - [MSYS 2](#windows-with-msys2-httpsmsys2githubio)
- [Run](#run)

## Debian testing/Linux Mint/Ubuntu
Install dependencies:
```sh
sudo apt-get install git cmake make g++ libclang-3.6-dev liblldb-3.6-dev clang-format-3.6 pkg-config libboost-system-dev libboost-thread-dev libboost-filesystem-dev libboost-log-dev libboost-regex-dev libgtksourceviewmm-3.0-dev aspell-en libaspell-dev
```

Get juCi++ source, compile and install:
```sh
git clone --recursive https://github.com/cppit/jucipp
mkdir jucipp/build
cd jucipp/build
cmake ..
make
sudo make install
```

## Debian stable
Install dependencies:
```sh
sudo apt-get install git cmake make g++ libclang-3.5-dev liblldb-3.5-dev clang-format-3.5 pkg-config libboost-system-dev libboost-thread-dev libboost-filesystem-dev libboost-log-dev libboost-regex-dev libgtksourceviewmm-3.0-dev aspell-en libaspell-dev
```

Get juCi++ source, compile and install:
```sh
git clone --recursive https://github.com/cppit/jucipp
mkdir jucipp/build
cd jucipp/build
cmake ..
make
sudo make install
```

##Arch Linux
**Arch Linux's lldb package has an issue that is being worked on, and for the time being you have to build juCi++ without debug support. If you have the lldb package installed, please remove this package before building juCi++.**

Package available in the Arch User Repository:
https://aur.archlinux.org/packages/jucipp-git/

Alternatively, follow the instructions below.

Install dependencies:
```sh
sudo pacman -S git cmake make clang gtksourceviewmm boost aspell aspell-en
```

Get juCi++ source, compile and install:
```sh
git clone --recursive https://github.com/cppit/jucipp
mkdir jucipp/build
cd jucipp/build
cmake ..
make
sudo make install
```

## OS X with Homebrew (http://brew.sh/)
**Installing llvm may take some time, and you need to follow the lldb code signing instructions. For an easier dependency install, but without debug support, remove `--with-lldb` below.**

Install dependencies:
```sh
brew install --with-clang --with-lldb llvm
brew install cmake pkg-config boost homebrew/x11/gtksourceviewmm3 aspell clang-format
```

Get juCi++ source, compile and install:
```sh
git clone --recursive https://github.com/cppit/jucipp
mkdir jucipp/build
cd jucipp/build
cmake ..
make
make install
```

##Windows with MSYS2 (https://msys2.github.io/)
**MSYS2 does not yet support lldb, but you can still compile juCi++ without debug support. Also for the time being, MSYS2 must be installed in the default MSYS2 folder (C:\msys64 or C:\msys32).**

Note that juCi++ must be run in a MinGW Shell (for instance MinGW-w64 Win64 Shell). 

Install dependencies (replace `x86_64` with `i686` for 32-bit MSYS2 installs):
```sh
pacman -S git mingw-w64-x86_64-cmake make mingw-w64-x86_64-toolchain mingw-w64-x86_64-clang mingw-w64-x86_64-gtkmm3 mingw-w64-x86_64-gtksourceviewmm3 mingw-w64-x86_64-boost mingw-w64-x86_64-aspell mingw-w64-x86_64-aspell-en
```

Get juCi++ source, compile and install (replace `mingw64` with `mingw32` for 32-bit MSYS2 installs):
```sh
git clone --recursive https://github.com/cppit/jucipp
mkdir jucipp/build
cd jucipp/build
cmake -G"MSYS Makefiles" -DCMAKE_INSTALL_PREFIX=/mingw64 ..
make
make install
```

## Run
```sh
juci
```
Alternatively, you can also include directories and files:
```sh
juci [directory] [file1 file2 ...]
```
