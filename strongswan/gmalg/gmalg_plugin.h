/*
 * Copyright (C) 2025 PQGM-IKEv2 Project
 *
 * gmalg_plugin.h - Chinese National Cryptographic Algorithm Plugin Header
 */

#ifndef GMALG_PLUGIN_H_
#define GMALG_PLUGIN_H_

#include <plugins/plugin.h>
#include <crypto/hashers/hasher.h>
#include <crypto/crypters/crypter.h>

/* GM/T 0004-2012 SM3 Hash Algorithm (256-bit) */
#define HASH_SM3		1032  /* SM3 Hash Algorithm */

/* GM/T 0002-2012 SM4 Block Cipher */
#define ENCR_SM4_ECB	1040  /* SM4 ECB mode */
#define ENCR_SM4_CBC	1041  /* SM4 CBC mode */
#define ENCR_SM4_CTR	1042  /* SM4 CTR mode */

/* GM/T 0003-2012 SM2 Signature Algorithm */
#define SIGN_SM2		1050  /* SM2 Signature */

/* GM/T 0003-2012 SM2 Key Exchange */
#define KE_SM2			1051  /* SM2 Key Exchange */

/* PRF using SM3 */
#define PRF_SM3			1052  /* PRF with SM3 */

typedef struct gmalg_plugin_t gmalg_plugin_t;

struct gmalg_plugin_t {

	/**
	 * Implements plugin interface
	 */
	plugin_t plugin;
};

/**
 * gmalg_plugin_create - Create the gmalg plugin instance
 *
 * Returns the plugin instance
 */
plugin_t *gmalg_plugin_create(void);

#endif /* GMALG_PLUGIN_H_ */
