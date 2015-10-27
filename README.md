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
* C++ warnings and errors on the fly
* C++ Fix-its
* Automated CMake processing
* Fast C++ autocomletion (including external libraries)
* Keyword and buffer autocomletion for other file types
* Tooltips showing type information and doxygen documentation
* Refactoring across files
* Highlighting of similar types
* Documentation search
* Spell checking depending on file context
* Run shell commands within JuCi++, even on Windows
* Regex search and replace
* Smart paste, keys and indentation
* Auto-indentation of C++ file buffers through [clang-format](http://clang.llvm.org/docs/ClangFormat.html)
* Source minimap
* Full UTF-8 support

See [enhancements](https://github.com/cppit/jucipp/labels/enhancement) for planned features.

## Dependencies ##
* boost-filesystem
* boost-log
* boost-thread
* boost-system
* gtkmm-3.0
* gtksourceviewmm-3.0
* aspell
* libclang
* [libclangmm](http://github.com/cppit/libclangmm/)

## Installation ##
See [installation guide](http://github.com/cppit/jucipp/blob/master/docs/install.md).
