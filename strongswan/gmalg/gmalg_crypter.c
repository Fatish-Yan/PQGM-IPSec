/*
 * Copyright (C) 2025 PQGM-IKEv2 Project
 *
 * gmalg_crypter.c - SM4 Block Cipher Implementation
 */

#include "gmalg_crypter.h"

#include <gmssl/sm4.h>
#include "crypters/crypter.h"

#define SM4_KEY_SIZE 16  /* 128-bit key */
#define SM4_BLOCK_SIZE 16  /* 128-bit block */

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
	 * SM4 mode
	 */
	sm4_mode_t mode;

	/**
	 * Encryption/Decryption context
	 */
	union {
		SM4_ECB_CTX ecb;
		SM4_CBC_CTX cbc;
		SM4_CTX ctr;
	} ctx;

	/**
	 * Initialization Vector (for CBC and CTR modes)
	 */
	uint8_t iv[SM4_BLOCK_SIZE];

	/**
	 * Key
	 */
	uint8_t key[SM4_KEY_SIZE];
};

/**
 * decrypt_block function
 */
static void decrypt_block(private_gmalg_sm4_crypter_t *this,
						uint8_t *block)
{
	switch (this->mode)
	{
		case SM4_MODE_ECB:
			sm4_ecb_decrypt_block(this->ctx.ecb, block);
			break;
		case SM4_MODE_CBC:
			sm4_cbc_decrypt(&this->ctx.cbc, this->iv, block);
			break;
		case SM4_MODE_CTR:
			/* CTR mode uses encrypt for both directions */
			sm4_ctr_encrypt(&this->ctx.ctr, this->iv, block, SM4_BLOCK_SIZE, block);
			break;
	}
}

/**
 * encrypt_block function
 */
static void encrypt_block(private_gmalg_sm4_crypter_t *this,
						uint8_t *block)
{
	switch (this->mode)
	{
		case SM4_MODE_ECB:
			sm4_ecb_encrypt_block(this->ctx.ecb, block);
			break;
		case SM4_MODE_CBC:
			sm4_cbc_encrypt(&this->ctx.cbc, this->iv, block);
			break;
		case SM4_MODE_CTR:
			sm4_ctr_encrypt(&this->ctx.ctr, this->iv, block, SM4_BLOCK_SIZE, block);
			break;
	}
}

/**
 * set_key function
 */
static void set_key(private_gmalg_sm4_crypter_t *this, const uint8_t *key, size_t key_len)
{
	if (key_len != SM4_KEY_SIZE)
	{
		return;  /* SM4 only supports 128-bit key */
	}

	memcpy(this->key, key, SM4_KEY_SIZE);

	switch (this->mode)
	{
		case SM4_MODE_ECB:
			sm4_ecb_key_init(this->ctx.ecb, this->key);
			break;
		case SM4_MODE_CBC:
			sm4_cbc_key_init(&this->ctx.cbc, this->key);
			break;
		case SM4_MODE_CTR:
			sm4_ctr_key_init(&this->ctx.ctr, this->key);
			break;
	}
}

/**
 * destroy function
 */
static void destroy(private_gmalg_sm4_crypter_t *this)
{
	free(this);
}

/*
 * Generic SM4 crypter creator
 */
static gmalg_sm4_crypter_t* gmalg_sm4_crypter_create_generic(sm4_mode_t mode)
{
	private_gmalg_sm4_crypter_t *this;

	INIT(this,
		.public = {
			.crypter = {
				.encrypt = (void(*)(crypter_t*, uint8_t*))encrypt_block,
				.decrypt = (void(*)(crypter_t*, uint8_t*))decrypt_block,
				.set_key = (void(*)(crypter_t*, const uint8_t*, size_t))set_key,
				.destroy = (void(*)(crypter_t*))destroy,
			},
			.mode = mode,
		});

	memset(this->iv, 0, SM4_BLOCK_SIZE);

	return &this->public;
}

/*
 * see header file
 */
gmalg_sm4_crypter_t* gmalg_sm4_crypter_create(void)
{
	return gmalg_sm4_crypter_create_generic(SM4_MODE_ECB);
}

gmalg_sm4_crypter_t* gmalg_sm4_cbc_crypter_create(void)
{
	return gmalg_sm4_crypter_create_generic(SM4_MODE_CBC);
}

gmalg_sm4_crypter_t* gmalg_sm4_ctr_crypter_create(void)
{
	return gmalg_sm4_crypter_create_generic(SM4_MODE_CTR);
}
