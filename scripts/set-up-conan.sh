#!/bin/bash
# Install additional Conan remotes that are needed to build CRADLE.
echo "Setting up Conan..."
set -x -e
conan remote add bincrafters https://api.bintray.com/conan/bincrafters/public-conan
conan remote add conan-community https://api.bintray.com/conan/conan-community/conan
conan remote add conan-transit https://api.bintray.com/conan/conan/conan-transit
conan remote add tmadden https://api.bintray.com/conan/tmadden/public-conan
