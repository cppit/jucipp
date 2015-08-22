# juCi++
## Installation guide ##
Before installation, please install libclangmm, see [installation guide](http://github.com/cppit/libclangmm/blob/master/docs/install.md).

## Debian
```sh
sudo apt-get install pkg-config libboost-system-dev libboost-thread-dev libboost-filesystem-dev libboost-log-dev libgtkmm-3.0-dev libgtksourceviewmm-3.0-dev
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
brew install pkg-config boost gtkmm3 gtksourceviewmm3
```

```sh
git clone https://github.com/cppit/jucipp.git
cd jucipp
cmake .
make
make install
```

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

## Run
```sh
juci
```

#Windows
```sh
startxwin /usr/local/bin/juci
```
