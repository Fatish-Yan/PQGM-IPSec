# RFC 9370 Key Derivation Issue Analysis

## Problem Statement
When using SM2-KEM (or any additional KE) with initial IKE SA, the AUTH verification fails. The additional KE shared secret is not incorporated into the IKE SA key material derivation.

## Root Cause Analysis

### StrongSwan Key Derivation Flow

1. **IKE_SA_INIT (initial SA)**:
   - x25519 KE is processed
   - `derive_ike_keys()` is called with `kes = [x25519]`
   - SKEYSEED = prf(Ni | Nr, g^ir) - **only x25519 shared secret**
   - Keys SK_d, SK_ai, SK_ar, SK_ei, SK_er, SK_pi, SK_pr are derived

2. **IKE_INTERMEDIATE (RFC 9370)**:
   - SM2-KEM, ML-KEM KEs are processed
   - `derive_ike_keys()` is **NOT called** for initial SA
   - Shared secrets from additional KEs are **NOT incorporated**

3. **IKE_AUTH**:
   - AUTH payload computed with SK_pi/SK_pr
   - AUTH verification fails because keys don't include SM2-KEM/ML-KEM contributions

### Code Evidence

In `ike_init.c`, all calls to `derive_keys()` are conditioned on `this->old_sa` (rekeying):
```c
if (key_exchange_done(this) != NEED_MORE && this->old_sa)
{
    /* during rekeying, we derive keys once all exchanges are done */
    if (derive_keys(this) != SUCCESS)
    ...
}
```

For initial SA (`!this->old_sa`), this condition is never true, so `derive_keys()` is never called after IKE_INTERMEDIATE exchanges.

### RFC 9370 Requirement

According to RFC 9370 Section 2.2:
```
SKEYSEED = prf(Ni | Nr, g^ir | KEM1_SS | KEM2_SS | ...)
```

The additional KE shared secrets (KEM1_SS, KEM2_SS) must be included in the SKEYSEED derivation.

## Attempted Fixes

1. **Fixed KE storage**: Modified `key_exchange_done()` to store all KEs in `this->kes` array
2. **Fixed derive_keys timing**: Removed `this->old_sa` condition to call `derive_keys()` for initial SA
3. **Fixed keymat derivation**: Added code to include `add_secret` in SKEYSEED derivation

### Results
- Fix attempt caused "MAC verification failed" on IKE_INTERMEDIATE messages
- This indicates the key derivation timing is still wrong
- IKE_INTERMEDIATE messages are encrypted with initial keys, not re-derived keys

## Architectural Issue

The fundamental issue is that strongSwan's architecture derives keys AFTER IKE_SA_INIT but BEFORE IKE_INTERMEDIATE. This is incompatible with RFC 9370 which requires keys to be derived AFTER ALL exchanges are complete.

### Current Flow (strongSwan)
```
IKE_SA_INIT → derive_keys() → IKE_INTERMEDIATE → IKE_AUTH
              (x25519 only)    (no key update)
```

### Required Flow (RFC 9370)
```
IKE_SA_INIT → IKE_INTERMEDIATE → derive_keys() → IKE_AUTH
              (collect all KEs)   (all KE shared secrets)
```

## Recommended Fix

1. **Delay initial SA key derivation** until after all IKE_INTERMEDIATE exchanges
2. **Or** implement incremental key derivation that updates SK_* keys with additional KE contributions
3. **Ensure** IKE_INTERMEDIATE messages are encrypted with a separate key (or not encrypted at all during initial exchange)

## Files Involved

- `/home/ipsec/strongswan/src/libcharon/sa/ikev2/tasks/ike_init.c` - Task flow and derive_keys calls
- `/home/ipsec/strongswan/src/libcharon/sa/ikev2/keymat_v2.c` - Key derivation implementation
- `/home/ipsec/strongswan/src/libstrongswan/crypto/key_exchange.c` - KE shared secret concatenation

## Working Components

Despite this issue, the following components work correctly:
- SM2-KEM key encapsulation/decapsulation
- Shared secret computation (both sides compute identical SK = r_i || r_r)
- IKE_INTERMEDIATE message flow (cert exchange, KE payloads)
- Basic x25519 authentication (PSK and pubkey)

The issue is purely in the integration of additional KE shared secrets into the IKE SA key material.

## Temporary Workaround

For testing purposes, using x25519 only (without additional KEs) works correctly:
```
proposals = aes256-sha256-x25519
```

This allows the rest of the system to be tested while the RFC 9370 key derivation issue is being resolved.
