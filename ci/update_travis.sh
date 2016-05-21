#!/bin/bash

function linux () {
  sudo apt-get update
  sudo apt-get purge lxc-docker
  sudo apt-get install --yes --force-yes -o Dpkg::Options::="--force-confnew" linux-image-extra-$(uname -r) docker-engine
  sudo service docker stop || exit
  sudo rm -rf /var/lib/docker || exit
  sudo service docker start || exit
}

# TODO method should update osx, brew, packages etc needed for juCi++
function osx () {
  brew update
  brew rm llvm
  brew doctor
  brew upgrade
  brew install --with-clang --with-lldb llvm
  brew install cmake pkg-config boost homebrew/x11/gtksourceviewmm3 aspell clang-format
}

$TRAVIS_OS_NAME