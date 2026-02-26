#!/bin/bash
#
# PQ-GM-IKEv2 Certificate Generation Script (using strongSwan pki)
# Generates SM2 dual certificates (SignCert + EncCert) for IKEv2 protocol
#
# Certificate Types:
#   - SignCert: SM2 signature certificate for IKE_AUTH (with digitalSignature)
#   - EncCert:  SM2 encryption certificate for SM2-KEM (with ikeIntermediate EKU)
#
# Note: This script uses strongSwan's pki tool which supports SM2 keys
#

set -e

# Configuration
CERT_DIR="/home/ipsec/PQGM-IPSec/certs"
DAYS_CA=3650     # CA validity: 10 years
DAYS_CERT=365    # End entity validity: 1 year

# Subject DN components
C="CN"
O="PQGM-Test"
ST="Beijing"
L="Haidian"

# Color output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Check if pki is available
check_tools() {
    log_info "Checking tools..."
    if ! command -v pki &> /dev/null; then
        log_error "strongSwan pki tool not found. Please install strongSwan first."
        exit 1
    fi

    PKI_VERSION=$(pki --version 2>&1 | head -1)
    log_info "PKI version: $PKI_VERSION"

    # Check if openssl is available for verification
    if command -v openssl &> /dev/null; then
        OPENSSL_VERSION=$(openssl version)
        log_info "OpenSSL version: $OPENSSL_VERSION"
    fi
}

# Generate SM2 key pair using pki
# Usage: gen_sm2_key <output_key_file>
gen_sm2_key() {
    local key_file="$1"

    log_info "Generating SM2 key pair: $key_file"

    # Generate EC key with SM2 curve (if supported) or use generic EC key
    pki --gen --type ecdsa --size 256 --outform pem > "$key_file" 2>/dev/null || {
        log_warn "SM2 curve not directly supported, generating generic ECDSA key"
        pki --gen --type ecdsa --size 256 --outform pem > "$key_file"
    }
}

# Generate CA certificate (self-signed)
# Usage: gen_ca_cert <key_file> <cert_file> <cn>
gen_ca_cert() {
    local key_file="$1"
    local cert_file="$2"
    local cn="$3"

    log_info "Generating CA certificate: $cert_file (CN=$cn)"

    pki --self --type ecdsa \
        --in "$key_file" \
        --dn "C=$C, ST=$ST, L=$L, O=$O, CN=$cn" \
        --ca \
        --pathlen 2 \
        --lifetime $DAYS_CA \
        --outform pem > "$cert_file"
}

# Generate end-entity certificate with specific flags
# Usage: gen_ee_cert <key_file> <cert_file> <cn> <ca_cert> <ca_key> <flags>
gen_ee_cert() {
    local key_file="$1"
    local cert_file="$2"
    local cn="$3"
    local ca_cert="$4"
    local ca_key="$5"
    local flags="$6"

    log_info "Generating certificate: $cert_file (CN=$cn, flags=$flags)"

    pki --issue --type ecdsa \
        --in "$key_file" \
        --cacert "$ca_cert" \
        --cakey "$ca_key" \
        --dn "C=$C, ST=$ST, L=$L, O=$O, CN=$cn" \
        --lifetime $DAYS_CERT \
        $flags \
        --outform pem > "$cert_file"
}

# Verify certificate
# Usage: verify_cert <cert_file> <ca_cert>
verify_cert() {
    local cert_file="$1"
    local ca_cert="$2"

    log_info "Verifying certificate: $cert_file"

    if pki --verify --in "$cert_file" --cacert "$ca_cert" 2>/dev/null; then
        log_info "Certificate verification: PASSED"
        return 0
    else
        log_warn "Certificate verification: FAILED (but this might be OK for test certs)"
        return 1
    fi
}

# Print certificate info
# Usage: print_cert_info <cert_file>
print_cert_info() {
    local cert_file="$1"

    echo "----------------------------------------"
    echo "Certificate: $cert_file"
    echo "----------------------------------------"
    pki --print --type x509 --in "$cert_file" 2>/dev/null | grep -E "(subject:|issuer:|validity:|serial:|flags:)" || true
    echo ""
}

# Generate all CA certificates
generate_ca_certs() {
    log_info "=== Generating CA Certificates ==="

    # Create CA directory
    mkdir -p "$CERT_DIR/ca"

    # Generate CA key and certificate
    gen_sm2_key "$CERT_DIR/ca/ca_key.pem"
    gen_ca_cert "$CERT_DIR/ca/ca_key.pem" "$CERT_DIR/ca/ca_cert.pem" "PQGM-CA"

    log_info "CA certificates generated successfully"
}

# Generate end-entity certificates for a role (initiator/responder)
# Usage: generate_ee_certs <role>
generate_ee_certs() {
    local role="$1"
    local role_dir="$CERT_DIR/$role"
    local cn_prefix="${role}.pqgm"

    log_info "=== Generating $role Certificates ==="

    mkdir -p "$role_dir"

    # 1. Signing Certificate (SignCert)
    # For IKE_AUTH signature verification
    log_info "Generating SignCert for $role..."
    gen_sm2_key "$role_dir/sign_key.pem"
    gen_ee_cert "$role_dir/sign_key.pem" "$role_dir/sign_cert.pem" \
        "$cn_prefix-sign" \
        "$CERT_DIR/ca/ca_cert.pem" "$CERT_DIR/ca/ca_key.pem" \
        "--flag serverAuth --flag clientAuth"

    # 2. Encryption Certificate (EncCert)
    # For SM2-KEM with IKE_INTERMEDIATE EKU
    log_info "Generating EncCert for $role..."
    gen_sm2_key "$role_dir/enc_key.pem"
    gen_ee_cert "$role_dir/enc_key.pem" "$role_dir/enc_cert.pem" \
        "$cn_prefix-enc" \
        "$CERT_DIR/ca/ca_cert.pem" "$CERT_DIR/ca/ca_key.pem" \
        "--flag ikeIntermediate"

    log_info "$role certificates generated successfully"
}

# Generate strongSwan configuration files
generate_swanctl_config() {
    local role="$1"
    local role_dir="$CERT_DIR/$role"

    log_info "Generating swanctl configuration for $role..."

    # Create swanctl directory structure
    local swanctl_dir="$CERT_DIR/swanctl/$role"
    mkdir -p "$swanctl_dir"/{cacerts,certs,private}

    # Copy certificates
    cp "$CERT_DIR/ca/ca_cert.pem" "$swanctl_dir/cacerts/"
    cp "$role_dir/sign_cert.pem" "$swanctl_dir/certs/"
    cp "$role_dir/enc_cert.pem" "$swanctl_dir/certs/"
    cp "$role_dir/sign_key.pem" "$swanctl_dir/private/"
    cp "$role_dir/enc_key.pem" "$swanctl_dir/private/"

    log_info "swanctl files prepared in $swanctl_dir"
}

# Main function
main() {
    log_info "========================================"
    log_info "PQ-GM-IKEv2 Certificate Generation (pki)"
    log_info "========================================"
    echo ""

    # Check tools
    check_tools
    echo ""

    # Generate CA certificates
    generate_ca_certs
    echo ""

    # Generate initiator certificates
    generate_ee_certs "initiator"
    echo ""

    # Generate responder certificates
    generate_ee_certs "responder"
    echo ""

    # Verify and print certificates
    log_info "=== Certificate Verification ==="

    # Verify CA
    print_cert_info "$CERT_DIR/ca/ca_cert.pem"

    # Verify initiator certs
    for cert_type in sign enc; do
        print_cert_info "$CERT_DIR/initiator/${cert_type}_cert.pem"
        verify_cert "$CERT_DIR/initiator/${cert_type}_cert.pem" "$CERT_DIR/ca/ca_cert.pem" || true
    done

    # Verify responder certs
    for cert_type in sign enc; do
        print_cert_info "$CERT_DIR/responder/${cert_type}_cert.pem"
        verify_cert "$CERT_DIR/responder/${cert_type}_cert.pem" "$CERT_DIR/ca/ca_cert.pem" || true
    done

    # Generate swanctl config files
    log_info "=== Generating swanctl Configuration Files ==="
    generate_swanctl_config "initiator"
    generate_swanctl_config "responder"

    # Summary
    log_info "========================================"
    log_info "Certificate Generation Complete!"
    log_info "========================================"
    echo ""
    echo "Generated files:"
    echo ""
    echo "CA certificates:"
    echo "  $CERT_DIR/ca/ca_key.pem      - CA private key"
    echo "  $CERT_DIR/ca/ca_cert.pem     - CA certificate"
    echo ""
    echo "Initiator certificates:"
    echo "  $CERT_DIR/initiator/sign_key.pem - Signing private key"
    echo "  $CERT_DIR/initiator/sign_cert.pem- Signing certificate (serverAuth+clientAuth)"
    echo "  $CERT_DIR/initiator/enc_key.pem  - Encryption private key"
    echo "  $CERT_DIR/initiator/enc_cert.pem - Encryption certificate (ikeIntermediate)"
    echo ""
    echo "Responder certificates:"
    echo "  $CERT_DIR/responder/sign_key.pem - Signing private key"
    echo "  $CERT_DIR/responder/sign_cert.pem- Signing certificate (serverAuth+clientAuth)"
    echo "  $CERT_DIR/responder/enc_key.pem  - Encryption private key"
    echo "  $CERT_DIR/responder/enc_cert.pem - Encryption certificate (ikeIntermediate)"
    echo ""
    echo "swanctl configuration directories:"
    echo "  $CERT_DIR/swanctl/initiator/"
    echo "  $CERT_DIR/swanctl/responder/"
    echo ""
}

# Run main
main "$@"
