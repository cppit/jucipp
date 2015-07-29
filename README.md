# juCi++
###### a lightweight C++-IDE with support for C++11 and C++14.
## About
A lot of IDEs struggle with good C++11/14 support. Therefore we
created juCi++. juCi++ is based of the compiler and will *always*
support new versions of C++.

## Features
* Syntax highlighing (even C++11/14)
* Superfast autocomletion (even external libraries)
* Accurate refactoring across files
* Basic editor functionallity
* Highlighting of similar types
* write your own plugins in python (limited atm)

## Dependencies ##
Please install these dependencies on your system.
* libboost-python-dev 
* libboost-filesystem-dev 
* libboost-log-dev 
* libboost-test-dev 
* libboost-thread-dev 
* libboost-system-dev 
* libgtkmm-3.0-dev 
* libgtksourceview2.0-dev 
* libgtksourceviewmm-3.0-dev 
* libpython-dev 
* libclang-dev
* [libclangmm](http://github.com/cppit/libclangmm/)
* cmake
* make
* clang or gcc (compiler)

## Installation ##
Quickstart:
```sh
$ cmake .
$ make
$ sudo make install
```
See [installation guide](http://github.com/cppit/jucipp/blob/master/docs/install.md) for more details.

