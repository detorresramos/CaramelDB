#!/bin/bash

# Install dependencies for macOS (do NOT run with sudo — Homebrew refuses root)

brew install llvm
brew install gcc
brew install cmake
brew install libomp

git submodule update --init --recursive

curl -LsSf https://astral.sh/uv/install.sh | sh
uv sync --directory cython --extra dev
