/*
 * Copyright (C) 2025 PQGM-IKEv2 Project
 *
 * gmalg_signer.h - SM2 Signature Algorithm Header
 */

#ifndef GMALG_SIGNER_H_
#define GMALG_SIGNER_H_

#include "signers/signer.h"

/**
 * SM2 Signer
 */
typedef struct gmalg_sm2_signer_t gmalg_sm2_signer_t;

struct gmalg_sm2_signer_t {

	/**
	 * Public signer interface
	 */
	signer_t signer;
};

/**
 * Create an SM2 signer instance
 *
 * Returns signer instance
 */
gmalg_sm2_signer_t* gmalg_sm2_signer_create(void);

#endif /* GMALG_SIGNER_H_ */
