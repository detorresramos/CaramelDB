#!/bin/bash
# --no-sync, and --project rather than --directory so cwd is not cython/: test
# the package bin/build.py installed, not a re-sync or the cython/ sources.

BASEDIR=$(dirname "$0")

# Run python tests. Switch to build directory first so we can use the so file.
# Forward all arguments.
cd $BASEDIR/../build/

echo "Running the following tests:"
uv run --no-sync --project ../cython python -m pytest ../ --ignore-glob=../deps --collect-only "$@"
echo "Output of the tests:"
uv run --no-sync --project ../cython python -m pytest ../ --ignore-glob=../deps -s "$@"
