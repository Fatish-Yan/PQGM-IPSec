/*
 * Test ML-DSA OID mapping
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <library.h>
#include <asn1/oid.h>
#include <credentials/keys/public_key.h>
#include <credentials/keys/signature_params.h>

int main(int argc, char *argv[])
{
    signature_scheme_t scheme;
    int oid;

    printf("Testing ML-DSA OID mapping:\n\n");

    /* Test SIGN_MLDSA65 -> OID_MLDSA65 */
    printf("1. SIGN_MLDSA65 (%d) -> OID: ", SIGN_MLDSA65);
    oid = signature_scheme_to_oid(SIGN_MLDSA65);
    if (oid == OID_MLDSA65) {
        printf("OID_MLDSA65 (%d) ✓\n", oid);
    } else if (oid == OID_UNKNOWN) {
        printf("OID_UNKNOWN ✗ (FAIL)\n");
    } else {
        printf("Unknown OID %d ✗ (FAIL)\n", oid);
    }

    /* Test OID_MLDSA65 -> SIGN_MLDSA65 */
    printf("2. OID_MLDSA65 (%d) -> SIGN: ", OID_MLDSA65);
    scheme = signature_scheme_from_oid(OID_MLDSA65);
    if (scheme == SIGN_MLDSA65) {
        printf("SIGN_MLDSA65 (%d) ✓\n", scheme);
    } else if (scheme == SIGN_UNKNOWN) {
        printf("SIGN_UNKNOWN ✗ (FAIL)\n");
    } else {
        printf("Unknown scheme %d ✗ (FAIL)\n", scheme);
    }

    printf("\n");

    /* Test signature_params_build */
    printf("3. Testing signature_params_build:\n");
    {
        signature_params_t params = {
            .scheme = SIGN_MLDSA65,
            .params = NULL,
        };
        chunk_t asn1 = chunk_empty;

        if (signature_params_build(&params, &asn1)) {
            printf("   Built ASN.1 successfully (%zu bytes)\n", asn1.len);
            printf("   OID encoding: ");
            for (size_t i = 0; i < asn1.len && i < 20; i++) {
                printf("%02X ", asn1.ptr[i]);
            }
            printf("...\n");
            printf("   ✓\n");
            chunk_free(&asn1);
        } else {
            printf("   ✗ FAILED to build ASN.1\n");
        }
    }

    printf("\nAll tests completed!\n");

    return 0;
}
