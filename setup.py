import os
import sys
import numpy as np

from Cython.Build import cythonize
from setuptools import Extension, setup


def get_compile_args():
    base_compile_options = [
        "-std=gnu++17",
        "-Wall",
        "-Wextra",
        "-Wno-unused-function",
        "-Wno-psabi",
        "-pedantic",
        "-fopenmp",
    ]

    valid_modes = {"Debug", "Release", "RelWithDebInfo", "DebugWithAsan"}
    build_mode = "Release"

    for arg in sys.argv:
        if arg.startswith("--mode="):
            mode = arg.split("=")[1]
            if mode not in valid_modes:
                sys.exit(
                    f"Error: Invalid build mode '{mode}'. Valid options are: {', '.join(valid_modes)}"
                )
            build_mode = mode
            sys.argv.remove(arg)
            break

    build_mode_compile_options = {
        "Debug": ["-Og", "-g", "-fno-omit-frame-pointer"],
        "DebugWithAsan": ["-Og", "-g", "-fno-omit-frame-pointer", "-fsanitize=address"],
        "Release": ["-DNDEBUG", "-Ofast", "-funroll-loops", "-ftree-vectorize"],
        "RelWithDebInfo": [
            "-DNDEBUG",
            "-Ofast",
            "-funroll-loops",
            "-ftree-vectorize",
            "-g",
            "-fno-omit-frame-pointer",
        ],
    }

    return base_compile_options + build_mode_compile_options[build_mode]


def get_link_args():
    return ["-fopenmp"]


def get_include_dirs():
    return [
        os.path.join(os.getcwd(), "deps", "cereal/include"),
        os.path.join(os.getcwd(), ""),
        os.path.join(os.getcwd(), "src"),
        os.path.join(os.getcwd(), "src/construct"),
        os.path.join(os.getcwd(), "src/construct/filter"),
        os.path.join(os.getcwd(), "src/solve"),
        np.get_include(),
    ]


extensions = [
    Extension(
        name="carameldb",
        sources=[
            "cython_bindings.pyx",
            "src/construct/SpookyHash.cc",
            "src/construct/Codec.cc",
            "src/solve/GaussianElimination.cc",
            "src/solve/HypergraphPeeler.cc",
            "src/solve/LazyGaussianElimination.cc",
            "src/solve/Solve.cc",
            "src/Modulo2System.cc",
            "src/BitArray.cc",  
        ],
        include_dirs=get_include_dirs(),
        language="c++",
        extra_compile_args=get_compile_args(),
        extra_link_args=get_link_args(),
    )
]

setup(
    name="carameldb",
    version="0.0.1",
    author="Ben Coleman, Vihan Lakshman, David Torres, Chen Luo",
    author_email="detorresramos1@gmail.com",
    description="A Succinct Read-Only Lookup Table via Compressed Static Functions",
    long_description="",
    license_files=("LICENSE",),
    ext_modules=cythonize(
        extensions,
        build_dir="build",
        annotate=True,
        compiler_directives={"language_level": "3"},
    ),
    zip_safe=False,
    install_requires=["numpy"],
    extras_require={"test": ["pytest>=6.0"]},
    python_requires=">=3.7",
)
