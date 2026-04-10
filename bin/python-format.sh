#!/bin/bash

BASEDIR=$(dirname "$0")
cd $BASEDIR/..
uv run --directory cython black . --exclude "build|.venv|venv|.env|env|deps"
uv run --directory cython isort . --profile black --skip-glob deps --skip-glob .venv --skip _deps
