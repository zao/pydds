import os
from setuptools import setup, Extension
from setuptools.command.build_ext import build_ext
from wheel.bdist_wheel import bdist_wheel

cmp_core_sources = [
    os.path.join("dds/dep/cmp_core/shaders", x)
    for x in [
        "bc1_encode_kernel.cpp",
        "bc2_encode_kernel.cpp",
        "bc3_encode_kernel.cpp",
        "bc4_encode_kernel.cpp",
        "bc5_encode_kernel.cpp",
        "bc6_encode_kernel.cpp",
        "bc7_encode_kernel.cpp",
    ]
]

dds_sources = [
    os.path.join("dds/", x)
    for x in [
        "gli_format_names.cpp",
        "process_image.cpp",
    ]
]

include_dirs = [
    "dds",
    "dds/dep/cmp_core/shaders",
    "dds/dep/cmp_core/source",
    "dds/dep/fmt/include",
    "dds/dep/gli",
    "dds/dep/gli/external",
    "dds/dep/gsl/include",
]

ext_modules = [
    Extension(
        "dds_sys",
        sources=["dds/dds_bindings.cpp"] + cmp_core_sources + dds_sources,
        define_macros=[
            ("FMT_HEADER_ONLY", "1"),
            ('Py_LIMITED_API', 0x03080000),
        ],
        include_dirs=include_dirs,
        py_limited_api=True,
    ),
]

class bdist_wheel_abi3(bdist_wheel):
    def get_tag(self):
        python, abi, plat = super().get_tag()

        if python.startswith("cp"):
            # on CPython, our wheels are abi3 and compatible back to 3.8
            return "cp38", "abi3", plat

        return python, abi, plat

from pathlib import Path
long_description = (Path(__file__).parent / "README.md").read_text()

class build_ext_cxx17(build_ext):
    def build_extensions(self):
        c = self.compiler.compiler_type
        if c == 'msvc':
            for e in self.extensions:
                e.extra_compile_args = ['/std:c++latest']
        else:
            for e in self.extensions:
                e.extra_compile_args = ['-std=c++17']
        build_ext.build_extensions(self)

setup(
    name="pydds",
    version="0.0.6",
    description="DDS (including BC7) decompression bindings",
    long_description=long_description,
    long_description_content_type="text/markdown",
    classifiers=[
        "Development Status :: 3 - Alpha",
        "License :: OSI Approved :: GNU General Public License v3 or later (GPLv3+)",
        "Natural Language :: English",
        "Programming Language :: Python :: 3 :: Only",
    ],
    cmdclass={
        "bdist_wheel": bdist_wheel_abi3,
        "build_ext": build_ext_cxx17,
    },
    ext_modules=ext_modules,
    install_requires=["Pillow"]
)
