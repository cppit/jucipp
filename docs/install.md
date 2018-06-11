# juCi++ Installation Guide

- Installation
  - Linux
    - [Debian/Linux Mint/Ubuntu](#debianlinux-mintubuntu)
    - [Arch Linux/Manjaro Linux](#arch-linuxmanjaro-linux)
    - [Fedora](#fedora)
    - [Mageia](#mageia)
    - [OpenSUSE Tumbleweed](#opensuse-tumbleweed)
  - [FreeBSD](#freebsd)
  - MacOS
    - [Homebrew](#macos-with-homebrew-httpbrewsh)
  - Windows
    - [MSYS 2](#windows-with-msys2-httpsmsys2githubio)
- [Run](#run)

## Debian/Linux Mint/Ubuntu
Install dependencies:
```sh
sudo apt-get install libclang-4.0-dev liblldb-4.0-dev || sudo apt-get install libclang-3.8-dev liblldb-3.8-dev
sudo apt-get install git cmake make g++ clang-format pkg-config libboost-filesystem-dev libboost-serialization-dev libgtksourceviewmm-3.0-dev aspell-en libaspell-dev libgit2-dev exuberant-ctags
```

Get juCi++ source, compile and install:
```sh
git clone --recursive https://gitlab.com/cppit/jucipp
mkdir jucipp/build
cd jucipp/build
cmake -DCMAKE_CXX_COMPILER=g++ ..
make
sudo make install
```

## Arch Linux/Manjaro Linux
Package available in the Arch User Repository:
https://aur.archlinux.org/packages/jucipp-git/

If you have the arch package [yaourt](https://archlinux.fr/yaourt-en) installed:
```sh
yaourt -S jucipp-git
```

Alternatively, follow the instructions below.

Install dependencies:
```sh
sudo pacman -S git cmake pkg-config make clang lldb gtksourceviewmm boost aspell aspell-en libgit2 ctags
```

Get juCi++ source, compile and install:
```sh
git clone --recursive https://gitlab.com/cppit/jucipp
mkdir jucipp/build
cd jucipp/build
cmake ..
make
sudo make install
```

## Fedora
Install dependencies:
```sh
sudo dnf install git cmake make gcc-c++ clang-devel clang lldb-devel boost-devel gtksourceviewmm3-devel gtkmm30-devel aspell-devel aspell-en libgit2-devel ctags
```

Get juCi++ source, compile and install:
```sh
git clone --recursive https://gitlab.com/cppit/jucipp
mkdir jucipp/build
cd jucipp/build
cmake -DCMAKE_CXX_COMPILER=g++ ..
make
sudo make install
```

## Mageia
**Mageia might not yet support LLDB, but you can compile without debug support.**

Install dependencies:

32-bit:

```sh
sudo urpmi git cmake make gcc-c++ clang libclang-devel libboost-devel libgtkmm3.0-devel libgtksourceviewmm3.0-devel libaspell-devel aspell-en libgit2-devel
```

64-bit:
```sh
sudo urpmi git cmake make gcc-c++ clang lib64clang-devel lib64boost-devel lib64gtkmm3.0-devel lib64gtksourceviewmm3.0-devel lib64aspell-devel aspell-en libgit2-devel
```

Get juCi++ source, compile and install:
```sh
git clone --recursive https://gitlab.com/cppit/jucipp
mkdir jucipp/build
cd jucipp/build
cmake -DCMAKE_CXX_COMPILER=g++ ..
make
sudo make install
```

## OpenSUSE Tumbleweed
Install dependencies:
```sh
sudo zypper install git-core cmake gcc-c++ boost-devel clang-devel lldb-devel lldb gtksourceviewmm-devel aspell-devel aspell-en libgit2-devel ctags
```

Get juCi++ source, compile and install:
```sh
git clone --recursive https://gitlab.com/cppit/jucipp
mkdir jucipp/build
cd jucipp/build
cmake -DCMAKE_CXX_COMPILER=g++ ..
make
sudo make install
```

## FreeBSD
On FreeBSD, latest release of juCi++ is available through the port: jucipp.

## MacOS with Homebrew (http://brew.sh/)
**Installing llvm may take some time, and you need to follow the lldb code signing instructions. For an easier dependency install, but without debug support, remove `--with-lldb` below.**

Install dependencies:
```sh
brew install --with-lldb llvm
brew install cmake pkg-config boost gtksourceviewmm3 gnome-icon-theme aspell clang-format libgit2 ctags
```

Get juCi++ source, compile and install:
```sh
git clone --recursive https://gitlab.com/cppit/jucipp
mkdir jucipp/build
cd jucipp/build
cmake ..
make
make install
```

## Windows with MSYS2 (https://msys2.github.io/)
**See https://gitlab.com/cppit/jucipp/issues/190 for details on adding debug support in MSYS2**

Install dependencies (replace `x86_64` with `i686` for 32-bit MSYS2 installs):
```sh
pacman -S git mingw-w64-x86_64-cmake make mingw-w64-x86_64-toolchain mingw-w64-x86_64-clang mingw-w64-x86_64-gtkmm3 mingw-w64-x86_64-gtksourceviewmm3 mingw-w64-x86_64-boost mingw-w64-x86_64-aspell mingw-w64-x86_64-aspell-en mingw-w64-x86_64-libgit2 mingw-w64-x86_64-universal-ctags-git
```

Note that juCi++ must be built and run in a MinGW Shell (for instance MinGW-w64 Win64 Shell).

Get juCi++ source, compile and install (replace `mingw64` with `mingw32` for 32-bit MSYS2 installs):
```sh
git clone --recursive https://gitlab.com/cppit/jucipp
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
