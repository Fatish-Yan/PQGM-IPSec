/*
 * Copyright (C) 2025 PQGM-IKEv2 Project
 *
 * gmalg_hasher.h - SM3 Hash Algorithm Header
 */

#ifndef GMALG_HASHER_H_
#define GMALG_HASHER_H_

#include <crypto/hashers/hasher.h>
#include <crypto/prfs/prf.h>

#define SM3_DIGEST_SIZE 32  /* SM3 produces 256-bit (32 byte) hash */

typedef struct gmalg_sm3_hasher_t gmalg_sm3_hasher_t;
typedef struct gmalg_sm3_prf_t gmalg_sm3_prf_t;

struct gmalg_sm3_hasher_t {

	/**
	 * Public hasher interface
	 */
	hasher_t hasher_interface;
};

struct gmalg_sm3_prf_t {

	/**
	 * Public PRF interface
	 */
	prf_t prf_interface;

	/**
	 * Key for PRF
	 */
	chunk_t key;
};

/**
 * Create an SM3 hasher instance
 *
 * @param algo		hash algorithm (must be HASH_SM3)
 * Returns hasher instance
 */
gmalg_sm3_hasher_t* gmalg_sm3_hasher_create(hash_algorithm_t algo);

/**
 * Create an SM3 PRF instance
 *
 * @param key	Key for PRF
 *
 * Returns prf instance
 */
prf_t* gmalg_sm3_prf_create(chunk_t key);

#endif /* GMALG_HASHER_H_ */
