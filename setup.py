"""setup.py for arena_hnswlib Python package using pybind11 + CMake build."""

import os
import sys
import subprocess
from pathlib import Path
from setuptools import setup, Extension
from setuptools.command.build_ext import build_ext


class CMakeExtension(Extension):
    def __init__(self, name: str, sourcedir: str = "") -> None:
        super().__init__(name, sources=[])
        self.sourcedir = os.fspath(Path(sourcedir).resolve())


class CMakeBuild(build_ext):
    def build_extension(self, ext: CMakeExtension) -> None:
        ext_fullpath = Path.cwd() / self.get_ext_fullpath(ext.name)
        extdir = ext_fullpath.parent.resolve()

        # Find pybind11 cmake dir
        import pybind11
        pybind11_cmake_dir = pybind11.get_cmake_dir()

        cmake_args = [
            f"-DCMAKE_LIBRARY_OUTPUT_DIRECTORY={extdir}",
            f"-DPYTHON_EXECUTABLE={sys.executable}",
            f"-Dpybind11_DIR={pybind11_cmake_dir}",
            "-DCMAKE_BUILD_TYPE=Release",
            "-DBUILD_ARENA_PYTHON=ON",
            # Don't build tests/benchmarks from here
            "-DBUILD_TESTS=OFF",
            "-DBUILD_BENCHMARKS=OFF",
        ]

        build_temp = Path(self.build_temp) / ext.name
        build_temp.mkdir(parents=True, exist_ok=True)

        subprocess.run(
            ["cmake", ext.sourcedir, *cmake_args],
            cwd=build_temp,
            check=True,
        )
        subprocess.run(
            ["cmake", "--build", ".", "--target", "_arena_hnswlib_ext", "--parallel"],
            cwd=build_temp,
            check=True,
        )


setup(
    name="arena_hnswlib",
    version="0.1.0",
    description="Python bindings for arena-hnswlib",
    packages=["arena_hnswlib"],
    package_dir={"arena_hnswlib": "arena_hnswlib"},
    ext_modules=[CMakeExtension("arena_hnswlib._arena_hnswlib_ext", sourcedir=".")],
    cmdclass={"build_ext": CMakeBuild},
    python_requires=">=3.8",
    install_requires=["numpy"],
)
