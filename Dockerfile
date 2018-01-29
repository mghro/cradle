FROM ubuntu:xenial as builder
COPY . /cradle
WORKDIR /cradle
RUN scripts/docker-build.sh

FROM ubuntu:xenial
COPY --from=builder /fips-deploy/cradle/linux-make-release cradle
