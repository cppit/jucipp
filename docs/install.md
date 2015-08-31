# juCi++
## Installation guide ##
Before installation, please install libclangmm, see [installation guide](http://github.com/cppit/libclangmm/blob/master/docs/install.md).

## Debian/Ubuntu 15
```sh
sudo apt-get install pkg-config libboost-system-dev libboost-thread-dev libboost-filesystem-dev libboost-log-dev libgtkmm-3.0-dev libgtksourceviewmm-3.0-dev aspell-en libaspell-dev git
```
```sh
git clone http://github.com/cppit/jucipp.git
cd jucipp
cmake .
make
sudo make install
```

## Ubuntu 14/Linux Mint 17
```sh
sudo add-apt-repository ppa:ubuntu-toolchain-r/test
sudo apt-get update
sudo apt-get install g++-4.9
sudo apt-get install pkg-config libboost-system1.55-dev libboost-thread1.55-dev libboost-filesystem1.55-dev libboost-log1.55-dev libgtkmm-3.0-dev libgtksourceviewmm-3.0-dev aspell-en libaspell-dev git
```
```sh
git clone http://github.com/cppit/jucipp.git
cd jucipp
cmake .
make
sudo make install
```

## OS X with Homebrew (http://brew.sh/)
```sh
brew install pkg-config boost gtkmm3 homebrew/x11/gtksourceviewmm3 aspell git
```

```sh
git clone https://github.com/cppit/jucipp.git
cd jucipp
cmake .
make
make install
```

##Windows with MSYS2 (https://msys2.github.io/)
Install dependencies(replace [arch] with i686 or x86_64 depending on your MSYS2 install):
```sh
pacman -S patch autoconf automake-wrapper mingw-w64-[arch]-gtkmm3 mingw-w64-[arch]-boost mingw-w64-[arch]-aspell mingw-w64-[arch]-aspell-en git
```

Get juCi++ source:
```sh
git clone https://github.com/cppit/jucipp.git
cd jucipp
```

Compiling and installing gtksourceview3 and gtksourceviewmm3:
```sh
cd MINGW-packages/mingw-w64-gtksourceview3/
makepkg-mingw -sLf
pacman -U mingw-w64-[arch]-gtksourceview3-3.12.0-2-any.pkg.tar.xz
cd ../mingw-w64-gtksourceviewmm3/
makepkg-mingw -sLf
pacman -U mingw-w64-[arch]-gtksourceviewmm3-3.12.0-1-any.pkg.tar.xz
cd ../../
```

Compile and install juCi++ source:
```sh
cmake -G"MSYS Makefiles" -DCMAKE_INSTALL_PREFIX=/mingw[32 or 64] .
make
make install
```

<!--
## Windows with Cygwin (https://www.cygwin.com/)
**Make sure the PATH environment variable does not include paths to non-Cygwin cmake, make and g++.**

Select and install the following packages from the Cygwin-installer:
```
pkg-config libboost-devel libgtkmm3.0-devel libgtksourceviewmm3.0-devel xinit
```
Then run the following in the Cygwin Terminal:
```sh
git clone https://github.com/cppit/jucipp.git
cd jucipp
cmake .
make
make install
```

Note that we are currently working on a Windows-version without the need of an X-server.
-->

## Run
```sh
juci
```

<!--
#### Windows
```sh
startxwin /usr/local/bin/juci
```
-->
