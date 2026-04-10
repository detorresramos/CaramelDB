#!/bin/bash

# Run this script as sudo (for apt commands)

apt update

# Install cmake
apt install cmake -y

# Install gcc for c++
# Make sure that openmp is supported with this version of gcc
# If using yum run: yum install gcc-c++
apt install g++ -y

git submodule update --init --recursive

curl -LsSf https://astral.sh/uv/install.sh | sh
uv sync --directory cython --extra dev
