FROM ubuntu:22.04 AS builder

RUN apt-get update && apt-get install -y --no-install-recommends \
    gcc \
    libc6-dev \
    gnu-efi \
    binutils \
    python3 \
    make \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /build

COPY . .

RUN make clean && make

FROM scratch AS export

COPY --from=builder /build/build/DragonTool.efi /
