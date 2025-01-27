#!/bin/python

import os
import argparse

parser = argparse.ArgumentParser()
parser.add_argument("-U", "--update", action="store_true", help="Update")
parser.add_argument("-C", "--configure", action="store_true", help="Force configure")
parser.add_argument("-B", "--build", action="store_true", help="Build")
args = parser.parse_args()

# ------------------------------------------------------------------------------

dir_path = os.path.dirname(os.path.realpath(__file__))
build_root = ".build"

# ------------------------------------------------------------------------------

vendor_dir = f"{build_root}/3rdparty"

deps = [
    ("SDL",   "https://github.com/libsdl-org/SDL.git", "main"   ),
    ("glad",  "https://github.com/Dav1dde/glad.git",   "glad2"  ),
    ("imgui", "https://github.com/ocornut/imgui.git",  "docking"),
    ("glm",   "https://github.com/g-truc/glm.git",     "master" ),
]

for (name, url, branch) in deps:
    path = f"{dir_path}/{vendor_dir}/{name}"

    if os.path.exists(f"{path}"):
        if args.update:
            print(f"  Updating [{name}]")
            os.system(f"cd \"{path}\" && git pull")
            os.system(f"cd \"{path}\" && git submodule update --depth 1 --recursive")
    else:
        print(f"  Cloning [{name}]")
        os.system(f"git clone -b {branch} --depth 1 --recursive {url} \"{path}\"")

# ------------------------------------------------------------------------------

build_type = "Debug"
cmake_dir = f"{build_root}/{build_type}"
c_compiler = "clang"
cxx_compiler = "clang++"

configure_ok = True

if ((not os.path.exists(cmake_dir)) or args.configure):
    print(f"  Configuring [{build_type}]")
    configure_ok = 0 == os.system(f"cmake -B {cmake_dir} -G Ninja -DVENDOR_DIR={vendor_dir} -DCMAKE_BUILD_TYPE={build_type} -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_C_COMPILER={c_compiler} -DCMAKE_CXX_COMPILER={cxx_compiler}")

if (configure_ok and args.build):
    print(f"  Building [{build_type}]")
    os.system(f"cmake --build {cmake_dir}")
