import os
from setuptools import setup, Extension
from Cython.Build import cythonize

include_dirs = [
    os.path.join(os.getcwd(), 'deps', 'cereal/include'),
#     os.path.join(os.getcwd(), ''),
#     os.path.join(os.getcwd(), 'src'),
#     os.path.join(os.getcwd(), 'src/construct'),
#     os.path.join(os.getcwd(), 'src/solve'),
]

print(include_dirs)

extensions = [
    Extension(
        name="carameldb",
        sources=[
            "cython_bindings.pyx",
          #   "src/construct/SpookyHash.cc",
          #   "src/BitArray.cc",
          ],
        include_dirs=include_dirs,
        language='c++',
        extra_compile_args=['-std=c++17'],  # Use C++17 standard
    )
]

setup(
    name="carameldb",
    ext_modules=cythonize(extensions),
)
