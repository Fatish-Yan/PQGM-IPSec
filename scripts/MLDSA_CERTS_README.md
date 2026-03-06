# ML-DSA Certificate Generation

## Overview

This directory contains scripts for generating ML-DSA-65 keys and certificates for testing the PQGM-IKEv2 implementation.

## Prerequisites

### Option A: OpenSSL 3.5+ with oqs-provider (Recommended for certificates)

ML-DSA (FIPS 204) certificate generation requires:
- OpenSSL 3.5 or later
- oqs-provider 0.12.0 or later

#### Installation Instructions

```bash
# Build OpenSSL 3.5+ with OQS support
git clone https://github.com/openssl/openssl.git
cd openssl
git checkout openssl-3.5
./config --prefix=/opt/openssl-3.5
make -j$(nproc)
sudo make install

# Build oqs-provider
git clone https://github.com/open-quantum-safe/oqs-provider.git
cd oqs-provider
git checkout openssl-3.5
mkdir build && cd build
cmake -DOPENSSL_ROOT_DIR=/opt/openssl-3.5 ..
make
sudo make install
```

### Option B: liboqs (For raw keys)

For systems with OpenSSL 3.0-3.4, raw ML-DSA keys can be generated using liboqs.

```bash
# Install liboqs
git clone https://github.com/open-quantum-safe/liboqs.git
cd liboqs
mkdir build && cd build
cmake -DOQS_ENABLE_SIG_ml_dsa_65=ON ..
make
sudo make install
```

## Scripts

### 1. generate_mldsa_certs.sh

Generates ML-DSA-65 certificates in PEM format (requires OpenSSL 3.5+).

**Usage:**
```bash
./scripts/generate_mldsa_certs.sh
```

**Output:**
- `docker/initiator/certs/mldsa/mldsa_key.pem`
- `docker/initiator/certs/mldsa/mldsa_cert.pem`
- `docker/responder/certs/mldsa/mldsa_key.pem`
- `docker/responder/certs/mldsa/mldsa_cert.pem`

### 2. generate_mldsa_raw_keys.c

Generates raw ML-DSA-65 keypairs in binary format (fallback for OpenSSL 3.0-3.4).

**Compile:**
```bash
gcc -o generate_mldsa_raw_keys generate_mldsa_raw_keys.c -loqs
```

**Usage:**
```bash
./generate_mldsa_raw_keys
```

**Output:**
- `initiator_mldsa_public.bin` (1952 bytes)
- `initiator_mldsa_private.bin` (4032 bytes)
- `responder_mldsa_public.bin` (1952 bytes)
- `responder_mldsa_private.bin` (4032 bytes)

## ML-DSA-65 Key Sizes

According to FIPS 204:
- **Public key**: 1952 bytes
- **Private key**: 4032 bytes
- **Signature**: 3309 bytes

## Current System Status

```
OpenSSL version: 3.0.2 (doesn't support ML-DSA)
oqs-provider: Not installed
```

**Recommendation**: For initial testing, use raw ML-DSA keys generated with liboqs. Full certificate-based authentication requires OpenSSL 3.5+ upgrade.

## Integration with strongSwan

Once ML-DSA certificates are generated, configure strongSwan:

```bash
# swanctl.conf
remote_addrs = 10.0.0.2
auth = pubkey
certs = mldsa_cert.pem
```

## References

- [FIPS 204: Module-Lattice-Based Digital Signature Standard](https://csrc.nist.gov/pubs/fips/204/final)
- [OpenSSL 3.5 Release Notes](https://www.openssl.org/news/openssl-3.5-notes.html)
- [oqs-provider Documentation](https://github.com/open-quantum-safe/oqs-provider)
- [liboqs Documentation](https://github.com/open-quantum-safe/liboqs)
