#!/bin/bash

if [ "${cmake_command}" == "" ]; then
  cmake_command="cmake -DENABLE_TESTING=1 -DCMAKE_CXX_FLAGS=-Werror .."
fi

if [ "${make_command}" == "" ]; then
  make_command="make -j 2"
fi

cd jucipp || exit
mkdir -p build && cd build || exit
sh -c "${cmake_command}" || exit
exec sh -c "${make_command}"