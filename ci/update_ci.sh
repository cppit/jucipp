#!/bin/bash

function linux () {
  sudo apt-get update
  sudo apt-get purge lxc-docker
  sudo apt-get install --yes --force-yes -o Dpkg::Options::="--force-confnew" linux-image-extra-$(uname -r) docker-engine
  sudo service docker stop || exit
  sudo rm -rf /var/lib/docker || exit
  sudo service docker start || exit
}

function brew_install() {
  (brew outdated "$1" || brew install $1) || (echo "Error installing $1"; return 1)
}

function osx () {
  brew update || return 1
  brew uninstall llvm --force || return 1
  brew upgrade --all || return 1
  brew update || return 1
  brew upgrade --all || return 1
  brew install --with-clang llvm
  brew_install "boost" || return 1
  brew_install "aspell" || return 1
  brew_install "clang-format" || return 1
  brew_install "pkg-config" || return 1
  brew_install "gtksourceviewmm3" || return 1
}

function windows () {
  arch=x86_64
  if [ "$PLATFORM" == "x86" ]; then
    arch=i686
  fi
  sh -c "pacman -S --noconfirm git mingw-w64-${arch}-cmake make mingw-w64-${arch}-toolchain mingw-w64-${arch}-clang mingw-w64-${arch}-gtkmm3 mingw-w64-${arch}-gtksourceviewmm3 mingw-w64-${arch}-boost mingw-w64-${arch}-aspell mingw-w64-${arch}-aspell-en mingw-w64-${arch}-libgit2"
}

if [ "$TRAVIS_OS_NAME" == "" ]; then
  TRAVIS_OS_NAME=windows
fi

$TRAVIS_OS_NAME