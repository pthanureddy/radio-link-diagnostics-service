FROM ubuntu:24.04 AS build

RUN apt-get update \
    && DEBIAN_FRONTEND=noninteractive apt-get install --yes --no-install-recommends \
       build-essential cmake ninja-build \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .
RUN cmake -S . -B build -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -DRADIO_WARNINGS_AS_ERRORS=ON \
    && cmake --build build \
    && ctest --test-dir build --output-on-failure

FROM ubuntu:24.04
RUN groupadd --system radio-link \
    && useradd --system --gid radio-link --no-create-home radio-link
COPY --from=build /src/build/radio_link_monitor /usr/local/bin/radio_link_monitor
COPY --from=build /src/build/radio_link_sender /usr/local/bin/radio_link_sender
USER radio-link
EXPOSE 9400/udp
ENTRYPOINT ["/usr/local/bin/radio_link_monitor"]
CMD ["--port", "9400", "--continuous"]
