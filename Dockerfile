FROM ubuntu:focal as builder
COPY scripts /scripts
WORKDIR /scripts
RUN /scripts/docker-setup.sh
COPY . /cradle
WORKDIR /cradle
RUN /scripts/docker-build.sh

FROM ubuntu:focal
COPY --from=builder /cradle/build/deploy /cradle
COPY ./docker-config.json /root/.config/cradle/config.json
WORKDIR /cradle
VOLUME ["/var/cache/cradle"]
EXPOSE 41071
ENTRYPOINT ["/cradle/server"]
