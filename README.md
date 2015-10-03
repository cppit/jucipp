# juCi++
###### a lightweight C++-IDE with support for C++11 and C++14.
## About
Current IDEs struggle with C++ support due to the complexity of
the programming language. juCI++, however, is designed especially 
towards libclang with speed in mind. 

## Features
* Fast and responsive
* Syntax highlighting (even C++11/14, and more than 100 other file types)
* C++ warnings and errors on the fly
* Fast C++ autocomletion (even external libraries)
* Tooltips showing type information and doxygen documentation
* Refactoring across files
* Highlighting of similar types
* Spell checking depending on file context
* Basic editor functionality
* Write your own plugins in python (disabled at the moment)

## Dependencies ##
* libboost-filesystem-dev 
* libboost-log-dev 
* libboost-test-dev 
* libboost-thread-dev 
* libboost-system-dev 
* libgtkmm-3.0-dev 
* libgtksourceview2.0-dev 
* libgtksourceviewmm-3.0-dev
* libaspell-dev
* libclang-dev
* [libclangmm](../../../libclangmm/)

## Installation ##
See [installation guide](docs/install.md).
