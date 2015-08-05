# juCi++
## Installation guide ##
Before installation, please install libclangmm see [installation guide](http://github.com/cppit/libclangmm/blob/master/docs/install.md) for installation.
## Debian
First dependencies:
```sh
$ sudo apt-get install libboost-python-dev libboost-filesystem-dev libboost-log-dev libboost-test-dev 
libboost-thread-dev libboost-system-dev libgtkmm-3.0-dev libgtksourceview2.0-dev libgtksourceviewmm-3.0-dev
libpython-dev libclang-dev make cmake gcc g++
```
Install the project:
```sh
$ git clone http://github.com/cppit/jucipp.git juci
$ cd juci
$ cmake .
$ make
$ sudo make install
```
## Run
```sh
$ juci
```
