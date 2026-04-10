#!/bin/bash

BASEDIR=$(dirname "$0")

# Run python tests. Switch to build directory first so we can use the so file.
# Forward all arguments.
cd $BASEDIR/../build/

echo "Running the following tests:"
uv run --directory ../cython python -m pytest ../ --ignore-glob=../deps --collect-only "$@"
echo "Output of the tests:"
uv run --directory ../cython python -m pytest ../ --ignore-glob=../deps -s "$@"
