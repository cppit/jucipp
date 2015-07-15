# juCi++
## Installation guide ##
## Linux
```sh
# Libraries (missing libclangmm, see http://github.com/cppit/libclangmm/ for installation)
$ sudo apt-get install libboost-python-dev libboost-filesystem-dev libboost-log-dev libboost-test-dev 
libboost-thread-dev libboost-system-dev libgtkmm-3.0-dev libgtksourceview2.0-dev libgtksourceviewmm-3.0-dev
libpython-dev libclang-dev
# Programs
$sudo apt-get install make cmake gcc

```
Compile
```sh
# When git clone
$ cd path-to-cloned-from-folder/jucipp/juci
# When download zipped file, extraxt it to a folder of your choice
$ cd path-to-folder-extraxted-into/jucipp-master/juci
# In both cases above you can choose remove the jucipp folder, but remeber to apply changes to cd command as well.
$ cmake .
$ make
```

## Run
```sh
$ ./bin/juci
```
