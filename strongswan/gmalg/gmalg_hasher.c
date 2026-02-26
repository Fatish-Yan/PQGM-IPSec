/*
 * Copyright (C) 2025 PQGM-IKEv2 Project
 *
 * gmalg_hasher.c - SM3 Hash Algorithm Implementation
 */

#include "gmalg_hasher.h"

#include <gmssl/sm3.h>
#include "hashers/hasher.h"

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
 * hash function
 */
static void sm3_hash(private_gmalg_sm3_hasher_t *this, const chunk_t data)
{
	sm3_update(&this->ctx, data.ptr, data.len);
}

/**
 * get_hash_size function
 */
static size_t get_hash_size(private_gmalg_sm3_hasher_t *this)
{
	return SM3_DIGEST_SIZE;
}

/**
 * allocate_hash function
 */
static uint8_t* allocate_hash(private_gmalg_sm3_hasher_t *this, chunk_t *data)
{
	*data = chunk_alloc(SM3_DIGEST_SIZE);
	sm3_finish(&this->ctx, data->ptr);
	return data->ptr;
}

/**
 * get_hash function
 */
static uint8_t* get_hash(private_gmalg_sm3_hasher_t *this, uint8_t *buf)
{
	sm3_finish(&this->ctx, buf);
	return buf;
}

/**
 * reset function
 */
static void reset(private_gmalg_sm3_hasher_t *this)
{
	sm3_init(&this->ctx);
}

/**
 * destroy function
 */
static void destroy(private_gmalg_sm3_hasher_t *this)
{
	free(this);
}

/*
 * see header file
 */
gmalg_sm3_hasher_t* gmalg_sm3_hasher_create()
{
	private_gmalg_sm3_hasher_t *this;

	INIT(this,
		.public = {
			.hasher = {
				.hash = (void(*)(hasher_t*, const chunk_t))sm3_hash,
				.get_hash_size = (size_t(*)(hasher_t*))get_hash_size,
				.allocate_hash = (uint8_t*(*)(hasher_t*, chunk_t*))allocate_hash,
				.get_hash = (uint8_t*(*)(hasher_t*, uint8_t*))get_hash,
				.reset = (void(*)(hasher_t*))reset,
				.destroy = (void(*)(hasher_t*))destroy,
			},
		},
	);

	sm3_init(&this->ctx);

	return &this->public;
}

/*
 * SM3 PRF Implementation
 */
#include "prfs/prf.h"

typedef struct private_gmalg_sm3_prf_t private_gmalg_sm3_prf_t;

struct private_gmalg_sm3_prf_t {

	/**
	 * public prf interface
	 */
	prf_t public;

	/**
	 * Key
	 */
	chunk_t key;
};

METHOD(prf_t, get_bytes, size_t,
	private_gmalg_sm3_prf_t *this, chunk_t seed, uint8_t *bytes)
{
	SM3_CTX ctx;
	uint8_t digest[SM3_DIGEST_SIZE];
	size_t key_len = this->key.len;
	size_t i;

	/* HMAC-SM3 with key as specified in RFC 2104 */
	for (i = 0; i < seed.len; i += SM3_DIGEST_SIZE)
	{
		size_t len = (seed.len - i) < SM3_DIGEST_SIZE ? (seed.len - i) : SM3_DIGEST_SIZE;

		/* Inner hash: H(K ^ ipad, text) */
		sm3_init(&ctx);
		/* TODO: implement proper HMAC-SM3 */
		/* For now, simple SM3 of key || seed */
		sm3_update(&ctx, this->key.ptr, key_len);
		sm3_update(&ctx, seed.ptr + i, len);
		sm3_finish(&ctx, digest);

		memcpy(bytes + i, digest, len);
	}

	return seed.len;
}

METHOD(prf_t, allocate_bytes, chunk_t,
	private_gmalg_sm3_prf_t *this, chunk_t seed)
{
	chunk_t bytes = chunk_alloc(seed.len);
	get_bytes(this, seed, bytes.ptr);
	return bytes;
}

METHOD(prf_t, get_key, chunk_t,
	private_gmalg_sm3_prf_t *this)
{
	return this->key;
}

METHOD(prf_t, set_key, void,
	private_gmalg_sm3_prf_t *this, chunk_t key)
{
	/* Free old key if exists */
	chunk_free(&this->key);
	/* Set new key */
	this->key = chunk_clone(key);
}

METHOD(prf_t, destroy, void,
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
			.prf = {
				.get_bytes = _get_bytes,
				.allocate_bytes = _allocate_bytes,
				.get_key = _get_key,
				.set_key = _set_key,
				.destroy = _destroy,
			},
			.key = chunk_clone(key),
		);

	return &this->public;
}
