#!/bin/bash
set -x -e
scripts/set-up-system.sh
scripts/set-up-python.sh
source .python/bin/activate
export CC=`which gcc-5`
export CXX=`which g++-5`
scripts/set-up-conan.sh
./fips set config linux-make-release
./fips gen
./fips make server
