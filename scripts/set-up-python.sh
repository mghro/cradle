#!/bin/bash
# Set up a Python virtual environment so that it can build CRADLE.
echo "Setting up Python environment in .python..."
set -x -e
virtualenv --python=python3.5 .python
source .python/bin/activate
python --version
pip install conan jinja2 gcovr pytest websocket-client msgpack
