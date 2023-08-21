#!/bin/bash
# Usage sh bin/create_flamegraph.sh <python_script> <flamegraph_name>

# Requirements:
#  - You need to have built with debug symbols (Debug or RelWithDebInfo)
#  - Perf needs to be installed (so you need to be running on linux). 

# See https://stackoverflow.com/questions/592620/how-can-i-check-if-a-program-exists-from-a-bash-script
type perf >/dev/null 2>&1 || { echo >&2 "This script requires perf but it's not installed.  Aborting."; exit 1; }

BASEDIR=$(dirname "$0")

echo "Creating flamegraph for python script '$1'"
SCRIPT=$1

perf record -F 99 --call-graph dwarf python3 $SCRIPT
perf script > out.perf

$BASEDIR/../deps/flamegraph/stackcollapse-perf.pl out.perf | $BASEDIR/../deps/flamegraph/flamegraph.pl  > $2.svg

rm out.perf
rm perf.data