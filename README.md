# juCi++
###### a lightweight platform independent C++-IDE with support for C++11 and C++14.
## About
Current IDEs struggle with C++ support due to the complexity of
the programming language. juCI++, however, is designed especially 
towards libclang with speed and ease of use in mind. 

## Features
* Platform independent
* Fast and responsive (written in C++)
* Syntax highlighting (even C++11/14, and more than 100 other file types)
* (Obj)C(++) warnings and errors on the fly
* (Obj)C(++) Fix-its
* Automated CMake processing
* Fast (Obj)C(++) autocomletion (even external libraries)
* Keyword and buffer autocomletion for other file types
* Tooltips showing type information and doxygen documentation
* Refactoring across files
* Highlighting of similar types
* Spell checking depending on file context
* Run shell commands within JuCi++, even on Windows
* Regex search and replace
* Smart paste, keys and indentation
* Source minimap
* Full UTF-8 support
* Write your own plugins in Python (disabled at the moment)

See [enhancements](https://github.com/cppit/jucipp/labels/enhancement) for planned features.

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
* [libclangmm](http://github.com/cppit/libclangmm/)

## Installation ##
See [installation guide](http://github.com/cppit/jucipp/blob/master/docs/install.md).
