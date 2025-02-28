#!/usr/bin/env python3

import argparse
import os
import re
import sys


def checked_system_call(cmd):
    exit_code = os.system(cmd)
    if exit_code != 0:
        exit(1)


def main():
    bin_directory = os.path.dirname(os.path.realpath(__file__))
    os.chdir(bin_directory)

    parser = argparse.ArgumentParser()
    parser.add_argument(
        "-m",
        "--build_mode",
        default="Release",
        choices=["Release", "Debug", "RelWithDebInfo", "DebugWithAsan"],
        metavar="MODE",  # Don't print the choices because they're ugly
        help='The mode to build with (see CMakeLists.txt for the specific compiler flags for each mode). Default is "Release".',
    )
    parser.add_argument(
        "-t",
        "--target",
        default="package",
        type=str,
        help="Specify a target to build (from available cmake targets). If no target is specified it defaults to 'package' which will simply build and install the library with pip.",
    )
    parser.add_argument(
        "-b",
        "--binding",
        default="pybind",
        choices=["pybind", "cython"],
        help="Choose which Python binding to build (pybind11 or Cython). Default is 'pybind'.",
    )
    args = parser.parse_args()

    # Change directory to top level.
    os.chdir("..")

    if args.target == "package":
        os.chdir(args.binding)

        os.system('mkdir -p "../build"')
        
        if args.binding == "cython":
            checked_system_call(f"python3 setup.py build_ext --inplace --mode={args.build_mode}")
            checked_system_call("pip3 install -e . --no-deps")
        else:
            os.environ["CARAMEL_BUILD_MODE"] = args.build_mode
            checked_system_call(f"pip3 install . --verbose --force --no-dependencies")
        os.chdir("..")
    else:
        cmake_command = f"cmake -B build -S . -DPYTHON_EXECUTABLE=$(which python3) -DCMAKE_BUILD_TYPE={args.build_mode}"
        build_command = f"cmake --build build --target {args.target}"

        checked_system_call(cmake_command)
        checked_system_call(build_command)


if __name__ == "__main__":
    main()
