#!/bin/bash
set -x -e
scripts/set-up-system.sh
scripts/set-up-python.sh --python=python3.5
source .venv/bin/activate
export CC=`which gcc-5`
export CXX=`which g++-5`
update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-5 60 --slave /usr/bin/g++ g++ /usr/bin/g++-5
scripts/set-up-conan.sh
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j server
