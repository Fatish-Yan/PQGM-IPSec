/*
 * Copyright (C) 2025 PQGM-IKEv2 Project
 *
 * gmalg_hasher.h - SM3 Hash Algorithm Header
 */

#ifndef GMALG_HASHER_H_
#define GMALG_HASHER_H_

#include "hashers/hasher.h"
#include "prfs/prf.h"

#define SM3_DIGEST_SIZE 32  /* SM3 produces 256-bit (32 byte) hash */

/**
 * SM3 Hasher
 */
typedef struct gmalg_sm3_hasher_t gmalg_sm3_hasher_t;

struct gmalg_sm3_hasher_t {

	/**
	 * Public hasher interface
	 */
	hasher_t hasher;
};

/**
 * Create an SM3 hasher instance
 *
 * Returns hasher instance
 */
gmalg_sm3_hasher_t* gmalg_sm3_hasher_create(void);

/**
 * Create an SM3 PRF instance
 *
 * @param key	Key for PRF
 *
 * Returns prf instance
 */
prf_t* gmalg_sm3_prf_create(chunk_t key);

#endif /* GMALG_HASHER_H_ */
