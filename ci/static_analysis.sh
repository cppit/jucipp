#!/bin/bash

function linux () {
  cd ci || exit
  sudo docker run -it \
    -v "${PWD}/../:/jucipp/" \
    "cppit/jucipp:${distribution}" \
    sh -c "cd jucipp/build && scan-build -o ../html_${distribution} --status-bugs make"
}

#TODO should do static analysis on build
function osx () {
  true
}

$TRAVIS_OS_NAME
