#!/bin/bash
# Generate ML-DSA-65 keys and certificates using liboqs oqs-provider
#
# NOTE: This script requires OpenSSL 3.5+ with oqs-provider
# Current system has OpenSSL 3.0.2 which doesn't support ML-DSA
#
# Reference: https://github.com/open-quantum-safe/oqs-provider
#
# ML-DSA (FIPS 204) is supported in OpenSSL 3.5+ with oqs-provider 0.10.0+

set -e

INITIATOR_DIR="/home/ipsec/PQGM-IPSec/docker/initiator/certs/mldsa"
RESPONDER_DIR="/home/ipsec/PQGM-IPSec/docker/responder/certs/mldsa"

echo "=== ML-DSA Certificate Generation Script ==="
echo ""
echo "Current OpenSSL version:"
openssl version
echo ""

# Check if oqs-provider is available
if ! openssl list -providers 2>/dev/null | grep -q oqs; then
    echo "WARNING: oqs-provider not found."
    echo ""
    echo "ML-DSA certificate generation requires:"
    echo "  1. OpenSSL 3.5+ with oqs-provider support"
    echo "  2. OR use liboqs tools to generate raw keys"
    echo ""
    echo "To install oqs-provider:"
    echo "  git clone https://github.com/open-quantum-safe/oqs-provider.git"
    echo "  cd oqs-provider"
    echo "  git checkout openssl-3.5"
    echo "  mkdir build && cd build"
    echo "  cmake -DOPENSSL_ROOT_DIR=/path/to/openssl-3.5 .."
    echo "  make"
    echo "  sudo make install"
    echo ""
    echo "Temporary solution: Use raw ML-DSA keys for testing"
    echo "See: scripts/generate_mldsa_raw_keys.c"
    exit 1
fi

echo "Generating ML-DSA-65 certificates..."

# Generate Initiator key and certificate
openssl req -x509 -newkey ml-dsa-65 \
    -keyout "$INITIATOR_DIR/mldsa_key.pem" \
    -out "$INITIATOR_DIR/mldsa_cert.pem" \
    -days 365 \
    -subj "/CN=initiator.pqgm.test" \
    -addext "subjectAltName=DNS:initiator.pqgm.test"

# Generate Responder key and certificate
openssl req -x509 -newkey ml-dsa-65 \
    -keyout "$RESPONDER_DIR/mldsa_key.pem" \
    -out "$RESPONDER_DIR/mldsa_cert.pem" \
    -days 365 \
    -subj "/CN=responder.pqgm.test" \
    -addext "subjectAltName=DNS:responder.pqgm.test"

echo "ML-DSA-65 certificates generated successfully"
echo ""
echo "Files created:"
echo "  $INITIATOR_DIR/mldsa_key.pem"
echo "  $INITIATOR_DIR/mldsa_cert.pem"
echo "  $RESPONDER_DIR/mldsa_key.pem"
echo "  $RESPONDER_DIR/mldsa_cert.pem"
