#!/bin/bash
echo "Setting up system..."
add-apt-repository -y ppa:ubuntu-toolchain-r/test
apt-get update -qy
apt-get install -y --upgrade python3 python3-pip g++-5 gcc-5 lcov cmake git curl
pip3 install virtualenv
