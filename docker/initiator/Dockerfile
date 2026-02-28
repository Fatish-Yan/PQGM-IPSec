# Dockerfile for PQ-GM-IKEv2 strongSwan
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

# Install dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    libssl-dev \
    libgmp-dev \
    git \
    wget \
    gmssl \
    iproute2 \
    iputils-ping \
    && rm -rf /var/lib/apt/lists/*

# Install strongSwan from source with gmalg plugin
COPY strongswan /opt/strongswan
RUN cd /opt/strongswan && \
    ./configure --prefix=/usr/local \
        --enable-gmalg \
        --enable-swanctl \
        --enable-openssl \
        --enable-pem \
        --enable-pkcs1 \
        --enable-pkcs8 \
        --enable-x509 \
        --enable-pubkey \
        --enable-curl \
        --enable-kdf \
        --enable-ml \
    && make -j$(nproc) \
    && make install

# Copy certificates and configs
COPY certs-pqgm /etc/certs-pqgm

# Create config directory
RUN mkdir -p /usr/local/etc/swanctl/{x509,x509ca,private}

WORKDIR /root

# Start charon by default
CMD ["/usr/local/libexec/ipsec/charon", "--debug-ike", "2"]
