#!/bin/bash

function linux () {
  cd ci || exit
  sudo rm ../build -rf
  sudo docker run -it \
    -e "CXX=$CXX" \
    -e "CC=$CC" \
    -e "make_command=$make_command" \
    -e "cmake_command=$cmake_command" \
    -e "distribution=$distribution" \
    -v "$PWD/../:/jucipp" \
    --entrypoint="/jucipp/ci/entrypoint.sh" \
    "cppit/jucipp:$distribution"
}

#TODO Should run compile/install instructions for osx
function osx () {
  true
}

$TRAVIS_OS_NAME
