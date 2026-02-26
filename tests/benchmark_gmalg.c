/*
 * Performance benchmark for gmalg SM3/SM4 algorithms
 * For thesis experimental data collection
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <library.h>
#include "gmalg_plugin.h"
#include "gmalg_hasher.h"
#include "gmalg_crypter.h"

#define TEST_ROUNDS 10000
#define DATA_SIZE (64 * 1024)  /* 64 KB */

/* Get current time in nanoseconds */
static double get_time_ns(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec * 1e9 + ts.tv_nsec;
}

/* Benchmark SM3 Hash */
static void bench_sm3_hash(void)
{
	gmalg_sm3_hasher_t *hasher;
	uint8_t *data;
	uint8_t hash[32];
	double start, end, elapsed;

	printf("\n=== SM3 Hash Performance Benchmark ===\n");

	hasher = gmalg_sm3_hasher_create(HASH_SM3);
	if (!hasher) {
		printf("ERROR: Failed to create SM3 hasher\n");
		return;
	}

	data = malloc(DATA_SIZE);
	memset(data, 0xAB, DATA_SIZE);

	/* Warm up */
	for (int i = 0; i < 100; i++) {
		chunk_t d = chunk_create(data, DATA_SIZE);
		((hasher_t*)hasher)->get_hash((hasher_t*)hasher, d, hash);
	}

	/* Benchmark */
	start = get_time_ns();
	for (int i = 0; i < TEST_ROUNDS; i++) {
		chunk_t d = chunk_create(data, DATA_SIZE);
		((hasher_t*)hasher)->get_hash((hasher_t*)hasher, d, hash);
	}
	end = get_time_ns();

	elapsed = (end - start) / 1e9;  /* Convert to seconds */

	printf("Test rounds: %d\n", TEST_ROUNDS);
	printf("Data size: %d bytes (%.2f KB)\n", DATA_SIZE, DATA_SIZE / 1024.0);
	printf("Total time: %.4f seconds\n", elapsed);
	printf("Time per hash: %.6f ms\n", (elapsed / TEST_ROUNDS) * 1000);
	printf("Throughput: %.2f MB/s\n", (DATA_SIZE * TEST_ROUNDS) / (elapsed * 1024 * 1024));
	printf("Hash operations/sec: %.0f\n", TEST_ROUNDS / elapsed);

	free(data);
	((hasher_t*)hasher)->destroy((hasher_t*)hasher);
}

/* Benchmark SM3 PRF */
static void bench_sm3_prf(void)
{
	prf_t *prf;
	uint8_t key[32] = {0};
	uint8_t seed[64] = {0};
	uint8_t output[32];
	double start, end, elapsed;

	printf("\n=== SM3 PRF Performance Benchmark ===\n");

	memset(key, 0xAB, 32);
	memset(seed, 0xCC, 64);

	chunk_t key_chunk = chunk_create(key, 32);
	prf = gmalg_sm3_prf_create(key_chunk);
	if (!prf) {
		printf("ERROR: Failed to create SM3 PRF\n");
		return;
	}

	/* Warm up */
	for (int i = 0; i < 100; i++) {
		chunk_t s = chunk_create(seed, 64);
		prf->get_bytes(prf, s, output);
	}

	/* Benchmark */
	start = get_time_ns();
	for (int i = 0; i < TEST_ROUNDS; i++) {
		chunk_t s = chunk_create(seed, 64);
		prf->get_bytes(prf, s, output);
	}
	end = get_time_ns();

	elapsed = (end - start) / 1e9;

	printf("Test rounds: %d\n", TEST_ROUNDS);
	printf("Key size: 32 bytes\n");
	printf("Seed size: 64 bytes\n");
	printf("Output size: 32 bytes\n");
	printf("Total time: %.4f seconds\n", elapsed);
	printf("Time per PRF: %.6f ms\n", (elapsed / TEST_ROUNDS) * 1000);
	printf("PRF operations/sec: %.0f\n", TEST_ROUNDS / elapsed);

	prf->destroy(prf);
}

/* Benchmark SM4 ECB */
static void bench_sm4_ecb(void)
{
	gmalg_sm4_crypter_t *crypter;
	uint8_t key[16] = {0};
	uint8_t *data;
	uint8_t iv[16] = {0};
	double start, end, elapsed;

	printf("\n=== SM4 ECB Performance Benchmark ===\n");

	crypter = gmalg_sm4_crypter_create(ENCR_SM4_ECB, 16);
	if (!crypter) {
		printf("ERROR: Failed to create SM4 ECB crypter\n");
		return;
	}

	data = malloc(DATA_SIZE);
	memset(data, 0xDD, DATA_SIZE);
	memset(key, 0xAA, 16);
	memset(iv, 0xFF, 16);

	chunk_t key_chunk = chunk_create(key, 16);
	chunk_t data_chunk = chunk_create(data, DATA_SIZE);
	chunk_t iv_chunk = chunk_create(iv, 16);

	((crypter_t*)crypter)->set_key((crypter_t*)crypter, key_chunk);

	/* Warm up */
	for (int i = 0; i < 100; i++) {
		((crypter_t*)crypter)->encrypt((crypter_t*)crypter, data_chunk, iv_chunk, &data_chunk);
		((crypter_t*)crypter)->decrypt((crypter_t*)crypter, data_chunk, iv_chunk, &data_chunk);
	}

	/* Benchmark Encrypt */
	start = get_time_ns();
	for (int i = 0; i < TEST_ROUNDS; i++) {
		((crypter_t*)crypter)->encrypt((crypter_t*)crypter, data_chunk, iv_chunk, &data_chunk);
	}
	end = get_time_ns();
	elapsed = (end - start) / 1e9;

	printf("Encryption:\n");
	printf("  Total time: %.4f seconds\n", elapsed);
	printf("  Time per op: %.6f ms\n", (elapsed / TEST_ROUNDS) * 1000);
	printf("  Throughput: %.2f MB/s\n", (DATA_SIZE * TEST_ROUNDS) / (elapsed * 1024 * 1024));

	/* Benchmark Decrypt */
	start = get_time_ns();
	for (int i = 0; i < TEST_ROUNDS; i++) {
		((crypter_t*)crypter)->decrypt((crypter_t*)crypter, data_chunk, iv_chunk, &data_chunk);
	}
	end = get_time_ns();
	elapsed = (end - start) / 1e9;

	printf("Decryption:\n");
	printf("  Total time: %.4f seconds\n", elapsed);
	printf("  Time per op: %.6f ms\n", (elapsed / TEST_ROUNDS) * 1000);
	printf("  Throughput: %.2f MB/s\n", (DATA_SIZE * TEST_ROUNDS) / (elapsed * 1024 * 1024));

	free(data);
	((crypter_t*)crypter)->destroy((crypter_t*)crypter);
}

/* Benchmark SM4 CBC */
static void bench_sm4_cbc(void)
{
	gmalg_sm4_crypter_t *crypter;
	uint8_t key[16] = {0};
	uint8_t *data;
	uint8_t iv[16] = {0};
	double start, end, elapsed;

	printf("\n=== SM4 CBC Performance Benchmark ===\n");

	crypter = gmalg_sm4_cbc_crypter_create(ENCR_SM4_CBC, 16);
	if (!crypter) {
		printf("ERROR: Failed to create SM4 CBC crypter\n");
		return;
	}

	data = malloc(DATA_SIZE);
	memset(data, 0xDD, DATA_SIZE);
	memset(key, 0xAA, 16);
	memset(iv, 0xFF, 16);

	chunk_t key_chunk = chunk_create(key, 16);
	chunk_t data_chunk = chunk_create(data, DATA_SIZE);
	chunk_t iv_chunk = chunk_create(iv, 16);

	((crypter_t*)crypter)->set_key((crypter_t*)crypter, key_chunk);

	/* Warm up */
	for (int i = 0; i < 100; i++) {
		((crypter_t*)crypter)->encrypt((crypter_t*)crypter, data_chunk, iv_chunk, &data_chunk);
		((crypter_t*)crypter)->decrypt((crypter_t*)crypter, data_chunk, iv_chunk, &data_chunk);
	}

	/* Benchmark Encrypt */
	start = get_time_ns();
	for (int i = 0; i < TEST_ROUNDS; i++) {
		((crypter_t*)crypter)->encrypt((crypter_t*)crypter, data_chunk, iv_chunk, &data_chunk);
	}
	end = get_time_ns();
	elapsed = (end - start) / 1e9;

	printf("Encryption:\n");
	printf("  Total time: %.4f seconds\n", elapsed);
	printf("  Time per op: %.6f ms\n", (elapsed / TEST_ROUNDS) * 1000);
	printf("  Throughput: %.2f MB/s\n", (DATA_SIZE * TEST_ROUNDS) / (elapsed * 1024 * 1024));

	/* Benchmark Decrypt */
	start = get_time_ns();
	for (int i = 0; i < TEST_ROUNDS; i++) {
		((crypter_t*)crypter)->decrypt((crypter_t*)crypter, data_chunk, iv_chunk, &data_chunk);
	}
	end = get_time_ns();
	elapsed = (end - start) / 1e9;

	printf("Decryption:\n");
	printf("  Total time: %.4f seconds\n", elapsed);
	printf("  Time per op: %.6f ms\n", (elapsed / TEST_ROUNDS) * 1000);
	printf("  Throughput: %.2f MB/s\n", (DATA_SIZE * TEST_ROUNDS) / (elapsed * 1024 * 1024));

	free(data);
	((crypter_t*)crypter)->destroy((crypter_t*)crypter);
}

int main(int argc, char *argv[])
{
	library_init(NULL, "benchmark_gmalg");
	lib->plugins->load(lib->plugins, "");
	atexit(library_deinit);

	printf("===========================================\n");
	printf("  GMALG Performance Benchmark\n");
	printf("  For Thesis Experimental Data\n");
	printf("===========================================\n");
	printf("Loaded plugins: %s\n", lib->plugins->loaded_plugins(lib->plugins));
	printf("Test rounds: %d\n", TEST_ROUNDS);
	printf("Data size: %d bytes (%.2f KB)\n", DATA_SIZE, DATA_SIZE / 1024.0);

	bench_sm3_hash();
	bench_sm3_prf();
	bench_sm4_ecb();
	bench_sm4_cbc();

	printf("\n===========================================\n");
	printf("  Benchmark Complete!\n");
	printf("===========================================\n");

	return 0;
}
