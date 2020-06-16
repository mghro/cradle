#!/bin/bash
# Set up a fresh Ubuntu installation so that it can build CRADLE.
# This should be run with sudo.
# (This is currently tuned to support Ubuntu Xenial.)
echo "Setting up system..."
set -x -e
apt-get update -qy
apt-get install -y software-properties-common apt-transport-https
add-apt-repository -y ppa:ubuntu-toolchain-r/test
apt-get update -qy
apt-get install -y --upgrade python3 python3-dev g++-5 gcc-5 lcov git curl make
curl -sSL https://cmake.org/files/v3.17/cmake-3.17.2-Linux-x86_64.sh -o install-cmake.sh
chmod +x install-cmake.sh
./install-cmake.sh --prefix=/usr/local --skip-license
curl --silent --show-error --retry 5 https://bootstrap.pypa.io/get-pip.py | python3
python3 -m pip install virtualenv
