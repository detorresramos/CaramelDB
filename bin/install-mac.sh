#!/bin/bash

# Run this script with sudo

# Install llvm
brew install llvm

# Install gcc
brew install gcc

# Install cmake
brew install cmake

# Install openmp (from LLVM)
brew install libomp

pip3 install pytest
