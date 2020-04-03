#!/bin/bash
# Set up a fresh Ubuntu installation so that it can build CRADLE.
# This should be run with sudo.
# (This is currently tuned to support Ubuntu Xenial.)
echo "Setting up system..."
set -x -e
apt-get update -qy
apt-get install -y software-properties-common wget apt-transport-https
add-apt-repository -y ppa:ubuntu-toolchain-r/test
wget https://apt.kitware.com/keys/kitware-archive-latest.asc
apt-key add kitware-archive-latest.asc
apt-add-repository 'deb https://apt.kitware.com/ubuntu/ xenial main'
apt-get update -qy
apt-get install -y --upgrade python3 python3-dev g++-5 gcc-5 lcov git cmake curl
curl --silent --show-error --retry 5 https://bootstrap.pypa.io/get-pip.py | python3
python3 -m pip install virtualenv
