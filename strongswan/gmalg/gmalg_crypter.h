/*
 * Copyright (C) 2025 PQGM-IKEv2 Project
 *
 * gmalg_crypter.h - SM4 Block Cipher Header
 */

#ifndef GMALG_CRYPTER_H_
#define GMALG_CRYPTER_H_

#include <crypto/crypters/crypter.h>

typedef struct gmalg_sm4_crypter_t gmalg_sm4_crypter_t;

struct gmalg_sm4_crypter_t {

	/**
	 * Public crypter interface
	 */
	crypter_t crypter_interface;
};

/**
 * Create an SM4 ECB crypter instance
 *
 * @param algo		encryption algorithm (must be ENCR_SM4_ECB)
 * @param key_size	key size in bytes (must be 16 for SM4)
 * Returns crypter instance
 */
gmalg_sm4_crypter_t* gmalg_sm4_crypter_create(encryption_algorithm_t algo, size_t key_size);

/**
 * Create an SM4 CBC crypter instance
 *
 * @param algo		encryption algorithm (must be ENCR_SM4_CBC)
 * @param key_size	key size in bytes (must be 16 for SM4)
 * Returns crypter instance
 */
gmalg_sm4_crypter_t* gmalg_sm4_cbc_crypter_create(encryption_algorithm_t algo, size_t key_size);

/**
 * Create an SM4 CTR crypter instance
 *
 * @param algo		encryption algorithm (must be ENCR_SM4_CTR)
 * @param key_size	key size in bytes (must be 16 for SM4)
 * Returns crypter instance
 */
gmalg_sm4_crypter_t* gmalg_sm4_ctr_crypter_create(encryption_algorithm_t algo, size_t key_size);

#endif /* GMALG_CRYPTER_H_ */
