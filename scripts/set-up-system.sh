#!/bin/bash
echo "Setting up system..."
apt-get update -qy
apt-get dist-upgrade -qy
apt-get install -y software-properties-common
add-apt-repository -y ppa:ubuntu-toolchain-r/test
add-apt-repository -y ppa:deadsnakes/ppa
apt-get update -qy
apt-get install -y --upgrade python3.5 python3.5-dev g++-5 gcc-5 lcov cmake git curl
wget https://bootstrap.pypa.io/get-pip.py -O /tmp/get-pip.py
python3.5 /tmp/get-pip.py
python3.5 -m pip install virtualenv
rm /tmp/get-pip.py
