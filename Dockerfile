FROM ubuntu:xenial as builder

COPY . /cradle

WORKDIR /cradle

RUN scripts/set-up-system.sh \
 && scripts/set-up-python.sh \
 && source .python/bin/activate \
 && export CC=`which gcc-5` \
 && export CXX=`which g++-5` \
 && scripts/set-up-conan.sh \
 && ./fips set config linux-make-release \
 && ./fips gen \
 && ./fips make server

FROM ubuntu:xenial
COPY --from=builder /fips-deploy/cradle/linux-make-release cradle
