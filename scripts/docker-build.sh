#!/bin/bash
set -x -e
source /scripts/.venv/bin/activate
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make server
