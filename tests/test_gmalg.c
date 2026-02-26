/*
 * Test program for gmalg SM3/SM4 algorithms
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <library.h>
#include "gmalg_plugin.h"
#include "gmalg_hasher.h"
#include "gmalg_crypter.h"

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

int main(int argc, char *argv[])
{
	int failed = 0;

	library_init(NULL, "test_gmalg");
	lib->plugins->load(lib->plugins, "");
	atexit(library_deinit);

	printf("===========================================\n");
	printf("  GMALG Plugin Test Suite\n");
	printf("  Testing SM3 Hash and SM4 Encryption\n");
	printf("===========================================\n");
	printf("Loaded plugins: %s\n", lib->plugins->loaded_plugins(lib->plugins));

	/* Run tests */
	if (test_sm3_hash() < 0) failed++;
	if (test_sm3_prf() < 0) failed++;
	if (test_sm4_ecb() < 0) failed++;
	if (test_sm4_cbc() < 0) failed++;

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
