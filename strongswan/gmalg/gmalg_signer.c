/*
 * Copyright (C) 2025 PQGM-IKEv2 Project
 *
 * gmalg_signer.c - SM2 Signature Algorithm Implementation
 */

#include "gmalg_signer.h"

#include <gmssl/sm2.h>
#include "signers/signer.h"

#define SM2_KEY_SIZE 32  /* 256-bit private key */
#define SM2_SIG_SIZE 64  /* r + s, each 32 bytes */

typedef struct private_gmalg_sm2_signer_t private_gmalg_sm2_signer_t;

/**
 * private data of SM2 signer
 */
struct private_gmalg_sm2_signer_t {

	/**
	 * public signer interface
	 */
	gmalg_sm2_signer_t public;

	/**
	 * SM2 key
	 */
	SM2_KEY sm2_key;

	/**
	 * Private key flag
	 */
	bool has_private_key;
};

/**
 * sign function
 */
static bool sign(private_gmalg_sm2_signer_t *this, chunk_t data, chunk_t *signature)
{
	if (!this->has_private_key)
	{
		return FALSE;
	}

	/* Allocate signature buffer (64 bytes for r + s) */
	*signature = chunk_alloc(SM2_SIG_SIZE);

	/* SM2 signature with SM3 hash */
	if (sm2_sign(&this->sm2_key, SM2_DEFAULT_ID, SM2_DEFAULT_ID_LEN,
				 data.ptr, data.len, signature->ptr) != 1)
	{
		chunk_free(signature);
		return FALSE;
	}

	return TRUE;
}

/**
 * verify function
 */
static bool verify(private_gmalg_sm2_signer_t *this, chunk_t data, chunk_t signature)
{
	/* SM2 signature verification with SM3 hash */
	return sm2_verify(&this->sm2_key, SM2_DEFAULT_ID, SM2_DEFAULT_ID_LEN,
					   data.ptr, data.len, signature.ptr) == 1;
}

/**
 * set_key function
 */
static void set_key(private_gmalg_sm2_signer_t *this, chunk_t key)
{
	/* For SM2, key format depends on whether it's private or public key */
	/* This is a simplified implementation */
	if (key.len == SM2_KEY_SIZE || key.len > SM2_KEY_SIZE * 2)
	{
		/* Try to load as private key */
		if (sm2_private_key_info_from_der(&this->sm2_key, key.ptr, key.len) == 1)
		{
			this->has_private_key = TRUE;
			return;
		}
		/* Try to load as public key */
		if (sm2_public_key_info_from_der(&this->sm2_key, key.ptr, key.len) == 1)
		{
			this->has_private_key = FALSE;
			return;
		}
	}
}

/**
 * destroy function
 */
static void destroy(private_gmalg_sm2_signer_t *this)
{
	free(this);
}

/*
 * see header file
 */
gmalg_sm2_signer_t* gmalg_sm2_signer_create(void)
{
	private_gmalg_sm2_signer_t *this;

	INIT(this,
		.public = {
			.signer = {
				.set_key = (void(*)(signer_t*, chunk_t))set_key,
				.sign = (bool(*)(signer_t*, chunk_t, chunk_t*))sign,
				.verify = (bool(*)(signer_t*, chunk_t, chunk_t))verify,
				.destroy = (void(*)(signer_t*))destroy,
			},
			.has_private_key = FALSE,
		});

	/* Initialize SM2 key context */
	sm2_key_init(&this->sm2_key);

	return &this->public;
}
