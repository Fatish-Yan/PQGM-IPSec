/*
 * Copyright (C) 2025 PQGM-IKEv2 Project
 *
 * gmalg_crypter.c - SM4 Block Cipher Implementation
 */

#include <string.h>
#include <gmssl/sm4.h>

#include <library.h>
#include <crypto/crypters/crypter.h>
#include <gmssl/sm4.h>

#include "gmalg_crypter.h"
#include "gmalg_plugin.h"


typedef struct private_gmalg_sm4_crypter_t private_gmalg_sm4_crypter_t;

/**
 * SM4 modes
 */
typedef enum {
	SM4_MODE_ECB = 0,
	SM4_MODE_CBC = 1,
	SM4_MODE_CTR = 2,
} sm4_mode_t;

/**
 * private data of SM4 crypter
 */
struct private_gmalg_sm4_crypter_t {

	/**
	 * public crypter interface
	 */
	gmalg_sm4_crypter_t public;

	/**
	 * SM4 encryption key
	 */
	SM4_KEY enc_key;

	/**
	 * SM4 decryption key
	 */
	SM4_KEY dec_key;

	/**
	 * SM4 mode
	 */
	sm4_mode_t mode;

	/**
	 * Key size
	 */
	size_t key_size;
};

METHOD(crypter_t, encrypt, bool,
	private_gmalg_sm4_crypter_t *this, chunk_t data, chunk_t iv, chunk_t *encrypted)
{
	uint8_t *out, *in;
	size_t nblocks;

	in = data.ptr;
	out = data.ptr;
	if (encrypted)
	{
		*encrypted = chunk_alloc(data.len);
		out = encrypted->ptr;
	}

	nblocks = data.len / SM4_BLOCK_SIZE;

	switch (this->mode)
	{
		case SM4_MODE_ECB:
			sm4_encrypt_blocks(&this->enc_key, in, nblocks, out);
			break;

		case SM4_MODE_CBC:
			if (iv.len < SM4_BLOCK_SIZE)
			{
				return FALSE;
			}
			{
				uint8_t iv_copy[SM4_BLOCK_SIZE];
				memcpy(iv_copy, iv.ptr, SM4_BLOCK_SIZE);
				sm4_cbc_encrypt_blocks(&this->enc_key, iv_copy, in, nblocks, out);
			}
			break;

		case SM4_MODE_CTR:
			if (iv.len < SM4_BLOCK_SIZE)
			{
				return FALSE;
			}
			{
				uint8_t ctr[SM4_BLOCK_SIZE];
				memcpy(ctr, iv.ptr, SM4_BLOCK_SIZE);
				sm4_ctr_encrypt_blocks(&this->enc_key, ctr, in, nblocks, out);
			}
			break;
	}

	return TRUE;
}

METHOD(crypter_t, decrypt, bool,
	private_gmalg_sm4_crypter_t *this, chunk_t data, chunk_t iv, chunk_t *decrypted)
{
	uint8_t *out, *in;
	size_t nblocks;

	in = data.ptr;
	out = data.ptr;
	if (decrypted)
	{
		*decrypted = chunk_alloc(data.len);
		out = decrypted->ptr;
	}

	nblocks = data.len / SM4_BLOCK_SIZE;

	switch (this->mode)
	{
		case SM4_MODE_ECB:
			/* For ECB, we use decrypt key */
			sm4_encrypt_blocks(&this->dec_key, in, nblocks, out);
			break;

		case SM4_MODE_CBC:
			if (iv.len < SM4_BLOCK_SIZE)
			{
				return FALSE;
			}
			{
				uint8_t iv_copy[SM4_BLOCK_SIZE];
				memcpy(iv_copy, iv.ptr, SM4_BLOCK_SIZE);
				sm4_cbc_decrypt_blocks(&this->dec_key, iv_copy, in, nblocks, out);
			}
			break;

		case SM4_MODE_CTR:
			/* CTR mode uses same operation for encrypt and decrypt */
			return encrypt(this, data, iv, decrypted);
	}

	return TRUE;
}

METHOD(crypter_t, get_block_size, size_t,
	private_gmalg_sm4_crypter_t *this)
{
	if (this->mode == SM4_MODE_CTR)
	{
		return 1;  /* CTR mode can handle any size */
	}
	return SM4_BLOCK_SIZE;
}

METHOD(crypter_t, get_iv_size, size_t,
	private_gmalg_sm4_crypter_t *this)
{
	if (this->mode == SM4_MODE_ECB)
	{
		return 0;
	}
	return SM4_BLOCK_SIZE;
}

METHOD(crypter_t, get_key_size, size_t,
	private_gmalg_sm4_crypter_t *this)
{
	return this->key_size;
}

METHOD(crypter_t, set_key, bool,
	private_gmalg_sm4_crypter_t *this, chunk_t key)
{
	if (key.len != SM4_KEY_SIZE)
	{
		return FALSE;
	}
	sm4_set_encrypt_key(&this->enc_key, key.ptr);
	sm4_set_decrypt_key(&this->dec_key, key.ptr);
	this->key_size = SM4_KEY_SIZE;
	return TRUE;
}

METHOD(crypter_t, destroy, void,
	private_gmalg_sm4_crypter_t *this)
{
	free(this);
}

/*
 * Generic SM4 crypter creator
 */
static gmalg_sm4_crypter_t* gmalg_sm4_crypter_create_generic(
	encryption_algorithm_t algo, size_t key_size, sm4_mode_t mode)
{
	private_gmalg_sm4_crypter_t *this;

	/* SM4 only supports 128-bit key */
	if (key_size != SM4_KEY_SIZE)
	{
		return NULL;
	}

	INIT(this,
		.public = {
			.crypter_interface = {
				.encrypt = _encrypt,
				.decrypt = _decrypt,
				.get_block_size = _get_block_size,
				.get_iv_size = _get_iv_size,
				.get_key_size = _get_key_size,
				.set_key = _set_key,
				.destroy = _destroy,
			},
		},
	);

	/* Initialize mode and key_size after INIT */
	this->mode = mode;
	this->key_size = key_size;

	return &this->public;
}

/*
 * see header file
 */
gmalg_sm4_crypter_t* gmalg_sm4_crypter_create(encryption_algorithm_t algo, size_t key_size)
{
	if (algo != ENCR_SM4_ECB)
	{
		return NULL;
	}
	return gmalg_sm4_crypter_create_generic(algo, key_size, SM4_MODE_ECB);
}

gmalg_sm4_crypter_t* gmalg_sm4_cbc_crypter_create(encryption_algorithm_t algo, size_t key_size)
{
	if (algo != ENCR_SM4_CBC)
	{
		return NULL;
	}
	return gmalg_sm4_crypter_create_generic(algo, key_size, SM4_MODE_CBC);
}

gmalg_sm4_crypter_t* gmalg_sm4_ctr_crypter_create(encryption_algorithm_t algo, size_t key_size)
{
	if (algo != ENCR_SM4_CTR)
	{
		return NULL;
	}
	return gmalg_sm4_crypter_create_generic(algo, key_size, SM4_MODE_CTR);
}
