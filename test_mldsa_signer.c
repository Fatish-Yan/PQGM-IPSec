/*
 * test_mldsa_signer.c - ML-DSA-65 Signer Unit Test
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <oqs/oqs.h>

#define MLDSA65_PUBLIC_KEY_BYTES  1952
#define MLDSA65_SECRET_KEY_BYTES  4032
#define MLDSA65_SIGNATURE_BYTES   3309

int main(void)
{
	OQS_SIG *sig = NULL;
	uint8_t *public_key = NULL;
	uint8_t *secret_key = NULL;
	uint8_t *signature = NULL;
	uint8_t message[] = "Hello, ML-DSA-65!";
	size_t message_len = sizeof(message) - 1;
	size_t sig_len;
	int ret = 1;

	printf("=== ML-DSA-65 Signer Unit Test ===\n\n");

	/* Step 1: Create ML-DSA-65 context */
	printf("1. Creating ML-DSA-65 context...\n");
	sig = OQS_SIG_new(OQS_SIG_alg_ml_dsa_65);
	if (!sig) {
		printf("   FAIL: Failed to create OQS_SIG context\n");
		goto cleanup;
	}
	printf("   OK: Created context for %s\n", sig->method_name);
	printf("   Public key size: %zu bytes\n", sig->length_public_key);
	printf("   Secret key size: %zu bytes\n", sig->length_secret_key);
	printf("   Signature size: %zu bytes\n", sig->length_signature);

	/* Verify sizes match our definitions */
	if (sig->length_public_key != MLDSA65_PUBLIC_KEY_BYTES) {
		printf("   FAIL: Public key size mismatch\n");
		goto cleanup;
	}
	if (sig->length_secret_key != MLDSA65_SECRET_KEY_BYTES) {
		printf("   FAIL: Secret key size mismatch\n");
		goto cleanup;
	}
	if (sig->length_signature != MLDSA65_SIGNATURE_BYTES) {
		printf("   FAIL: Signature size mismatch\n");
		goto cleanup;
	}

	/* Step 2: Allocate key buffers */
	printf("\n2. Allocating key buffers...\n");
	public_key = malloc(sig->length_public_key);
	secret_key = malloc(sig->length_secret_key);
	signature = malloc(sig->length_signature);
	if (!public_key || !secret_key || !signature) {
		printf("   FAIL: Memory allocation failed\n");
		goto cleanup;
	}
	printf("   OK: Buffers allocated\n");

	/* Step 3: Generate keypair */
	printf("\n3. Generating keypair...\n");
	if (OQS_SIG_keypair(sig, public_key, secret_key) != OQS_SUCCESS) {
		printf("   FAIL: Keypair generation failed\n");
		goto cleanup;
	}
	printf("   OK: Keypair generated\n");

	/* Step 4: Sign message */
	printf("\n4. Signing message: \"%s\"\n", message);
	sig_len = sig->length_signature;
	if (OQS_SIG_sign(sig, signature, &sig_len,
					 message, message_len, secret_key) != OQS_SUCCESS) {
		printf("   FAIL: Signature generation failed\n");
		goto cleanup;
	}
	printf("   OK: Signature generated (%zu bytes)\n", sig_len);

	/* Step 5: Verify signature (should succeed) */
	printf("\n5. Verifying valid signature...\n");
	if (OQS_SIG_verify(sig, message, message_len,
					   signature, sig_len, public_key) != OQS_SUCCESS) {
		printf("   FAIL: Valid signature verification failed\n");
		goto cleanup;
	}
	printf("   OK: Valid signature verified\n");

	/* Step 6: Verify with wrong message (should fail) */
	printf("\n6. Verifying with tampered message...\n");
	uint8_t tampered[] = "Hello, ML-DSA-66!";
	if (OQS_SIG_verify(sig, tampered, sizeof(tampered) - 1,
					   signature, sig_len, public_key) == OQS_SUCCESS) {
		printf("   FAIL: Tampered message should NOT verify\n");
		goto cleanup;
	}
	printf("   OK: Tampered message correctly rejected\n");

	/* Step 7: Verify with wrong signature (should fail) */
	printf("\n7. Verifying with tampered signature...\n");
	signature[0] ^= 0xFF;  /* Flip bits */
	if (OQS_SIG_verify(sig, message, message_len,
					   signature, sig_len, public_key) == OQS_SUCCESS) {
		printf("   FAIL: Tampered signature should NOT verify\n");
		goto cleanup;
	}
	printf("   OK: Tampered signature correctly rejected\n");

	printf("\n=== ALL TESTS PASSED ===\n");
	ret = 0;

cleanup:
	if (sig) OQS_SIG_free(sig);
	if (public_key) free(public_key);
	if (secret_key) free(secret_key);
	if (signature) free(signature);
	return ret;
}
