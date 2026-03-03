#!/bin/bash

# Install dependencies for macOS (do NOT run with sudo — Homebrew refuses root)

brew install llvm
brew install gcc
brew install cmake
brew install libomp

git submodule update --init --recursive

pip3 install pytest numpy cython
