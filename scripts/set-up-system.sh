#!/bin/bash
echo "Setting up system..."
add-apt-repository -y ppa:ubuntu-toolchain-r/test
add-apt-repository -y ppa:deadsnakes/ppa
apt-get update -qy
apt-get install -y --upgrade python3.5 python3.5-pip g++-5 gcc-5 lcov cmake git curl
pip3.5 install virtualenv
