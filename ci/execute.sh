#!/bin/bash

function linux () {
  cd ci || exit
  if [ "${script}" == "clean" ]; then
    sudo rm ../build -rf
    return 0
  fi
  sudo docker run -it \
    -e "CXX=$CXX" \
    -e "CC=$CC" \
    -e "make_command=$make_command" \
    -e "cmake_command=$cmake_command" \
    -e "distribution=$distribution" \
    -v "$PWD/../:/jucipp" \
    --entrypoint="/jucipp/ci/${script}.sh" \
    "cppit/jucipp:$distribution"
}

#TODO Should run compile/install instructions for osx
function osx () {
  cd .. || (echo "Error changing directory";return 1)
  if [ "${script}" == "clean" ]; then
    sudo rm -rf "./jucipp/build"
    return 0
  fi
  sh -c "./jucipp/ci/${script}.sh" || return 1
}

function windows () {
  export PATH="/mingw64/bin:/usr/local/bin:/usr/bin:/bin:/c/WINDOWS/system32:/c/WINDOWS:/c/WINDOWS/System32/Wbem:/c/WINDOWS/System32/WindowsPowerShell/v1.0:/usr/bin/site_perl:/usr/bin/vendor_perl:/usr/bin/core_perl"
  cd "C:/projects" || (echo "Error changing directory"; return 1)
  if [ "${script}" == "clean" ]; then
    sudo rm "C:/projects/jucipp/build" -rf
    return 0
  fi
  sh -c "C:/projects/jucipp/ci/${script}.sh"
}


if [ "$TRAVIS_OS_NAME" == "" ]; then
  TRAVIS_OS_NAME=windows
fi

$TRAVIS_OS_NAME
