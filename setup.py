import os
from pybind11.setup_helpers import Pybind11Extension, build_ext
from setuptools import find_packages, setup, Extension

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
    Pybind11Extension(
        "dds_sys",
        sources=["dds/dds_bindings.cpp"] + cmp_core_sources + dds_sources,
        define_macros=[("FMT_HEADER_ONLY", "1")],
        include_dirs=include_dirs,
    ),
]

from pathlib import Path

long_description = (Path(__file__).parent / "README.md").read_text()

setup(
    name="pydds",
    version="0.0.4",
    description="DDS (including BC7) decompression bindings",
    long_description=long_description,
    long_description_content_type="text/markdown",
    classifiers=[
        "Development Status :: 3 - Alpha",
        "License :: OSI Approved :: GNU General Public License v3 or later (GPLv3+)",
        "Natural Language :: English",
        "Programming Language :: Python :: 3 :: Only",
    ],
    cmdclass={"build_ext": build_ext},
    packages=find_packages(),
    ext_modules=ext_modules,
    install_requires=["Pillow"]
)
