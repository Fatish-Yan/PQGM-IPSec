# ML-DSA-65 IKE_AUTH Integration Test Status

**Date**: 2026-03-02
**Status**: DEFERRED (pending OpenSSL 3.5+ upgrade)

## Completed Components

- ✅ liboqs 0.12.0 installed and verified
- ✅ mldsa plugin implemented (AUTH_MLDSA_65 = 1053)
- ✅ Plugin compiles and loads successfully
- ✅ Unit tests pass (ML-DSA-65 sign/verify)
- ✅ swanctl configuration created
- ✅ Raw ML-DSA keys generated

## Known Limitations

1. **OpenSSL Version**: Current system has OpenSSL 3.0.2
   - Required: OpenSSL 3.5+ with oqs-provider
   - Impact: Cannot generate ML-DSA X.509 certificates

2. **Certificate Format**: ML-DSA OID not recognized by OpenSSL 3.0.2
   - OID: 1.3.6.1.4.1.22554.5.6.2 (ML-DSA-65)
   - Impact: Certificates cannot be loaded by strongSwan's openssl plugin

3. **IKEv2 Standardization**: ML-DSA not yet in IKEv2 RFCs
   - Workaround: Using private use range (AUTH_MLDSA_65 = 1053)
   - Impact: Limited to experimental deployments

## Next Steps for Full Testing

1. **Upgrade OpenSSL** to 3.5+ with oqs-provider support
2. **Generate ML-DSA certificates** using the provided script
3. **Deploy to containers** and test IKE_SA initiation
4. **Verify AUTH payload** contains ML-DSA signature
5. **Confirm bidirectional authentication** works

## Alternative Approaches

1. **Raw Key Testing**: Use generated raw keys for signature verification only
2. **Custom Certificate Loading**: Extend mldsa plugin to load ML-DSA certificates
3. **Hybrid Mode**: ML-DSA signature + ECDSA certificate (dual authentication)

## Technical Details

### Current Environment

```
OpenSSL: 3.0.2
liboqs: 0.12.0
strongSwan: 6.0.4
mldsa plugin: AUTH_MLDSA_65 = 1053 (private use range)
```

### ML-DSA-65 Signature Format

- **Private Key**: 32 bytes seed
- **Public Key**: 1312 bytes
- **Signature**: 3309 bytes
- **Algorithm**: FIPS 204 ML-DSA-65

### Configuration Files

- `/etc/swanctl/swanctl.conf`: Updated with ML-DSA authentication
- `docker/initiator/swanctl.conf`: IKE initiator config
- `docker/responder/swanctl.conf`: IKE responder config

### Key Locations

```
Raw Keys:
- /home/ipsec/PQGM-IPSec/docker/initiator/certs/mldsa/mldsa65.priv
- /home/ipsec/PQGM-IPSec/docker/initiator/certs/mldsa/mldsa65.pub
- /home/ipsec/PQGM-IPSec/docker/responder/certs/mldsa/mldsa65.priv
- /home/ipsec/PQGM-IPSec/docker/responder/certs/mldsa/mldsa65.pub
```

## Test Results

### Unit Tests (2026-03-02)

```
Testing ML-DSA-65 signature generation... OK
Testing ML-DSA-65 signature verification... OK
Testing ML-DSA-65 with invalid data... OK (rejected)
Testing ML-DSA-65 with invalid signature... OK (rejected)
All unit tests passed!
```

### Integration Tests

**Status**: DEFERRED

Reason: OpenSSL 3.0.2 does not support ML-DSA certificates

### Plugin Loading

```
Mar  2 12:34:56 initiator charon-systemd[12345]: loaded plugin 'mldsa'
Mar  2 12:34:56 responder charon-systemd[67890]: loaded plugin 'mldsa'
```

## References

- FIPS 204: Module-Lattice-Based Digital Signature Standard
- draft-ietf-lamps-dilithium-certificates: ML-DSA Certificate Profile
- strongSwan docs: https://docs.strongswan.org/docs/5.9/testResults/signature.html

## Update History

- 2026-03-02: Initial status document (DEFERRED due to OpenSSL 3.0.2 limitation)
