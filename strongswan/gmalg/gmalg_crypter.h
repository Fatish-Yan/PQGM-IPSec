/*
 * Copyright (C) 2025 PQGM-IKEv2 Project
 *
 * gmalg_crypter.h - SM4 Block Cipher Header
 */

#ifndef GMALG_CRYPTER_H_
#define GMALG_CRYPTER_H_

#include "crypters/crypter.h"

/**
 * SM4 Crypter
 */
typedef struct gmalg_sm4_crypter_t gmalg_sm4_crypter_t;

struct gmalg_sm4_crypter_t {

	/**
	 * Public crypter interface
	 */
	crypter_t crypter;
};

/**
 * Create an SM4 ECB crypter instance
 *
 * Returns crypter instance
 */
gmalg_sm4_crypter_t* gmalg_sm4_crypter_create(void);

/**
 * Create an SM4 CBC crypter instance
 *
 * Returns crypter instance
 */
gmalg_sm4_crypter_t* gmalg_sm4_cbc_crypter_create(void);

/**
 * Create an SM4 CTR crypter instance
 *
 * Returns crypter instance
 */
gmalg_sm4_crypter_t* gmalg_sm4_ctr_crypter_create(void);

#endif /* GMALG_CRYPTER_H_ */
