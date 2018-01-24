#!/bin/bash
python --version
if [ `python -c 'import sys; print("%i" % (sys.hexversion>=0x03050000))'` -eq 0 ]; then
    echo "At least Python 3.5 is required."
    exit 1
fi
echo "Setting up build prerequisites..."
pip install conan jinja2 gcovr pytest websocket-client
conan remote add bincrafters https://api.bintray.com/conan/bincrafters/public-conan
conan remote add conan-community https://api.bintray.com/conan/conan-community/conan
