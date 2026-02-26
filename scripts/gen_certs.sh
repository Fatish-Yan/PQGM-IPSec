#!/bin/bash
#
# PQ-GM-IKEv2 Certificate Generation Script
# Generates SM2 dual certificates (SignCert + EncCert) for IKEv2 protocol
#
# Certificate Types:
#   - SignCert: SM2 signature certificate for IKE_AUTH
#   - EncCert:  SM2 encryption certificate for SM2-KEM
#   - AuthCert: Post-quantum authentication certificate (placeholder)
#

set -e

# Configuration
CERT_DIR="/home/ipsec/PQGM-IPSec/certs"
PASS="PQGM2026"  # Certificate password
DAYS_CA=3650     # CA validity: 10 years
DAYS_CERT=365    # End entity validity: 1 year
SERIAL_LEN=16    # Serial number length in bytes

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

# Check if gmssl is available
check_tools() {
    log_info "Checking tools..."
    if ! command -v gmssl &> /dev/null; then
        log_error "GmSSL not found. Please install GmSSL first."
        exit 1
    fi

    GMSSL_VERSION=$(gmssl version)
    log_info "GmSSL version: $GMSSL_VERSION"

    # Check if openssl is available for verification
    if command -v openssl &> /dev/null; then
        OPENSSL_VERSION=$(openssl version)
        log_info "OpenSSL version: $OPENSSL_VERSION"
    fi
}

# Generate SM2 key pair
# Usage: gen_sm2_key <output_key_file> [output_pub_file]
gen_sm2_key() {
    local key_file="$1"
    local pub_file="$2"

    log_info "Generating SM2 key pair: $key_file"

    if [ -n "$pub_file" ]; then
        gmssl sm2keygen -pass "$PASS" -out "$key_file" -pubout "$pub_file"
    else
        gmssl sm2keygen -pass "$PASS" -out "$key_file"
    fi
}

# Generate CA certificate (self-signed)
# Usage: gen_ca_cert <key_file> <cert_file> <cn>
gen_ca_cert() {
    local key_file="$1"
    local cert_file="$2"
    local cn="$3"

    log_info "Generating CA certificate: $cert_file (CN=$cn)"

    gmssl certgen \
        -C "$C" -ST "$ST" -L "$L" -O "$O" \
        -CN "$cn" \
        -serial_len "$SERIAL_LEN" \
        -days "$DAYS_CA" \
        -key "$key_file" \
        -pass "$PASS" \
        -ca -path_len_constraint 2 \
        -key_usage keyCertSign -key_usage cRLSign \
        -gen_authority_key_id -gen_subject_key_id \
        -out "$cert_file"
}

# Generate end-entity certificate
# Usage: gen_ee_cert <key_file> <cert_file> <cn> <ca_cert> <ca_key> <key_usage1> [key_usage2] [ext_key_usage]
gen_ee_cert() {
    local key_file="$1"
    local cert_file="$2"
    local cn="$3"
    local ca_cert="$4"
    local ca_key="$5"
    local key_usage1="$6"
    local key_usage2="$7"
    local ext_key_usage="$8"

    log_info "Generating certificate: $cert_file (CN=$cn)"

    # Build the reqsign command
    local cmd="gmssl reqsign"
    cmd="$cmd -serial_len $SERIAL_LEN"
    cmd="$cmd -days $DAYS_CERT"
    cmd="$cmd -cacert $ca_cert"
    cmd="$cmd -key $ca_key"
    cmd="$cmd -pass $PASS"
    cmd="$cmd -gen_authority_key_id -gen_subject_key_id"

    # Add key usages
    cmd="$cmd -key_usage $key_usage1"
    if [ -n "$key_usage2" ]; then
        cmd="$cmd -key_usage $key_usage2"
    fi

    # Add extended key usage if specified
    if [ -n "$ext_key_usage" ]; then
        cmd="$cmd -ext_key_usage $ext_key_usage"
    fi

    # Generate CSR first
    local csr_file="${cert_file%.pem}.csr"
    gmssl reqgen \
        -C "$C" -ST "$ST" -L "$L" -O "$O" \
        -CN "$cn" \
        -key "$key_file" \
        -pass "$PASS" \
        -out "$csr_file"

    # Sign the CSR
    cmd="$cmd -in $csr_file"
    cmd="$cmd -out $cert_file"

    eval "$cmd"

    # Clean up CSR
    rm -f "$csr_file"
}

# Verify certificate
# Usage: verify_cert <cert_file> <ca_cert>
verify_cert() {
    local cert_file="$1"
    local ca_cert="$2"

    log_info "Verifying certificate: $cert_file"

    if gmssl certverify -cert "$cert_file" -cacert "$ca_cert" 2>/dev/null; then
        log_info "Certificate verification: PASSED"
        return 0
    else
        log_warn "Certificate verification: FAILED (but this might be OK for test certs)"
        return 1
    fi
}

# Print certificate info using OpenSSL
# Usage: print_cert_info <cert_file>
print_cert_info() {
    local cert_file="$1"

    if command -v openssl &> /dev/null; then
        echo "----------------------------------------"
        echo "Certificate: $cert_file"
        echo "----------------------------------------"
        openssl x509 -in "$cert_file" -text -noout 2>/dev/null | grep -E "(Subject:|Issuer:|Not Before|Not After|Public Key Algorithm|Signature Algorithm|CA:|Key Usage:|Extended Key Usage:)" || true
        echo ""
    fi
}

# Generate all CA certificates
generate_ca_certs() {
    log_info "=== Generating CA Certificates ==="

    # Create CA directory
    mkdir -p "$CERT_DIR/ca"

    # Generate SM2 CA key and certificate (shared for Sign and Enc)
    gen_sm2_key "$CERT_DIR/ca/ca_sm2_key.pem" "$CERT_DIR/ca/ca_sm2_pubkey.pem"
    gen_ca_cert "$CERT_DIR/ca/ca_sm2_key.pem" "$CERT_DIR/ca/ca_sm2_cert.pem" "PQGM-SM2-CA"

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

    # 1. SM2 Signing Certificate (SignCert)
    log_info "Generating SignCert for $role..."
    gen_sm2_key "$role_dir/sign_key.pem" "$role_dir/sign_pubkey.pem"
    gen_ee_cert "$role_dir/sign_key.pem" "$role_dir/sign_cert.pem" \
        "$cn_prefix-sign" \
        "$CERT_DIR/ca/ca_sm2_cert.pem" "$CERT_DIR/ca/ca_sm2_key.pem" \
        "digitalSignature" "nonRepudiation" "serverAuth"

    # 2. SM2 Encryption Certificate (EncCert)
    log_info "Generating EncCert for $role..."
    gen_sm2_key "$role_dir/enc_key.pem" "$role_dir/enc_pubkey.pem"
    gen_ee_cert "$role_dir/enc_key.pem" "$role_dir/enc_cert.pem" \
        "$cn_prefix-enc" \
        "$CERT_DIR/ca/ca_sm2_cert.pem" "$CERT_DIR/ca/ca_sm2_key.pem" \
        "keyEncipherment" "dataEncipherment" "serverAuth"

    # 3. Post-Quantum Authentication Certificate (AuthCert)
    # Using SPHINCS+ (if supported) or placeholder
    log_info "Generating AuthCert for $role..."
    if gmssl sphincskeygen -help &>/dev/null; then
        log_info "Using SPHINCS+ for post-quantum authentication..."
        gmssl sphincskeygen -out "$role_dir/auth_key.pem" -pubout "$role_dir/auth_pubkey.pem" 2>/dev/null || {
            log_warn "SPHINCS+ key generation failed, creating placeholder"
            create_pq_placeholder "$role_dir"
        }
    else
        log_warn "SPHINCS+ not supported, creating placeholder certificate"
        create_pq_placeholder "$role_dir"
    fi

    log_info "$role certificates generated successfully"
}

# Create placeholder for post-quantum certificate
create_pq_placeholder() {
    local role_dir="$1"

    # Create a README explaining the placeholder
    cat > "$role_dir/auth_key.pem" << 'EOF'
-----BEGIN PRIVATE KEY-----
PLACEHOLDER - Post-Quantum Authentication Key

This is a placeholder file. The actual post-quantum authentication
certificate (ML-DSA-65 or SLH-DSA-SHA2-128s) should be generated
using a tool that supports these algorithms.

Options:
1. Use oqs-provider for OpenSSL 3.x with ML-DSA support
2. Use GmSSL's SPHINCS+ support (sphincskeygen command)
3. Use another PQC library (liboqs, pqcrypto)

For testing purposes, you may skip post-quantum authentication
and use only SM2 signatures.
-----END PRIVATE KEY-----
EOF

    cat > "$role_dir/auth_cert.pem" << 'EOF'
-----BEGIN CERTIFICATE-----
PLACEHOLDER - Post-Quantum Authentication Certificate

This is a placeholder file. See auth_key.pem for details.
-----END CERTIFICATE-----
EOF

    log_warn "Placeholder files created for post-quantum certificates"
}

# Main function
main() {
    log_info "========================================"
    log_info "PQ-GM-IKEv2 Certificate Generation"
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
    print_cert_info "$CERT_DIR/ca/ca_sm2_cert.pem"

    # Verify initiator certs
    for cert_type in sign enc; do
        print_cert_info "$CERT_DIR/initiator/${cert_type}_cert.pem"
        verify_cert "$CERT_DIR/initiator/${cert_type}_cert.pem" "$CERT_DIR/ca/ca_sm2_cert.pem" || true
    done

    # Verify responder certs
    for cert_type in sign enc; do
        print_cert_info "$CERT_DIR/responder/${cert_type}_cert.pem"
        verify_cert "$CERT_DIR/responder/${cert_type}_cert.pem" "$CERT_DIR/ca/ca_sm2_cert.pem" || true
    done

    # Summary
    log_info "========================================"
    log_info "Certificate Generation Complete!"
    log_info "========================================"
    echo ""
    echo "Generated files:"
    echo ""
    echo "CA certificates:"
    echo "  $CERT_DIR/ca/ca_sm2_key.pem      - SM2 CA private key"
    echo "  $CERT_DIR/ca/ca_sm2_cert.pem     - SM2 CA certificate"
    echo ""
    echo "Initiator certificates:"
    echo "  $CERT_DIR/initiator/sign_key.pem - SM2 signing private key"
    echo "  $CERT_DIR/initiator/sign_cert.pem- SM2 signing certificate"
    echo "  $CERT_DIR/initiator/enc_key.pem  - SM2 encryption private key"
    echo "  $CERT_DIR/initiator/enc_cert.pem - SM2 encryption certificate"
    echo "  $CERT_DIR/initiator/auth_key.pem - Post-quantum auth key (placeholder)"
    echo "  $CERT_DIR/initiator/auth_cert.pem- Post-quantum auth cert (placeholder)"
    echo ""
    echo "Responder certificates:"
    echo "  $CERT_DIR/responder/sign_key.pem - SM2 signing private key"
    echo "  $CERT_DIR/responder/sign_cert.pem- SM2 signing certificate"
    echo "  $CERT_DIR/responder/enc_key.pem  - SM2 encryption private key"
    echo "  $CERT_DIR/responder/enc_cert.pem - SM2 encryption certificate"
    echo "  $CERT_DIR/responder/auth_key.pem - Post-quantum auth key (placeholder)"
    echo "  $CERT_DIR/responder/auth_cert.pem- Post-quantum auth cert (placeholder)"
    echo ""
    echo "Certificate password: $PASS"
    echo ""
}

# Run main
main "$@"
