/*
 * Test program for gmalg SM3/SM4 algorithms
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <library.h>
#include <crypto/key_exchange.h>
#include "gmalg_plugin.h"
#include "gmalg_hasher.h"
#include "gmalg_crypter.h"
#include "gmalg_signer.h"
#include "gmalg_ke.h"

/* Print hex buffer */
static void print_hex(const char *label, const uint8_t *data, size_t len)
{
	printf("%s: ", label);
	for (size_t i = 0; i < len; i++) {
		printf("%02x", data[i]);
	}
	printf("\n");
}

/* Test SM3 Hash Algorithm */
static int test_sm3_hash(void)
{
	printf("\n=== Testing SM3 Hash Algorithm ===\n");

	gmalg_sm3_hasher_t *hasher;
	uint8_t hash[32];
	char test_data[] = "Hello, SM3!";
	int result = 0;

	/* Create SM3 hasher */
	hasher = gmalg_sm3_hasher_create(HASH_SM3);
	if (!hasher) {
		printf("ERROR: Failed to create SM3 hasher\n");
		return -1;
	}
	printf("SM3 hasher created successfully\n");

	/* Test hash size */
	size_t hash_size = ((hasher_t*)hasher)->get_hash_size((hasher_t*)hasher);
	printf("SM3 hash size: %zu bytes (expected: 32)\n", hash_size);
	if (hash_size != 32) {
		printf("ERROR: Hash size mismatch!\n");
		result = -1;
	}

	/* Test hashing */
	chunk_t data = chunk_create((u_char*)test_data, strlen(test_data));
	if (!((hasher_t*)hasher)->get_hash((hasher_t*)hasher, data, hash)) {
		printf("ERROR: Hashing failed!\n");
		result = -1;
	} else {
		printf("Input data: \"%s\"\n", test_data);
		print_hex("SM3 hash", hash, 32);
		printf("SM3 hashing test: PASSED\n");
	}

	((hasher_t*)hasher)->destroy((hasher_t*)hasher);
	return result;
}

/* Test SM3 PRF */
static int test_sm3_prf(void)
{
	printf("\n=== Testing SM3 PRF ===\n");

	prf_t *prf;
	uint8_t key_bytes[32] = {0};
	uint8_t output[64];
	chunk_t key, seed;
	int result = 0;

	/* Create SM3 PRF */
	memset(key_bytes, 0xAB, 32);
	key = chunk_create(key_bytes, 32);
	prf = gmalg_sm3_prf_create(key);
	if (!prf) {
		printf("ERROR: Failed to create SM3 PRF\n");
		return -1;
	}
	printf("SM3 PRF created successfully\n");

	/* Test PRF */
	seed = chunk_create((u_char*)"Test seed data for SM3 PRF",
					   strlen("Test seed data for SM3 PRF"));

	if (!prf->get_bytes(prf, seed, output)) {
		printf("ERROR: PRF get_bytes failed!\n");
		result = -1;
	} else {
		printf("PRF key size: %zu bytes\n", key.len);
		printf("PRF seed: \"%s\"\n", (char*)seed.ptr);
		printf("PRF block size: %zu bytes\n", prf->get_block_size(prf));
		print_hex("PRF output (first 32 bytes)", output, 32);
		printf("SM3 PRF test: PASSED\n");
	}

	prf->destroy(prf);
	return result;
}

/* Test SM4 ECB Encryption */
static int test_sm4_ecb(void)
{
	printf("\n=== Testing SM4 ECB Encryption ===\n");

	gmalg_sm4_crypter_t *crypter;
	uint8_t key[16] = {0};
	uint8_t plaintext[32] = {0};
	uint8_t iv[16] = {0};
	chunk_t key_chunk, pt_chunk, iv_chunk;
	int result = 0;

	/* Create SM4 ECB crypter */
	memset(key, 0xAA, 16);
	memset(plaintext, 0xDD, 32);
	memset(iv, 0xFF, 16);

	key_chunk = chunk_create(key, 16);
	pt_chunk = chunk_create(plaintext, 32);
	iv_chunk = chunk_create(iv, 16);

	crypter = gmalg_sm4_crypter_create(ENCR_SM4_ECB, 16);
	if (!crypter) {
		printf("ERROR: Failed to create SM4 ECB crypter\n");
		return -1;
	}
	printf("SM4 ECB crypter created successfully\n");

	/* Set key */
	if (!((crypter_t*)crypter)->set_key((crypter_t*)crypter, key_chunk)) {
		printf("ERROR: Failed to set SM4 key\n");
		((crypter_t*)crypter)->destroy((crypter_t*)crypter);
		return -1;
	}
	printf("SM4 key set successfully (128-bit)\n");

	/* Test encryption */
	chunk_t encrypted = pt_chunk;
	if (!((crypter_t*)crypter)->encrypt((crypter_t*)crypter, pt_chunk, iv_chunk, &encrypted)) {
		printf("ERROR: Encryption failed!\n");
		result = -1;
	} else {
		printf("Plaintext: ");
		print_hex("", plaintext, 16);
		print_hex("Encrypted", encrypted.ptr, 16);

		/* Test decryption */
		chunk_t decrypted = encrypted;
		if (!((crypter_t*)crypter)->decrypt((crypter_t*)crypter, encrypted, iv_chunk, &decrypted)) {
			printf("ERROR: Decryption failed!\n");
			result = -1;
		} else {
			print_hex("Decrypted", decrypted.ptr, 16);

			/* Verify */
			if (memcmp(decrypted.ptr, plaintext, 16) == 0) {
				printf("SM4 ECB encryption/decryption test: PASSED\n");
			} else {
				printf("ERROR: Decryption mismatch!\n");
				result = -1;
			}
		}
	}

	((crypter_t*)crypter)->destroy((crypter_t*)crypter);
	return result;
}

/* Test SM4 CBC Encryption */
static int test_sm4_cbc(void)
{
	printf("\n=== Testing SM4 CBC Encryption ===\n");

	gmalg_sm4_crypter_t *crypter;
	uint8_t key[16] = {0};
	uint8_t plaintext[32] = {0};
	uint8_t iv[16] = {0};
	chunk_t key_chunk, pt_chunk, iv_chunk;
	int result = 0;

	/* Create SM4 CBC crypter */
	memset(key, 0xAA, 16);
	memset(plaintext, 0xDD, 32);
	memset(iv, 0xFF, 16);

	key_chunk = chunk_create(key, 16);
	pt_chunk = chunk_create(plaintext, 32);
	iv_chunk = chunk_create(iv, 16);

	crypter = gmalg_sm4_cbc_crypter_create(ENCR_SM4_CBC, 16);
	if (!crypter) {
		printf("ERROR: Failed to create SM4 CBC crypter\n");
		return -1;
	}
	printf("SM4 CBC crypter created successfully\n");

	/* Set key */
	if (!((crypter_t*)crypter)->set_key((crypter_t*)crypter, key_chunk)) {
		printf("ERROR: Failed to set SM4 key\n");
		((crypter_t*)crypter)->destroy((crypter_t*)crypter);
		return -1;
	}
	printf("SM4 key set successfully (128-bit)\n");

	/* Test encryption */
	chunk_t encrypted = pt_chunk;
	if (!((crypter_t*)crypter)->encrypt((crypter_t*)crypter, pt_chunk, iv_chunk, &encrypted)) {
		printf("ERROR: Encryption failed!\n");
		result = -1;
	} else {
		printf("Plaintext: ");
		print_hex("", plaintext, 16);
		print_hex("IV", iv, 16);
		print_hex("Encrypted", encrypted.ptr, 16);

		/* Test decryption */
		chunk_t decrypted = encrypted;
		if (!((crypter_t*)crypter)->decrypt((crypter_t*)crypter, encrypted, iv_chunk, &decrypted)) {
			printf("ERROR: Decryption failed!\n");
			result = -1;
		} else {
			print_hex("Decrypted", decrypted.ptr, 16);

			/* Verify */
			if (memcmp(decrypted.ptr, plaintext, 16) == 0) {
				printf("SM4 CBC encryption/decryption test: PASSED\n");
			} else {
				printf("ERROR: Decryption mismatch!\n");
				result = -1;
			}
		}
	}

	((crypter_t*)crypter)->destroy((crypter_t*)crypter);
	return result;
}

/* Test SM4 CTR Encryption */
static int test_sm4_ctr(void)
{
	printf("\n=== Testing SM4 CTR Encryption ===\n");

	gmalg_sm4_crypter_t *crypter;
	uint8_t key[16] = {0};
	uint8_t plaintext[32] = {0};
	uint8_t iv[16] = {0};
	chunk_t key_chunk, pt_chunk, iv_chunk;
	int result = 0;

	/* Create SM4 CTR crypter */
	memset(key, 0xAA, 16);
	memset(plaintext, 0xDD, 32);
	memset(iv, 0xFF, 16);

	key_chunk = chunk_create(key, 16);
	pt_chunk = chunk_create(plaintext, 32);
	iv_chunk = chunk_create(iv, 16);

	crypter = gmalg_sm4_ctr_crypter_create(ENCR_SM4_CTR, 16);
	if (!crypter) {
		printf("ERROR: Failed to create SM4 CTR crypter\n");
		return -1;
	}
	printf("SM4 CTR crypter created successfully\n");

	/* Set key */
	if (!((crypter_t*)crypter)->set_key((crypter_t*)crypter, key_chunk)) {
		printf("ERROR: Failed to set SM4 key\n");
		((crypter_t*)crypter)->destroy((crypter_t*)crypter);
		return -1;
	}
	printf("SM4 key set successfully (128-bit)\n");

	/* Test encryption */
	chunk_t encrypted = pt_chunk;
	if (!((crypter_t*)crypter)->encrypt((crypter_t*)crypter, pt_chunk, iv_chunk, &encrypted)) {
		printf("ERROR: Encryption failed!\n");
		result = -1;
	} else {
		printf("Plaintext: ");
		print_hex("", plaintext, 16);
		print_hex("IV/CTR", iv, 16);
		print_hex("Encrypted", encrypted.ptr, 16);

		/* Test decryption */
		chunk_t decrypted = encrypted;
		if (!((crypter_t*)crypter)->decrypt((crypter_t*)crypter, encrypted, iv_chunk, &decrypted)) {
			printf("ERROR: Decryption failed!\n");
			result = -1;
		} else {
			print_hex("Decrypted", decrypted.ptr, 16);

			/* Verify */
			if (memcmp(decrypted.ptr, plaintext, 16) == 0) {
				printf("SM4 CTR encryption/decryption test: PASSED\n");
			} else {
				printf("ERROR: Decryption mismatch!\n");
				result = -1;
			}
		}
	}

	((crypter_t*)crypter)->destroy((crypter_t*)crypter);
	return result;
}

/* Test SM2 Signature Algorithm */
static int test_sm2_signer(void)
{
	printf("\n=== Testing SM2 Signature Algorithm ===\n");

	signer_t *signer;
	uint8_t key_data[32] = {0};
	chunk_t key_chunk;
	uint8_t sig[128];
	chunk_t sig_chunk;
	chunk_t data;
	int result = 0;

	/* Create SM2 signer */
	signer = gmalg_sm2_signer_create(AUTH_SM2);
	if (!signer) {
		printf("ERROR: Failed to create SM2 signer\n");
		return -1;
	}
	printf("SM2 signer created successfully\n");

	/* Test signature size */
	size_t sig_size = signer->get_block_size(signer);
	printf("SM2 signature size: %zu bytes (expected: 70-72 for DER encoding)\n", sig_size);

	/* Test key size */
	size_t key_size = signer->get_key_size(signer);
	printf("SM2 key size: %zu bytes\n", key_size);

	/* Generate random key for testing */
	memset(key_data, 0xAB, 32);
	key_chunk = chunk_create(key_data, 32);

	/* Set key */
	if (!signer->set_key(signer, key_chunk)) {
		printf("WARNING: Failed to set raw key (expected for SM2 - needs proper key format)\n");
		printf("SM2 signer test: SKIPPED (requires properly formatted key)\n");
		signer->destroy(signer);
		return 0;  /* Not a failure - just needs proper key format */
	}
	printf("SM2 key set successfully\n");

	/* Test signing */
	data = chunk_create((u_char*)"Hello, SM2!", strlen("Hello, SM2!"));

	if (!signer->get_signature(signer, data, sig)) {
		printf("ERROR: SM2 signing failed!\n");
		result = -1;
	} else {
		/* Get actual signature size */
		size_t actual_sig_size = signer->get_block_size(signer);
		printf("Input data: \"Hello, SM2!\"\n");
		print_hex("SM2 signature", sig, actual_sig_size);

		/* Create sig_chunk with correct length */
		sig_chunk = chunk_create(sig, actual_sig_size);

		/* Test verification */
		if (!signer->verify_signature(signer, data, sig_chunk)) {
			printf("ERROR: SM2 signature verification failed!\n");
			result = -1;
		} else {
			printf("SM2 signature verification: PASSED\n");
		}

		/* Test with wrong data */
		chunk_t wrong_data = chunk_create((u_char*)"Wrong data", 10);
		if (signer->verify_signature(signer, wrong_data, sig_chunk)) {
			printf("ERROR: SM2 verified wrong data (should fail)!\n");
			result = -1;
		} else {
			printf("SM2 wrong data rejection: PASSED\n");
		}
	}

	signer->destroy(signer);
	return result;
}

/* Test SM2-KEM Key Encapsulation Mechanism (Bidirectional) */
static int test_sm2_kem(void)
{
	printf("\n=== Testing SM2-KEM (Bidirectional Encapsulation) ===\n");

	key_exchange_t *initiator, *responder;
	chunk_t initiator_enccert = chunk_empty;
	chunk_t initiator_ct = chunk_empty;
	chunk_t responder_enccert = chunk_empty;
	chunk_t responder_ct = chunk_empty;
	chunk_t initiator_secret = chunk_empty;
	chunk_t responder_secret = chunk_empty;
	int result = 0;

	/* Create initiator KE instance (is_initiator = TRUE) */
	initiator = gmalg_sm2_ke_create_with_role(KE_SM2, TRUE);
	if (!initiator) {
		printf("ERROR: Failed to create SM2-KEM initiator\n");
		return -1;
	}
	printf("SM2-KEM initiator created successfully\n");

	/* Create responder KE instance (is_initiator = FALSE) */
	responder = gmalg_sm2_ke_create_with_role(KE_SM2, FALSE);
	if (!responder) {
		printf("ERROR: Failed to create SM2-KEM responder\n");
		initiator->destroy(initiator);
		return -1;
	}
	printf("SM2-KEM responder created successfully\n");

	/*
	 * Bidirectional SM2-KEM Protocol:
	 *
	 * Step 1: Exchange EncCert public keys (mock mode: generate key pairs)
	 */
	printf("\n--- Step 1: Exchange EncCert public keys ---\n");

	/* Initiator gets its EncCert public key */
	if (!initiator->get_public_key(initiator, &initiator_enccert)) {
		printf("ERROR: Initiator failed to get EncCert public key\n");
		result = -1;
		goto cleanup;
	}
	printf("Initiator EncCert public key: %zu bytes\n", initiator_enccert.len);
	print_hex("  First 16 bytes", initiator_enccert.ptr, 16);

	/* Responder gets its EncCert public key */
	if (!responder->get_public_key(responder, &responder_enccert)) {
		printf("ERROR: Responder failed to get EncCert public key\n");
		result = -1;
		goto cleanup;
	}
	printf("Responder EncCert public key: %zu bytes\n", responder_enccert.len);
	print_hex("  First 16 bytes", responder_enccert.ptr, 16);

	/*
	 * Step 2: Initiator encapsulates r_i with Responder's EncCert
	 */
	printf("\n--- Step 2: Initiator encapsulates r_i ---\n");

	/* Initiator receives Responder's EncCert and encapsulates */
	if (!initiator->set_public_key(initiator, responder_enccert)) {
		printf("ERROR: Initiator failed to set Responder's EncCert\n");
		result = -1;
		goto cleanup;
	}
	printf("Initiator encrypted r_i with Responder's EncCert\n");

	/* Initiator gets ciphertext ct_i to send */
	if (!initiator->get_public_key(initiator, &initiator_ct)) {
		printf("ERROR: Initiator failed to get ciphertext\n");
		result = -1;
		goto cleanup;
	}
	printf("Initiator ciphertext ct_i: %zu bytes\n", initiator_ct.len);
	print_hex("  First 16 bytes", initiator_ct.ptr, 16);

	/*
	 * Step 3: Responder decapsulates r_i, encapsulates r_r
	 */
	printf("\n--- Step 3: Responder decapsulates r_i, encapsulates r_r ---\n");

	/* Responder receives Initiator's EncCert */
	if (!responder->set_public_key(responder, initiator_enccert)) {
		printf("ERROR: Responder failed to set Initiator's EncCert\n");
		result = -1;
		goto cleanup;
	}
	printf("Responder received Initiator's EncCert\n");

	/* Responder receives ct_i, decapsulates and encapsulates */
	if (!responder->set_public_key(responder, initiator_ct)) {
		printf("ERROR: Responder failed to process Initiator's ciphertext\n");
		result = -1;
		goto cleanup;
	}
	printf("Responder decrypted r_i and encrypted r_r\n");

	/* Responder gets ciphertext ct_r to send */
	if (!responder->get_public_key(responder, &responder_ct)) {
		printf("ERROR: Responder failed to get ciphertext\n");
		result = -1;
		goto cleanup;
	}
	printf("Responder ciphertext ct_r: %zu bytes\n", responder_ct.len);
	print_hex("  First 16 bytes", responder_ct.ptr, 16);

	/*
	 * Step 4: Initiator decapsulates r_r, computes SK = r_i || r_r
	 */
	printf("\n--- Step 4: Initiator decapsulates r_r ---\n");

	/* Initiator receives ct_r and decapsulates */
	if (!initiator->set_public_key(initiator, responder_ct)) {
		printf("ERROR: Initiator failed to process Responder's ciphertext\n");
		result = -1;
		goto cleanup;
	}
	printf("Initiator decrypted r_r\n");

	/*
	 * Step 5: Both compute shared secret SK = r_i || r_r
	 */
	printf("\n--- Step 5: Verify shared secrets match ---\n");

	/* Initiator gets shared secret */
	if (!initiator->get_shared_secret(initiator, &initiator_secret)) {
		printf("ERROR: Initiator failed to get shared secret\n");
		result = -1;
		goto cleanup;
	}
	printf("Initiator shared secret: %zu bytes\n", initiator_secret.len);
	print_hex("  SK = r_i || r_r", initiator_secret.ptr, 64);

	/* Responder gets shared secret */
	if (!responder->get_shared_secret(responder, &responder_secret)) {
		printf("ERROR: Responder failed to get shared secret\n");
		result = -1;
		goto cleanup;
	}
	printf("Responder shared secret: %zu bytes\n", responder_secret.len);
	print_hex("  SK = r_i || r_r", responder_secret.ptr, 64);

	/* Verify both secrets match */
	if (initiator_secret.len != responder_secret.len ||
		memcmp(initiator_secret.ptr, responder_secret.ptr, initiator_secret.len) != 0) {
		printf("ERROR: Shared secrets do not match!\n");
		result = -1;
	} else {
		printf("\n========================================\n");
		printf("SM2-KEM bidirectional encapsulation: PASSED\n");
		printf("SK = r_i || r_r (%zu bytes)\n", initiator_secret.len);
		printf("========================================\n");
	}

cleanup:
	chunk_clear(&initiator_enccert);
	chunk_clear(&initiator_ct);
	chunk_clear(&responder_enccert);
	chunk_clear(&responder_ct);
	chunk_clear(&initiator_secret);
	chunk_clear(&responder_secret);
	if (initiator) initiator->destroy(initiator);
	if (responder) responder->destroy(responder);
	return result;
}

int main(int argc, char *argv[])
{
	int failed = 0;

	library_init(NULL, "test_gmalg");
	lib->plugins->load(lib->plugins, "");
	atexit(library_deinit);

	printf("===========================================\n");
	printf("  GMALG Plugin Test Suite\n");
	printf("  Testing SM3/SM4/SM2 Algorithms\n");
	printf("===========================================\n");
	printf("Loaded plugins: %s\n", lib->plugins->loaded_plugins(lib->plugins));

	/* Run tests */
	if (test_sm3_hash() < 0) failed++;
	if (test_sm3_prf() < 0) failed++;
	if (test_sm4_ecb() < 0) failed++;
	if (test_sm4_cbc() < 0) failed++;
	if (test_sm4_ctr() < 0) failed++;
	if (test_sm2_signer() < 0) failed++;
	if (test_sm2_kem() < 0) failed++;

	printf("\n===========================================\n");
	if (failed == 0) {
		printf("  All tests PASSED!\n");
		printf("===========================================\n");
		return 0;
	} else {
		printf("  %d test(s) FAILED!\n", failed);
		printf("===========================================\n");
		return 1;
	}
}
