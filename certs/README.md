# PQ-GM-IKEv2 Test Certificates

This directory contains test certificates for the PQ-GM-IKEv2 protocol implementation.

## Certificate Types

The protocol uses three types of certificates:

| Type | Algorithm | Purpose | Phase |
|------|-----------|---------|-------|
| SignCert | SM2-with-SM3 | Digital signature for IKE_AUTH | IKE_AUTH |
| EncCert | SM2-with-SM3 | Key encapsulation for SM2-KEM | IKE_INTERMEDIATE |
| AuthCert | SPHINCS+-SM3 | Post-quantum authentication | IKE_AUTH |

## Directory Structure

```
certs/
├── ca/
│   ├── ca_sm2_key.pem       # SM2 CA private key (encrypted)
│   ├── ca_sm2_cert.pem      # SM2 CA certificate (10 years validity)
│   └── ca_sm2_pubkey.pem    # SM2 CA public key
├── initiator/
│   ├── sign_key.pem         # SM2 signing private key
│   ├── sign_cert.pem        # SM2 signing certificate
│   ├── sign_pubkey.pem      # SM2 signing public key
│   ├── enc_key.pem          # SM2 encryption private key
│   ├── enc_cert.pem         # SM2 encryption certificate
│   ├── enc_pubkey.pem       # SM2 encryption public key
│   ├── auth_key.pem         # SPHINCS+ authentication private key
│   └── auth_pubkey.pem      # SPHINCS+ authentication public key
└── responder/
    ├── sign_key.pem         # SM2 signing private key
    ├── sign_cert.pem        # SM2 signing certificate
    ├── sign_pubkey.pem      # SM2 signing public key
    ├── enc_key.pem          # SM2 encryption private key
    ├── enc_cert.pem         # SM2 encryption certificate
    ├── enc_pubkey.pem       # SM2 encryption public key
    ├── auth_key.pem         # SPHINCS+ authentication private key
    └── auth_pubkey.pem      # SPHINCS+ authentication public key
```

## Certificate Details

### CA Certificate
- **Subject**: CN=PQGM-SM2-CA, O=PQGM-Test, L=Haidian, ST=Beijing, C=CN
- **Algorithm**: SM2-with-SM3
- **Validity**: 10 years
- **Key Usage**: Certificate Sign, CRL Sign
- **Basic Constraints**: CA:TRUE, pathlen:2

### SignCert (Signing Certificate)
- **Subject**: CN=<role>.pqgm-sign, O=PQGM-Test, L=Haidian, ST=Beijing, C=CN
- **Algorithm**: SM2-with-SM3
- **Validity**: 1 year
- **Key Usage**: Digital Signature, Non Repudiation
- **Extended Key Usage**: TLS Web Server Authentication

### EncCert (Encryption Certificate)
- **Subject**: CN=<role>.pqgm-enc, O=PQGM-Test, L=Haidian, ST=Beijing, C=CN
- **Algorithm**: SM2-with-SM3
- **Validity**: 1 year
- **Key Usage**: Key Encipherment, Data Encipherment
- **Extended Key Usage**: TLS Web Server Authentication

### AuthCert (Post-Quantum Authentication)
- **Algorithm**: SPHINCS+-128s-SM3 (64-byte private key, 32-byte public key)
- **Note**: Raw key format, certificate not yet generated

## Generation

To regenerate all certificates:

```bash
cd /home/ipsec/PQGM-IPSec
./scripts/gen_certs.sh
```

### Prerequisites
- GmSSL 3.1.3+ (for SM2 certificate generation)
- OpenSSL 3.0+ (for certificate verification)

## Verification Commands

### Verify SM2 Certificate with OpenSSL
```bash
# View certificate details
openssl x509 -in certs/ca/ca_sm2_cert.pem -text -noout

# Verify certificate chain
openssl verify -CAfile certs/ca/ca_sm2_cert.pem certs/initiator/sign_cert.pem
```

### Verify with GmSSL
```bash
# Parse certificate
gmssl certparse -in certs/initiator/sign_cert.pem

# Verify certificate chain
gmssl certverify -cert certs/initiator/sign_cert.pem -cacert certs/ca/ca_sm2_cert.pem
```

### Verify SM2 Signature
```bash
# Extract public key
gmssl sm2keygen -pass PQGM2026 -out test.pem -pubout test_pub.pem

# Sign and verify
echo "test message" | gmssl sm3 -binary | gmssl sm2sign -key test.pem -pass PQGM2026 -out sig.bin
echo "test message" | gmssl sm3 -binary | gmssl sm2verify -key test_pub.pem -sig sig.bin
```

## Certificate Password

All encrypted private keys use password: `PQGM2026`

## Usage in strongSwan

Example swanctl configuration for initiator:

```conf
connections {
    pqgm {
        remote_addrs = 192.168.1.2
        local {
            auth = pubkey
            certs = sign_cert.pem
            # Additional certificates for IKE_INTERMEDIATE
        }
        remote {
            auth = pubkey
            # Verify with CA certificate
        }
        children {
            pqgm {
                esp_proposals = sm4-cbc-sm3
            }
        }
    }
}

authorities {
    pqgm-ca {
        cacert = ca_sm2_cert.pem
    }
}
```

## Security Notes

1. These are **TEST CERTIFICATES** only - do not use in production
2. Private keys are password-protected but use a simple password
3. CA certificate has pathlen:2, allowing for intermediate CAs if needed
4. SPHINCS+ keys are raw binary format (no PEM wrapper)

## Post-Quantum Authentication Status

The current implementation uses SPHINCS+-SM3 from GmSSL for post-quantum signatures.
For production use with ML-DSA (FIPS 204), consider:

1. **oqs-provider** for OpenSSL 3.x - provides ML-KEM and ML-DSA
2. **liboqs** - reference implementation of NIST PQC algorithms
3. **strongSwan's ml plugin** - already integrated for ML-KEM

## References

- GM/T 0002-2012: SM2 Elliptic Curve Public Key Cryptography
- GM/T 0003-2012: SM3 Cryptographic Hash Algorithm
- GM/T 0004-2012: SM4 Block Cipher Algorithm
- FIPS 203: Module-Lattice-Based Key-Encapsulation Mechanism Standard (ML-KEM)
- FIPS 204: Module-Lattice-Based Digital Signature Standard (ML-DSA)
- FIPS 205: Stateless Hash-Based Digital Signature Standard (SLH-DSA)
- RFC 9242: Intermediate Exchange in the IKEv2 Protocol
- RFC 9370: Multiple Key Exchanges in the IKEv2 Protocol
