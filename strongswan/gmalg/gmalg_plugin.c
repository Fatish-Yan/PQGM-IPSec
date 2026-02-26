/*
 * Copyright (C) 2025 PQGM-IKEv2 Project
 *
 * gmalg_plugin.c - Chinese National Cryptographic Algorithm Plugin
 *
 * This plugin provides support for:
 * - SM2: Elliptic Curve Cryptography (Sign)
 * - SM3: Hash Algorithm
 * - SM4: Block Cipher
 *
 */

#include "gmalg_plugin.h"

#include <library.h>
#include "gmalg_hasher.h"
#include "gmalg_crypter.h"

#if defined(HAVE_GMSSL)
#include <gmssl/sm3.h>
#include <gmssl/sm4.h>
#include <gmssl/sm2.h>
#endif

typedef struct private_gmalg_plugin_t private_gmalg_plugin_t;

/**
 * private data of gmalg_plugin
 */
struct private_gmalg_plugin_t {

	/**
	 * public functions
	 */
	gmalg_plugin_t public;
};

METHOD(plugin_t, get_name, char*,
	private_gmalg_plugin_t *this)
{
	return "gmalg";
}

METHOD(plugin_t, get_features, int,
	private_gmalg_plugin_t *this, plugin_feature_t *features[])
{
	static plugin_feature_t f[] = {
#ifdef HAVE_GMSSL
		/* SM3 Hash Algorithm */
		PLUGIN_REGISTER(HASHER, gmalg_sm3_hasher_create),
			PLUGIN_PROVIDE(HASHER, HASH_SM3),
			PLUGIN_PROVIDE(HASHER, HASH_SHA256),  /* SM3 is same size as SHA256 */

		/* SM3 PRF */
		PLUGIN_REGISTER(PRF, gmalg_sm3_prf_create),
			PLUGIN_PROVIDE(PRF, PRF_SM3),
			PLUGIN_PROVIDE(PRF, PRF_HMAC_SHA2_256),  /* Fallback for SHA256-based PRF */

		/* SM4 Block Cipher - ECB mode */
		PLUGIN_REGISTER(CRYPTER, gmalg_sm4_crypter_create),
			PLUGIN_PROVIDE(CRYPTER, ENCR_SM4_ECB),
			PLUGIN_PROVIDE(CRYPTER, ENCR_AES_ECB),  /* Same block/key size as AES */

		/* SM4 Block Cipher - CBC mode */
		PLUGIN_REGISTER(CRYPTER, gmalg_sm4_cbc_crypter_create),
			PLUGIN_PROVIDE(CRYPTER, ENCR_SM4_CBC),
			PLUGIN_PROVIDE(CRYPTER, ENCR_AES_CBC),  /* Same block/key size as AES */
#endif
	};
	*features = f;
	return countof(f);
}

METHOD(plugin_t, destroy, void,
	private_gmalg_plugin_t *this)
{
	free(this);
}

/*
 * see header file
 */
PLUGIN_DEFINE(gmalg)
{
	private_gmalg_plugin_t *this;

	INIT(this,
		.public = {
			.plugin = {
				.get_name = _get_name,
				.get_features = _get_features,
				.destroy = _destroy,
			},
		},
	);

	return &this->public.plugin;
}
