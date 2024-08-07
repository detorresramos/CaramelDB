import os
from setuptools import setup, Extension
from Cython.Build import cythonize
import sys


def get_compile_args():
    compile_options = [
        # '-Wall', '-Werror', '-Wextra', '-Wno-unused-function', '-Wno-psabi', '-pedantic', '-Wno-ignored-optimization-argument'
        '-Wall', '-Wextra', '-Wno-unused-function', '-Wno-psabi', '-pedantic'
    ]

    build_mode = 'Release'
    if '--debug' in sys.argv:
        build_mode = 'Debug'
        sys.argv.remove('--debug')

    build_mode_compile_options = {
        'Debug': [
            '-Og', '-g', '-fno-omit-frame-pointer'
        ],
        'Release': [
            '-DNDEBUG', '-Ofast', '-funroll-loops', '-ftree-vectorize'
        ]
    }

    return compile_options + build_mode_compile_options[build_mode]


def get_include_dirs():
    return [
        os.path.join(os.getcwd(), 'deps', 'cereal/include'),
        os.path.join(os.getcwd(), ''),
        os.path.join(os.getcwd(), 'src'),
        os.path.join(os.getcwd(), 'src/construct'),
        os.path.join(os.getcwd(), 'src/solve'),
    ]

extensions = [
    Extension(
        name="carameldb",
        sources=[
            "cython_bindings.pyx",
            "src/construct/SpookyHash.cc",
            "src/construct/SpookyHash2.cc",
            "src/construct/BucketedHashStore.cc",
            "src/construct/Codec.cc",
            "src/solve/GaussianElimination.cc",
            "src/solve/HypergraphPeeler.cc",
            "src/solve/LazyGaussianElimination.cc",
            "src/solve/Solve.cc",
            "src/Modulo2System.cc",
            "src/BitArray.cc",
        ],
        include_dirs=get_include_dirs(),
        language='c++',
        extra_compile_args=get_compile_args(),
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
        build_dir="build",  # Specify build directory
        annotate=True,      # Generate HTML annotation of the source
        compiler_directives={'language_level': "3"}  # Specify language level
    ),
    zip_safe=False,
    install_requires=["numpy"],
    extras_require={"test": ["pytest>=6.0"]},
    python_requires=">=3.7",
)
