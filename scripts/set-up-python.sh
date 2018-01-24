#!/bin/bash
echo "Setting up Python environment in .python..."
virtualenv --python=python3 .python
source .python/bin/activate
python --version
pip install conan jinja2 gcovr pytest websocket-client
