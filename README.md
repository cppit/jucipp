# juCi++
###### a lightweight C++-IDE with support for C++11 and C++14.
## About
juCi++ is a lightweight C++-IDE written in C++. You can write plugins
in Python and configure the IDE from the config.json file.


## Autocompletion
The IDE supports autocompletion on accessor specifiers such as dot and arrow. The autocompletion has excellent support for C++11/14.

## Syntax Highlighting
The IDE also supports syntax highligthing even in C++11 and C++14.

## To get C++11 and C++14 support
If you want support for external libraries and C++11 and C++14 you need to make sure you add a parameter to the cmake command. This will make compile_commands.json wich tells juCi++ all the paramters for the compiler. There is a bug where juCi++ sometimes puts the compile_commands.json file in the incorrect folder. Please make sure this file exists and that the contents of it covers all your files.
```sh
$ cmake . -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
```

## Plugins
There are expansion oppertunities with usage of python plugins.

## Configuration
Configuration description will arrive shortly after source code is added.

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
See [installation guide](http://github.com/cppit/jucipp/blob/master/docs/install.md).


