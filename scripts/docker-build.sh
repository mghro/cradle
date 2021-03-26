#!/bin/bash
set -x -e
export DEBIAN_FRONTEND=noninteractive
apt-get update
apt-get install -y lcov ocaml-nox python3-pip cmake
pip3 install virtualenv
gcc --version
scripts/set-up-python.sh --python=python3
source .venv/bin/activate
scripts/set-up-conan.sh
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j server
