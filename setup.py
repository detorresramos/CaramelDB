import os
from setuptools import setup, Extension
from Cython.Build import cythonize

include_dirs = [
    os.path.join(os.getcwd(), 'deps', 'cereal/include'),
    os.path.join(os.getcwd(), ''),
#     os.path.join(os.getcwd(), 'src'),
#     os.path.join(os.getcwd(), 'src/construct'),
#     os.path.join(os.getcwd(), 'src/solve'),
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
        include_dirs=include_dirs,
        language='c++',
        extra_compile_args=['-std=c++17'],  # Use C++17 standard
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
    ext_modules=cythonize(extensions),
    zip_safe=False,
    install_requires=["numpy"],
    extras_require={"test": ["pytest>=6.0"]},
    python_requires=">=3.7",
)