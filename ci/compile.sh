#!/bin/bash

if [ "${cmake_command}" == "" ]; then
  if [ "$APPVEYOR" != "" ]; then
    if [ "$PLATFORM" == "x64" ]; then
      mingw="mingw64"
    else
      mingw="mingw32"
    fi
    cmake_command="cmake -G\"MSYS Makefiles\" -DCMAKE_INSTALL_PREFIX=/${mingw} -DENABLE_TESTING=1 -DCMAKE_CXX_FLAGS=-Werror .."
    make_command="make"
  else
    cmake_command="cmake -DENABLE_TESTING=1 -DCMAKE_CXX_FLAGS=-Werror .."
  fi
fi

if [ "${make_command}" == "" ]; then
  make_command="make -j 2"
fi

cd jucipp || echo "Can't cd into jucipp"
git submodule update --init --recursive # appveyor doesn't checkout recursively
mkdir -p build && cd build || echo "Error making build directory"
sh -c "${cmake_command}" || echo "Cmake configuration failed"
exec sh -c "${make_command}"
