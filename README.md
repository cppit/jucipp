# juCi++ a lightweight C++-IDE with support for C++11 and C++14.
juCi++ is a lightweight C++-IDE written in C++. You can write plugins
in Python and configure the IDE from the config.json file.


## Autocompletion
The IDE supports autocompletion on accessor specifiers such as dot and arrow. The autocompletion has excellent support for C++11/14.

## Syntax Highlighting
The IDE also supports syntax highligthing even in C++11 and C++14.

## Plugins
There are expantion oppertunities with usage of python plugins.

## Configuration
Configuration description will arrive shortly after source code is added.

## Dependencies ##
Please install these dependencies on your system.

* libclang
* [libclangmm](http://github.com/cppit/libclangmm/tree/juci)
* cmake
* make
* clang or gcc (compiler)

## Installation ##

```sh
$ cmake .
$ make
```

## Run
```sh
$ ./bin/juci
```
