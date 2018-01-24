#!/bin/bash
echo "Setting up build prerequisites..."
sudo add-apt-repository ppa:ubuntu-toolchain-r/test
sudo apt-get update -qy
sudo apt-get install -y --upgrade python3 python3-pip g++-5 gcc-5 lcov cmake git curl
sudo pip3 install virtualenv
virtualenv --python=python3 .python
source .python/bin/activate
python --version
pip install conan jinja2 gcovr pytest websocket-client
conan remote add bincrafters https://api.bintray.com/conan/bincrafters/public-conan
conan remote add conan-community https://api.bintray.com/conan/conan-community/conan
