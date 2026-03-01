# PQ-GM-IKEv2 Implementation Final Report

## Date: 2026-03-01

## Summary

This report documents the implementation of SM2-KEM (Chinese commercial cryptography) in the strongSwan 6.0.4 IKEv2 stack for the PQ-GM-IKEv2 5-RTT protocol.

## Achievements

### 1. SM2-KEM Core Implementation ✅ COMPLETE

**File**: `strongswan/src/libstrongswan/plugins/gmalg/gmalg_ke.c`

- Implemented bidirectional SM2-KEM using GmSSL library
- Shared secret derivation: `SK = r_i || r_r` (64 bytes)
- File fallback mechanism for SM2 keys:
  - Public key: `/usr/local/etc/swanctl/x509/peer_sm2_pubkey.pem`
  - Private key: `/usr/local/etc/swanctl/private/sm2_enc_key.pem`
  - Password: `PQGM2026`

**Verification**:
```
Initiator: SK = 6fb3f9d240b0444c... (first 8 bytes)
Responder: SK = 6fb3f9d240b0444c... (MATCHES!)
```

### 2. Protocol Integration ✅ COMPLETE

**File**: `strongswan/src/libcharon/sa/ikev2/tasks/ike_init.c`

- Modified to support 5-RTT protocol flow:
  1. IKE_SA_INIT: x25519 KE (msg_id=0)
  2. IKE_INTERMEDIATE #0: Certificate exchange (msg_id=1)
  3. IKE_INTERMEDIATE #1: SM2-KEM (msg_id=2)
  4. IKE_INTERMEDIATE #2: ML-KEM-768 (msg_id=3)
  5. IKE_AUTH: ED25519 signature (msg_id=4)

- SM2 certificate exchange in IKE_INTERMEDIATE #0:
  - SignCert: SM2 signature certificate
  - EncCert: SM2 encryption certificate

### 3. Certificate Support ✅ COMPLETE

**Files**:
- `docker/initiator/certs/x509/signCert.pem` - Initiator's SM2 sign cert
- `docker/initiator/certs/x509/encCert.pem` - Initiator's SM2 enc cert
- `docker/responder/certs/x509/signCert.pem` - Responder's SM2 sign cert
- `docker/responder/certs/x509/encCert.pem` - Responder's SM2 enc cert

### 4. Docker Test Environment ✅ COMPLETE

**File**: `docker/docker-compose.yml`

- Initiator: 172.28.0.10
- Responder: 172.28.0.20
- Isolated network for testing

## Test Results

### Passing Tests

| Test Case | Result | Time |
|-----------|--------|------|
| x25519 + PSK | ✅ PASS | ~43ms |
| x25519 + Pubkey | ✅ PASS | ~50ms |
| SM2-KEM key exchange | ✅ PASS | N/A |
| SM2-KEM shared secret | ✅ MATCH | N/A |

### Failing Tests

| Test Case | Result | Error |
|-----------|--------|-------|
| x25519 + SM2-KEM + PSK | ❌ FAIL | AUTH_FAILED |
| x25519 + SM2-KEM + Pubkey | ❌ FAIL | AUTH_FAILED |
| Full 5-RTT (SM2+ML-KEM) | ❌ FAIL | AUTH_FAILED |

## Known Issue: RFC 9370 Key Derivation

**Status**: In Progress

The additional KE shared secrets (SM2-KEM, ML-KEM) are not being incorporated into the IKE SA key material derivation for **initial SA**.

**Root Cause**:
In strongSwan's architecture, `derive_ike_keys()` is called after IKE_SA_INIT but before IKE_INTERMEDIATE. The additional KE shared secrets are collected during IKE_INTERMEDIATE but never used to update the keys.

**Required Fix**:
Modify the key derivation to include all KE shared secrets after all IKE_INTERMEDIATE exchanges are complete.

**See**: `docs/RFC9370-KeyDerivation-Issue.md` for detailed analysis.

## Code Changes

### New Files
- `strongswan/src/libstrongswan/plugins/gmalg/gmalg_ke.c` - SM2-KEM implementation
- `strongswan/src/libstrongswan/plugins/gmalg/gmalg_ke.h` - SM2-KEM API
- `docker/docker-compose.yml` - Test environment
- `docker/initiator/config/*` - Initiator configuration
- `docker/responder/config/*` - Responder configuration

### Modified Files
- `strongswan/src/libcharon/sa/ikev2/tasks/ike_init.c` - 5-RTT protocol flow
- `strongswan/src/libcharon/sa/ikev2/tasks/ike_cert_post.c` - Certificate exchange

## Recommendations for Future Work

1. **Fix RFC 9370 key derivation** for initial SA
2. **Add timing instrumentation** for paper data collection
3. **Implement packet capture** for Wireshark analysis
4. **Test with real GM certificates** from Chinese CA
5. **Performance benchmarking** vs. pure x25519

## References

- RFC 7296: IKEv2
- RFC 9242: IKE_INTERMEDIATE
- RFC 9370: Multiple Key Exchanges
- FIPS 203: ML-KEM
- GM/T 0002-0004-2012: SM2/SM3/SM4
- GmSSL: https://github.com/guanzhi/GmSSL

## Contact

For questions about this implementation, refer to the source code in:
- `/home/ipsec/strongswan/src/libstrongswan/plugins/gmalg/`
- `/home/ipsec/PQGM-IPSec/docker/`
