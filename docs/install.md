# juCi++ Installation Guide

- Installation
  - Linux
    - [Debian testing/Linux Mint/Ubuntu](#debian-testinglinux-mintubuntu)
    - [Debian stable/Linux Mint Debian Edition/Raspbian](#debian-stablelinux-mint-debian-editionraspbian)
    - [Arch Linux/Manjaro Linux](#arch-linuxmanjaro-linux)
    - [Fedora 23](#fedora-23)
    - [Mageia 5](#mageia-5)
  - OS X
    - [Homebrew](#os-x-with-homebrew-httpbrewsh)
  - Windows
    - [MSYS 2](#windows-with-msys2-httpsmsys2githubio)
- [Run](#run)

## Debian testing/Linux Mint/Ubuntu
Install dependencies:
```sh
sudo apt-get install git cmake make g++ libclang-3.6-dev liblldb-3.6-dev clang-format-3.6 pkg-config libboost-filesystem-dev libboost-regex-dev libgtksourceviewmm-3.0-dev aspell-en libaspell-dev
```

Get juCi++ source, compile and install:
```sh
git clone --recursive https://github.com/cppit/jucipp
mkdir jucipp/build
cd jucipp/build
cmake -DCMAKE_CXX_COMPILER=g++ ..
make
sudo make install
```

## Debian stable/Linux Mint Debian Edition/Raspbian
Install dependencies:
```sh
sudo apt-get install git cmake make g++ libclang-3.5-dev liblldb-3.5-dev clang-format-3.5 pkg-config libboost-filesystem-dev libboost-regex-dev libgtksourceviewmm-3.0-dev aspell-en libaspell-dev
```

Get juCi++ source, compile and install:
```sh
git clone --recursive https://github.com/cppit/jucipp
mkdir jucipp/build
cd jucipp/build
cmake -DCMAKE_CXX_COMPILER=g++ ..
make
sudo make install
```

##Arch Linux/Manjaro Linux
**Arch Linux's lldb package has an issue that is being worked on (see https://github.com/cppit/jucipp/issues/191), and for the time being you have to build juCi++ without debug support. If you have the lldb package installed, please remove this package before building juCi++.**

Package available in the Arch User Repository:
https://aur.archlinux.org/packages/jucipp-git/

If you have the arch package [yaourt](https://archlinux.fr/yaourt-en) installed:
```sh
yaourt -S jucipp-git
```


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

## Fedora 23
Install dependencies:
```sh
sudo dnf install git cmake make gcc-c++ clang-devel clang lldb-devel boost-devel gtksourceviewmm3-devel gtkmm30-devel aspell-devel aspell-en
```

Get juCi++ source, compile and install:
```sh
git clone --recursive https://github.com/cppit/jucipp
mkdir jucipp/build
cd jucipp/build
cmake -DCMAKE_CXX_COMPILER=g++ ..
make
sudo make install
```

## Mageia 5

**Mageia doesn't support LLDB, but you can compile without debug support.**

Install dependencies:

32-bit:

```sh
sudo urpmi git cmake make gcc-c++ clang libclang-devel libboost-devel libgtkmm3.0-devel libgtksourceviewmm3.0-devel libaspell-devel aspell-en
```

64-bit:
```sh
sudo urpmi git cmake make gcc-c++ clang lib64clang-devel lib64boost-devel lib64gtkmm3.0-devel lib64gtksourceviewmm3.0-devel lib64aspell-devel aspell-en
```

Get juCi++ source, compile and install:
```sh
git clone --recursive https://github.com/cppit/jucipp
mkdir jucipp/build
cd jucipp/build
cmake -DCMAKE_CXX_COMPILER=g++ ..
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
**MSYS2 does not yet support lldb (see https://github.com/cppit/jucipp/issues/190), but you can still compile juCi++ without debug support.**

Install dependencies (replace `x86_64` with `i686` for 32-bit MSYS2 installs):
```sh
pacman -S git mingw-w64-x86_64-cmake make mingw-w64-x86_64-toolchain mingw-w64-x86_64-clang mingw-w64-x86_64-gtkmm3 mingw-w64-x86_64-gtksourceviewmm3 mingw-w64-x86_64-boost mingw-w64-x86_64-aspell mingw-w64-x86_64-aspell-en
```

Note that juCi++ must be built and run in a MinGW Shell (for instance MinGW-w64 Win64 Shell).

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
