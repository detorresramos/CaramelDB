import multiprocessing
import os
import re
import subprocess
import sys
from pathlib import Path

import numpy as np
from Cython.Build import cythonize
from setuptools import Extension, find_packages, setup
from setuptools.command.build_ext import build_ext


def get_build_mode():
    return os.environ.get("CARAMEL_BUILD_MODE", "Release")


class CMakeCythonBuild(build_ext):
    """Build caramel_lib via CMake, then compile the Cython extension against it."""

    def build_extension(self, ext):
        project_root = str(Path(__file__).resolve().parent.parent)
        build_dir = os.path.join(project_root, "build")
        os.makedirs(build_dir, exist_ok=True)

        cfg = get_build_mode()

        cmake_args = [
            f"-DCMAKE_BUILD_TYPE={cfg}",
            f"-DPYTHON_EXECUTABLE={sys.executable}",
        ]

        cmake_generator = os.environ.get("CMAKE_GENERATOR", "")
        if "CMAKE_ARGS" in os.environ:
            cmake_args += [item for item in os.environ["CMAKE_ARGS"].split(" ") if item]

        if not cmake_generator or cmake_generator == "Ninja":
            try:
                import ninja  # noqa: F401

                cmake_args += ["-GNinja"]
            except ImportError:
                pass

        if sys.platform.startswith("darwin"):
            archs = re.findall(r"-arch (\S+)", os.environ.get("ARCHFLAGS", ""))
            if archs:
                cmake_args += ["-DCMAKE_OSX_ARCHITECTURES={}".format(";".join(archs))]

        jobs = multiprocessing.cpu_count() * 2

        # Configure and build caramel_lib only
        subprocess.check_call(["cmake", project_root] + cmake_args, cwd=build_dir)
        subprocess.check_call(
            ["cmake", "--build", ".", "-t", "caramel_lib", f"-j{jobs}"],
            cwd=build_dir,
        )

        # Now let setuptools/Cython compile the extension as normal
        build_ext.build_extension(self, ext)


def get_compile_args():
    mode = get_build_mode()
    args = ["-std=c++17"]
    if mode in ("Release", "RelWithDebInfo"):
        args += ["-DNDEBUG", "-O3", "-ffast-math", "-funroll-loops", "-ftree-vectorize"]
    if mode in ("Debug", "DebugWithAsan"):
        args += ["-Og", "-g", "-fno-omit-frame-pointer"]
    if mode == "RelWithDebInfo":
        args += ["-g", "-fno-omit-frame-pointer"]
    if mode == "DebugWithAsan":
        args += ["-fsanitize=address"]

    # OpenMP support
    if sys.platform == "darwin":
        # Detect Homebrew libomp
        for prefix in ["/opt/homebrew/opt/libomp", "/usr/local/opt/libomp"]:
            if os.path.exists(prefix):
                args += ["-Xpreprocessor", "-fopenmp", f"-I{prefix}/include"]
                break
    else:
        args += ["-fopenmp", "-fpermissive"]

    return args


def get_link_args():
    args = []

    if sys.platform == "darwin":
        for prefix in ["/opt/homebrew/opt/libomp", "/usr/local/opt/libomp"]:
            if os.path.exists(prefix):
                args += [f"-L{prefix}/lib", "-lomp"]
                break
    else:
        args += ["-lgomp"]

    if get_build_mode() == "DebugWithAsan":
        args += ["-fsanitize=address"]

    return args


def get_include_dirs():
    project_root = str(Path(__file__).resolve().parent.parent)
    build_dir = os.path.join(project_root, "build")

    dirs = [
        project_root,
        os.path.join(project_root, "deps"),
        os.path.join(project_root, "deps", "cereal", "include"),
        build_dir,
        np.get_include(),
    ]
    return dirs


def get_library_dirs():
    project_root = str(Path(__file__).resolve().parent.parent)
    build_dir = os.path.join(project_root, "build")
    return [build_dir]


extensions = [
    Extension(
        "carameldb._caramel",
        sources=["_caramel.pyx"],
        include_dirs=get_include_dirs(),
        library_dirs=get_library_dirs(),
        libraries=["caramel_lib"],
        extra_compile_args=get_compile_args(),
        extra_link_args=get_link_args(),
        language="c++",
    )
]

setup(
    packages=find_packages(),
    ext_modules=cythonize(
        extensions,
        compiler_directives={
            "language_level": "3",
            "boundscheck": False,
            "wraparound": False,
        },
    ),
    cmdclass={"build_ext": CMakeCythonBuild},
    zip_safe=False,
)
