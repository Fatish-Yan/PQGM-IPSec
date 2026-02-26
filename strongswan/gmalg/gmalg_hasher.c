/*
 * Copyright (C) 2025 PQGM-IKEv2 Project
 *
 * gmalg_hasher.c - SM3 Hash Algorithm Implementation
 */

#include <string.h>

#include <library.h>
#include <crypto/hashers/hasher.h>
#include <crypto/prfs/prf.h>
#include <gmssl/sm3.h>

#include "gmalg_hasher.h"
#include "gmalg_plugin.h"

#define SM3_DIGEST_SIZE 32  /* SM3 produces 256-bit (32 byte) hash */

typedef struct private_gmalg_sm3_hasher_t private_gmalg_sm3_hasher_t;

/**
 * private data of SM3 hasher
 */
struct private_gmalg_sm3_hasher_t {

	/**
	 * public hasher interface
	 */
	gmalg_sm3_hasher_t public;

	/**
	 * SM3 context
	 */
	SM3_CTX ctx;
};

/**
 * Reset the SM3 context
 */
static void sm3_reset(private_gmalg_sm3_hasher_t *this)
{
	sm3_init(&this->ctx);
}

/**
 * Update SM3 with data
 */
static void sm3_update_ctx(private_gmalg_sm3_hasher_t *this, const uint8_t *data, size_t len)
{
	sm3_update(&this->ctx, data, len);
}

/**
 * Finalize SM3 and get digest
 */
static void sm3_final(private_gmalg_sm3_hasher_t *this, uint8_t *digest)
{
	sm3_finish(&this->ctx, digest);
}

METHOD(hasher_t, reset, bool,
	private_gmalg_sm3_hasher_t *this)
{
	sm3_reset(this);
	return TRUE;
}

METHOD(hasher_t, get_hash, bool,
	private_gmalg_sm3_hasher_t *this, chunk_t data, uint8_t *hash)
{
	sm3_update_ctx(this, data.ptr, data.len);
	if (hash != NULL)
	{
		sm3_final(this, hash);
		reset(this);
	}
	return TRUE;
}

METHOD(hasher_t, allocate_hash, bool,
	private_gmalg_sm3_hasher_t *this, chunk_t data, chunk_t *hash)
{
	sm3_update_ctx(this, data.ptr, data.len);
	if (hash != NULL)
	{
		hash->ptr = malloc(SM3_DIGEST_SIZE);
		hash->len = SM3_DIGEST_SIZE;

		sm3_final(this, hash->ptr);
		reset(this);
	}
	return TRUE;
}

METHOD(hasher_t, get_hash_size, size_t,
	private_gmalg_sm3_hasher_t *this)
{
	return SM3_DIGEST_SIZE;
}

METHOD(hasher_t, hasher_destroy, void,
	private_gmalg_sm3_hasher_t *this)
{
	free(this);
}

/*
 * see header file
 */
gmalg_sm3_hasher_t* gmalg_sm3_hasher_create(hash_algorithm_t algo)
{
	private_gmalg_sm3_hasher_t *this;

	/* Only support SM3 */
	if (algo != HASH_SM3)
	{
		return NULL;
	}

	INIT(this,
		.public = {
			.hasher_interface = {
				.get_hash = _get_hash,
				.allocate_hash = _allocate_hash,
				.get_hash_size = _get_hash_size,
				.reset = _reset,
				.destroy = _hasher_destroy,
			},
		},
	);

	/* initialize SM3 context */
	sm3_reset(this);

	return &(this->public);
}

/*
 * SM3 PRF Implementation
 */

typedef struct private_gmalg_sm3_prf_t private_gmalg_sm3_prf_t;

/**
 * Private data of SM3 PRF
 */
struct private_gmalg_sm3_prf_t {

	/**
	 * public prf interface
	 */
	gmalg_sm3_prf_t public;

	/**
	 * Key
	 */
	chunk_t key;
};

METHOD(prf_t, get_bytes, bool,
	private_gmalg_sm3_prf_t *this, chunk_t seed, uint8_t *bytes)
{
	SM3_CTX ctx;
	uint8_t digest[SM3_DIGEST_SIZE];
	size_t i;

	/* Simple PRF using SM3: SM3(key || seed) repeated */
	for (i = 0; i < seed.len; i += SM3_DIGEST_SIZE)
	{
		size_t len = (seed.len - i) < SM3_DIGEST_SIZE ? (seed.len - i) : SM3_DIGEST_SIZE;

		sm3_init(&ctx);
		sm3_update(&ctx, this->key.ptr, this->key.len);
		sm3_update(&ctx, seed.ptr + i, len);
		sm3_finish(&ctx, digest);

		memcpy(bytes + i, digest, len);
	}

	return TRUE;
}

METHOD(prf_t, allocate_bytes, bool,
	private_gmalg_sm3_prf_t *this, chunk_t seed, chunk_t *bytes)
{
	if (!bytes)
	{
		return FALSE;
	}

	bytes->ptr = malloc(seed.len);
	bytes->len = seed.len;

	return get_bytes(this, seed, bytes->ptr);
}

METHOD(prf_t, get_block_size, size_t,
	private_gmalg_sm3_prf_t *this)
{
	return SM3_DIGEST_SIZE;
}

METHOD(prf_t, get_key_size, size_t,
	private_gmalg_sm3_prf_t *this)
{
	return this->key.len;
}

METHOD(prf_t, set_key, bool,
	private_gmalg_sm3_prf_t *this, chunk_t key)
{
	/* Free old key if exists */
	chunk_free(&this->key);
	/* Set new key */
	this->key = chunk_clone(key);
	return TRUE;
}

METHOD(prf_t, prf_destroy, void,
	private_gmalg_sm3_prf_t *this)
{
	chunk_free(&this->key);
	free(this);
}

/*
 * see header file
 */
prf_t* gmalg_sm3_prf_create(chunk_t key)
{
	private_gmalg_sm3_prf_t *this;

	INIT(this,
		.public = {
			.prf_interface = {
				.get_bytes = _get_bytes,
				.allocate_bytes = _allocate_bytes,
				.get_block_size = _get_block_size,
				.get_key_size = _get_key_size,
				.set_key = _set_key,
				.destroy = _prf_destroy,
			},
			.key = chunk_clone(key),
		});

	return &this->public.prf_interface;
}
