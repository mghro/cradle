FROM ubuntu:xenial as builder
COPY . /cradle
WORKDIR /cradle
RUN scripts/docker-build.sh

FROM ubuntu:xenial
COPY --from=builder /cradle/build/deploy /cradle
COPY ./docker-config.json /root/.config/cradle/config.json
WORKDIR /cradle
VOLUME ["/var/cache/cradle"]
EXPOSE 41071
ENTRYPOINT ["/cradle/server"]
