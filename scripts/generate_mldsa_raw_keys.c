/*
 * Generate ML-DSA-65 raw keypairs for testing
 *
 * This is a fallback script for systems without OpenSSL 3.5+ oqs-provider
 * It generates raw ML-DSA-65 keypairs that can be used for testing
 *
 * Compile:
 *   gcc -o generate_mldsa_raw_keys generate_mldsa_raw_keys.c -loqs
 *
 * Usage:
 *   ./generate_mldsa_raw_keys
 *
 * Output:
 *   initiator_mldsa_public.bin  (1952 bytes)
 *   initiator_mldsa_private.bin (4032 bytes)
 *   responder_mldsa_public.bin  (1952 bytes)
 *   responder_mldsa_private.bin (4032 bytes)
 *
 * ML-DSA-65 key sizes (FIPS 204):
 *   Public key: 1952 bytes
 *   Private key: 4032 bytes
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <oqs/oqs.h>

#define ML_DSA_65_PUBLIC_KEY_SIZE 1952
#define ML_DSA_65_SECRET_KEY_SIZE 4032

int main(void) {
    OQS_SIG *sig = NULL;
    uint8_t public_key[ML_DSA_65_PUBLIC_KEY_SIZE];
    uint8_t secret_key[ML_DSA_65_SECRET_KEY_SIZE];
    FILE *f = NULL;
    int ret = 1;

    printf("=== ML-DSA-65 Raw Key Generation ===\n");
    printf("liboqs version: %s\n\n", OQS_version());

    /* Initialize liboqs */
    OQS_init();  /* void function in liboqs 0.12.0 */

    /* Create ML-DSA-65 context */
    sig = OQS_SIG_new(OQS_SIG_alg_ml_dsa_65);
    if (!sig) {
        fprintf(stderr, "ERROR: Failed to create ML-DSA-65 context\n");
        fprintf(stderr, "ML-DSA may not be enabled in liboqs build\n");
        OQS_destroy();
        return 1;
    }

    printf("Algorithm: %s\n", sig->method_name);
    printf("Public key size: %zu bytes\n", sig->length_public_key);
    printf("Secret key size: %zu bytes\n", sig->length_secret_key);
    printf("Signature size: %zu bytes\n\n", sig->length_signature);

    /* Generate keypair */
    if (OQS_SIG_keypair(sig, public_key, secret_key) != OQS_SUCCESS) {
        fprintf(stderr, "ERROR: Failed to generate keypair\n");
        goto cleanup;
    }

    /* Write initiator keys */
    printf("Writing initiator keys...\n");
    f = fopen("initiator_mldsa_public.bin", "wb");
    if (!f) {
        perror("ERROR: Failed to create initiator_mldsa_public.bin");
        goto cleanup;
    }
    fwrite(public_key, 1, ML_DSA_65_PUBLIC_KEY_SIZE, f);
    fclose(f);

    f = fopen("initiator_mldsa_private.bin", "wb");
    if (!f) {
        perror("ERROR: Failed to create initiator_mldsa_private.bin");
        goto cleanup;
    }
    fwrite(secret_key, 1, ML_DSA_65_SECRET_KEY_SIZE, f);
    fclose(f);

    /* Write responder keys */
    printf("Writing responder keys...\n");
    f = fopen("responder_mldsa_public.bin", "wb");
    if (!f) {
        perror("ERROR: Failed to create responder_mldsa_public.bin");
        goto cleanup;
    }
    fwrite(public_key, 1, ML_DSA_65_PUBLIC_KEY_SIZE, f);
    fclose(f);

    f = fopen("responder_mldsa_private.bin", "wb");
    if (!f) {
        perror("ERROR: Failed to create responder_mldsa_private.bin");
        goto cleanup;
    }
    fwrite(secret_key, 1, ML_DSA_65_SECRET_KEY_SIZE, f);
    fclose(f);

    printf("\nML-DSA-65 raw keys generated successfully\n");
    printf("\nFiles created:\n");
    printf("  initiator_mldsa_public.bin  (%d bytes)\n", ML_DSA_65_PUBLIC_KEY_SIZE);
    printf("  initiator_mldsa_private.bin (%d bytes)\n", ML_DSA_65_SECRET_KEY_SIZE);
    printf("  responder_mldsa_public.bin  (%d bytes)\n", ML_DSA_65_PUBLIC_KEY_SIZE);
    printf("  responder_mldsa_private.bin (%d bytes)\n", ML_DSA_65_SECRET_KEY_SIZE);

    ret = 0;

cleanup:
    if (sig) {
        OQS_SIG_free(sig);
    }
    OQS_destroy();  /* void function in liboqs 0.12.0 */
    return ret;
}
