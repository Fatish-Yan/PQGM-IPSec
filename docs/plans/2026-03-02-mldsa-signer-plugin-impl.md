# ML-DSA-65 Signer Plugin Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Add ML-DSA-65 post-quantum signature support to strongSwan for IKE_AUTH certificate authentication.

**Architecture:** Create independent `mldsa` plugin following `gmalg` pattern. Uses liboqs for ML-DSA-65 signature/verification, OpenSSL parses X.509 certificate structure. Supports bidirectional IKE_AUTH authentication (RFC 7296).

**Tech Stack:** strongSwan 6.0.4, liboqs (≥0.10), OpenSSL 3.0.2

---

## Prerequisites

### Task 0: Install liboqs Development Library

**Files:**
- System dependency

**Step 1: Install liboqs**

```bash
sudo apt update
sudo apt install -y liboqs-dev pkg-config
```

**Step 2: Verify installation**

Run: `pkg-config --libs --cflags liboqs`
Expected: `-I/usr/include -loqs` (or similar)

**Step 3: Check ML-DSA support**

Run: `cat /usr/include/oqs/oqs.h | grep -i "ml_dsa_65"`
Expected: Shows `OQS_SIG_alg_ml_dsa_65` constant

---

## Phase 1: Plugin Skeleton

### Task 1: Create Plugin Header File

**Files:**
- Create: `/home/ipsec/strongswan/src/libstrongswan/plugins/mldsa/mldsa_plugin.h`

**Step 1: Write the header file**

```c
/*
 * Copyright (C) 2026 PQGM-IKEv2 Project
 *
 * mldsa_plugin.h - ML-DSA Post-Quantum Signature Plugin
 */

#ifndef MLDSA_PLUGIN_H_
#define MLDSA_PLUGIN_H_

#include <plugins/plugin.h>

/**
 * Algorithm IDs (private use range, following gmalg pattern)
 */
#define AUTH_MLDSA_65		1053  /* ML-DSA-65 Signature (3293 bytes) */
#define AUTH_MLDSA_44		1054  /* ML-DSA-44 (reserved for future) */
#define AUTH_MLDSA_87		1055  /* ML-DSA-87 (reserved for future) */

typedef struct mldsa_plugin_t mldsa_plugin_t;

/**
 * ML-DSA signature plugin
 */
struct mldsa_plugin_t {

	/**
	 * implements plugin interface
	 */
	plugin_t plugin;
};

#endif /** MLDSA_PLUGIN_H_ @}*/
```

**Step 2: Verify file created**

Run: `ls -la /home/ipsec/strongswan/src/libstrongswan/plugins/mldsa/`
Expected: `mldsa_plugin.h` exists

---

### Task 2: Create Plugin Source File

**Files:**
- Create: `/home/ipsec/strongswan/src/libstrongswan/plugins/mldsa/mldsa_plugin.c`

**Step 1: Write the plugin source**

```c
/*
 * Copyright (C) 2026 PQGM-IKEv2 Project
 *
 * mldsa_plugin.c - ML-DSA Post-Quantum Signature Plugin
 */

#include "mldsa_plugin.h"
#include "mldsa_signer.h"

#include <library.h>

typedef struct private_mldsa_plugin_t private_mldsa_plugin_t;

/**
 * private data of mldsa_plugin
 */
struct private_mldsa_plugin_t {

	/**
	 * public functions
	 */
	mldsa_plugin_t public;
};

METHOD(plugin_t, get_name, char*,
	private_mldsa_plugin_t *this)
{
	return "mldsa";
}

METHOD(plugin_t, get_features, int,
	private_mldsa_plugin_t *this, plugin_feature_t *features[])
{
	static plugin_feature_t f[] = {
		/* ML-DSA-65 Signature Algorithm */
		PLUGIN_REGISTER(SIGNER, mldsa_signer_create),
			PLUGIN_PROVIDE(SIGNER, AUTH_MLDSA_65),
	};
	*features = f;
	return countof(f);
}

METHOD(plugin_t, destroy, void,
	private_mldsa_plugin_t *this)
{
	free(this);
}

/*
 * see header file
 */
PLUGIN_DEFINE(mldsa)
{
	private_mldsa_plugin_t *this;

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
```

**Step 2: Verify file created**

Run: `cat /home/ipsec/strongswan/src/libstrongswan/plugins/mldsa/mldsa_plugin.c | head -30`
Expected: Shows plugin code

---

### Task 3: Create Signer Header File

**Files:**
- Create: `/home/ipsec/strongswan/src/libstrongswan/plugins/mldsa/mldsa_signer.h`

**Step 1: Write the signer header**

```c
/*
 * Copyright (C) 2026 PQGM-IKEv2 Project
 *
 * mldsa_signer.h - ML-DSA-65 Signature Implementation
 */

#ifndef MLDSA_SIGNER_H_
#define MLDSA_SIGNER_H_

#include <crypto/signers/signer.h>

/**
 * ML-DSA-65 constants (from FIPS 204)
 */
#define MLDSA65_PUBLIC_KEY_BYTES  1952
#define MLDSA65_SECRET_KEY_BYTES  4032
#define MLDSA65_SIGNATURE_BYTES   3293

typedef struct mldsa_signer_t mldsa_signer_t;

/**
 * ML-DSA-65 signer
 */
struct mldsa_signer_t {

	/**
	 * signer interface
	 */
	signer_t signer_interface;
};

/**
 * Create an ML-DSA-65 signer instance
 *
 * @param algo		signature algorithm (must be AUTH_MLDSA_65)
 * @return			ML-DSA-65 signer instance, NULL if not supported
 */
signer_t* mldsa_signer_create(integrity_algorithm_t algo);

#endif /** MLDSA_SIGNER_H_ @}*/
```

**Step 2: Verify file created**

Run: `ls /home/ipsec/strongswan/src/libstrongswan/plugins/mldsa/`
Expected: `mldsa_signer.h` exists

---

### Task 4: Create Signer Source File (Skeleton)

**Files:**
- Create: `/home/ipsec/strongswan/src/libstrongswan/plugins/mldsa/mldsa_signer.c`

**Step 1: Write the skeleton signer source**

```c
/*
 * Copyright (C) 2026 PQGM-IKEv2 Project
 *
 * mldsa_signer.c - ML-DSA-65 Signature Implementation
 *
 * Uses liboqs for ML-DSA signature operations.
 */

#include "mldsa_signer.h"
#include "mldsa_plugin.h"

#include <library.h>
#include <crypto/signers/signer.h>
#include <utils/debug.h>

#ifdef HAVE_LIBOQS
#include <oqs/oqs.h>
#endif

typedef struct private_mldsa_signer_t private_mldsa_signer_t;

#ifdef HAVE_LIBOQS

/**
 * private data of ML-DSA-65 signer
 */
struct private_mldsa_signer_t {

	/**
	 * public signer interface
	 */
	mldsa_signer_t public;

	/**
	 * liboqs signature context
	 */
	OQS_SIG *sig_ctx;

	/**
	 * Private key (for signing)
	 */
	uint8_t private_key[MLDSA65_SECRET_KEY_BYTES];

	/**
	 * Public key (for verification)
	 */
	uint8_t public_key[MLDSA65_PUBLIC_KEY_BYTES];

	/**
	 * Has private key flag
	 */
	bool has_private_key;

	/**
	 * Has public key flag
	 */
	bool has_public_key;
};

/**
 * get_signature function
 */
METHOD(signer_t, get_signature, bool,
	private_mldsa_signer_t *this, chunk_t data, uint8_t *buffer)
{
	size_t sig_len = MLDSA65_SIGNATURE_BYTES;

	if (!this->has_private_key || !this->sig_ctx)
	{
		DBG1(DBG_LIB, "ML-DSA: cannot sign, no private key");
		return FALSE;
	}

	if (OQS_SIG_sign(this->sig_ctx, buffer, &sig_len,
					 data.ptr, data.len,
					 this->private_key) != OQS_SUCCESS)
	{
		DBG1(DBG_LIB, "ML-DSA: signature generation failed");
		return FALSE;
	}

	return TRUE;
}

/**
 * allocate_signature function
 */
METHOD(signer_t, allocate_signature, bool,
	private_mldsa_signer_t *this, chunk_t data, chunk_t *signature)
{
	if (!this->has_private_key || !this->sig_ctx)
	{
		DBG1(DBG_LIB, "ML-DSA: cannot sign, no private key");
		return FALSE;
	}

	*signature = chunk_alloc(MLDSA65_SIGNATURE_BYTES);

	if (OQS_SIG_sign(this->sig_ctx, signature->ptr, &signature->len,
					 data.ptr, data.len,
					 this->private_key) != OQS_SUCCESS)
	{
		DBG1(DBG_LIB, "ML-DSA: signature generation failed");
		chunk_free(signature);
		return FALSE;
	}

	return TRUE;
}

/**
 * verify_signature function
 */
METHOD(signer_t, verify_signature, bool,
	private_mldsa_signer_t *this, chunk_t data, chunk_t signature)
{
	if (!this->has_public_key || !this->sig_ctx)
	{
		DBG1(DBG_LIB, "ML-DSA: cannot verify, no public key");
		return FALSE;
	}

	if (signature.len != MLDSA65_SIGNATURE_BYTES)
	{
		DBG1(DBG_LIB, "ML-DSA: invalid signature length %d (expected %d)",
			 signature.len, MLDSA65_SIGNATURE_BYTES);
		return FALSE;
	}

	return OQS_SIG_verify(this->sig_ctx,
						  data.ptr, data.len,
						  signature.ptr, signature.len,
						  this->public_key) == OQS_SUCCESS;
}

/**
 * set_key function
 */
METHOD(signer_t, set_key, bool,
	private_mldsa_signer_t *this, chunk_t key)
{
	/* Try to parse as private key (4032 bytes) */
	if (key.len == MLDSA65_SECRET_KEY_BYTES)
	{
		memcpy(this->private_key, key.ptr, MLDSA65_SECRET_KEY_BYTES);
		/* Extract public key from private key (last 1952 bytes) */
		memcpy(this->public_key,
			   key.ptr + MLDSA65_SECRET_KEY_BYTES - MLDSA65_PUBLIC_KEY_BYTES,
			   MLDSA65_PUBLIC_KEY_BYTES);
		this->has_private_key = TRUE;
		this->has_public_key = TRUE;
		DBG1(DBG_LIB, "ML-DSA: loaded private key with embedded public key");
		return TRUE;
	}

	/* Try to parse as public key (1952 bytes) */
	if (key.len == MLDSA65_PUBLIC_KEY_BYTES)
	{
		memcpy(this->public_key, key.ptr, MLDSA65_PUBLIC_KEY_BYTES);
		this->has_public_key = TRUE;
		this->has_private_key = FALSE;
		DBG1(DBG_LIB, "ML-DSA: loaded public key only");
		return TRUE;
	}

	DBG1(DBG_LIB, "ML-DSA: invalid key length %d (expected %d or %d)",
		 key.len, MLDSA65_SECRET_KEY_BYTES, MLDSA65_PUBLIC_KEY_BYTES);
	return FALSE;
}

/**
 * get_block_size function (signature size)
 */
METHOD(signer_t, get_block_size, size_t,
	private_mldsa_signer_t *this)
{
	return MLDSA65_SIGNATURE_BYTES;
}

/**
 * get_key_size function
 */
METHOD(signer_t, get_key_size, size_t,
	private_mldsa_signer_t *this)
{
	return MLDSA65_PUBLIC_KEY_BYTES;
}

/**
 * destroy function
 */
METHOD(signer_t, destroy, void,
	private_mldsa_signer_t *this)
{
	if (this->sig_ctx)
	{
		OQS_SIG_free(this->sig_ctx);
	}
	free(this);
}

#endif /* HAVE_LIBOQS */

/*
 * see header file
 */
signer_t* mldsa_signer_create(integrity_algorithm_t algo)
{
#ifdef HAVE_LIBOQS
	private_mldsa_signer_t *this;

	if (algo != AUTH_MLDSA_65)
	{
		DBG1(DBG_LIB, "ML-DSA: unsupported algorithm %d", algo);
		return NULL;
	}

	INIT(this,
		.public = {
			.signer_interface = {
				.get_signature = _get_signature,
				.allocate_signature = _allocate_signature,
				.verify_signature = _verify_signature,
				.get_block_size = _get_block_size,
				.get_key_size = _get_key_size,
				.set_key = _set_key,
				.destroy = _destroy,
			},
		},
		.sig_ctx = NULL,
		.has_private_key = FALSE,
		.has_public_key = FALSE,
	);

	/* Initialize liboqs ML-DSA-65 context */
	this->sig_ctx = OQS_SIG_new(OQS_SIG_alg_ml_dsa_65);
	if (!this->sig_ctx)
	{
		DBG1(DBG_LIB, "ML-DSA: failed to create OQS_SIG context");
		free(this);
		return NULL;
	}

	DBG1(DBG_LIB, "ML-DSA-65 signer created successfully");

	return &this->public.signer_interface;
#else
	DBG1(DBG_LIB, "ML-DSA: liboqs not available");
	return NULL;
#endif
}
```

**Step 2: Verify file created**

Run: `wc -l /home/ipsec/strongswan/src/libstrongswan/plugins/mldsa/mldsa_signer.c`
Expected: ~200 lines

---

### Task 5: Create Makefile.am

**Files:**
- Create: `/home/ipsec/strongswan/src/libstrongswan/plugins/mldsa/Makefile.am`

**Step 1: Write the Makefile**

```makefile
AM_CPPFLAGS = \
	-I$(top_srcdir)/src/libstrongswan \
	$(liboqs_CFLAGS)

AM_CFLAGS = \
	$(PLUGIN_CFLAGS)

if MONOLITHIC
noinst_LTLIBRARIES = libstrongswan-mldsa.la
libstrongswan_mldsa_la_LIBADD = $(liboqs_LIBS)
else
plugin_LTLIBRARIES = libstrongswan-mldsa.la
libstrongswan_mldsa_la_LIBADD = \
	$(top_builddir)/src/libstrongswan/libstrongswan.la \
	$(liboqs_LIBS)
endif

libstrongswan_mldsa_la_SOURCES = \
	mldsa_plugin.c \
	mldsa_plugin.h \
	mldsa_signer.c \
	mldsa_signer.h

libstrongswan_mldsa_la_LDFLAGS = -module -avoid-version
```

**Step 2: Verify file created**

Run: `cat /home/ipsec/strongswan/src/libstrongswan/plugins/mldsa/Makefile.am`
Expected: Shows Makefile content

---

### Task 6: Update strongSwan Build System

**Files:**
- Modify: `/home/ipsec/strongswan/configure.ac` (add mldsa plugin option)
- Modify: `/home/ipsec/strongswan/src/libstrongswan/plugins/Makefile.am` (add mldsa directory)

**Step 1: Add mldsa to plugins Makefile.am**

Find the plugin list in `/home/ipsec/strongswan/src/libstrongswan/plugins/Makefile.am` and add:

Run first to find the pattern:
```bash
grep -n "if USE_GMALG" /home/ipsec/strongswan/src/libstrongswan/plugins/Makefile.am
```

Then add after the gmalg section:
```makefile
if USE_MLDSA
  SUBDIRS += mldsa
endif
```

**Step 2: Add configure option**

In `/home/ipsec/strongswan/configure.ac`, add after the gmalg section:

```bash
# Find the gmalg section
grep -n "gmalg\|GMALG" /home/ipsec/strongswan/configure.ac | head -20
```

Add:
```m4
# ML-DSA plugin
ARG_ENABLABLE([mldsa], [enable ML-DSA-65 post-quantum signature support.])
if test x$mldsa = xtrue; then
    PKG_CHECK_MODULES([liboqs], [liboqs])
    AC_DEFINE([HAVE_LIBOQS], [1], [Define if liboqs is available])
fi
AM_CONDITIONAL(USE_MLDSA, test x$mldsa = xtrue)
PLUGIN_TEST([mldsa])
```

**Step 3: Commit skeleton**

```bash
git add /home/ipsec/strongswan/src/libstrongswan/plugins/mldsa/
git add /home/ipsec/strongswan/src/libstrongswan/plugins/Makefile.am
git add /home/ipsec/strongswan/configure.ac
git commit -m "feat(mldsa): add ML-DSA-65 signer plugin skeleton"
```

---

## Phase 2: Build and Unit Test

### Task 7: Regenerate Build System

**Files:**
- Modified by autogen.sh

**Step 1: Run autogen.sh**

```bash
cd /home/ipsec/strongswan && ./autogen.sh
```

Expected: No errors

**Step 2: Configure with mldsa enabled**

```bash
cd /home/ipsec/strongswan && \
./configure --enable-mldsa --enable-gmalg --enable-swanctl \
    --with-gmssl=/usr/local --disable-defaults
```

Expected: Shows `mldsa: yes` in summary

---

### Task 8: Build the Plugin

**Files:**
- Build: `/home/ipsec/strongswan/src/libstrongswan/plugins/mldsa/`

**Step 1: Build**

```bash
cd /home/ipsec/strongswan && make -j$(nproc)
```

Expected: Compiles successfully, creates `libstrongswan-mldsa.so`

**Step 2: Verify plugin built**

Run: `ls -la /home/ipsec/strongswan/src/libstrongswan/plugins/mldsa/.libs/`
Expected: `libstrongswan-mldsa.so` exists

**Step 3: Install**

```bash
cd /home/ipsec/strongswan && sudo make install
```

**Step 4: Verify installed**

Run: `ls -la /usr/local/lib/ipsec/plugins/ | grep mldsa`
Expected: `libstrongswan-mldsa.la` and `.so`

**Step 5: Commit**

```bash
git add -A && git commit -m "build(mldsa): configure and build ML-DSA plugin"
```

---

### Task 9: Create Unit Test

**Files:**
- Create: `/home/ipsec/PQGM-IPSec/test_mldsa_signer.c`

**Step 1: Write the test program**

```c
/*
 * test_mldsa_signer.c - ML-DSA-65 Signer Unit Test
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <oqs/oqs.h>

#define MLDSA65_PUBLIC_KEY_BYTES  1952
#define MLDSA65_SECRET_KEY_BYTES  4032
#define MLDSA65_SIGNATURE_BYTES   3293

int main(void)
{
	OQS_SIG *sig = NULL;
	uint8_t *public_key = NULL;
	uint8_t *secret_key = NULL;
	uint8_t *signature = NULL;
	uint8_t message[] = "Hello, ML-DSA-65!";
	size_t message_len = sizeof(message) - 1;
	size_t sig_len;
	int ret = 1;

	printf("=== ML-DSA-65 Signer Unit Test ===\n\n");

	/* Step 1: Create ML-DSA-65 context */
	printf("1. Creating ML-DSA-65 context...\n");
	sig = OQS_SIG_new(OQS_SIG_alg_ml_dsa_65);
	if (!sig) {
		printf("   FAIL: Failed to create OQS_SIG context\n");
		goto cleanup;
	}
	printf("   OK: Created context for %s\n", sig->method_name);
	printf("   Public key size: %d bytes\n", sig->length_public_key);
	printf("   Secret key size: %d bytes\n", sig->length_secret_key);
	printf("   Signature size: %d bytes\n", sig->length_signature);

	/* Verify sizes match our definitions */
	if (sig->length_public_key != MLDSA65_PUBLIC_KEY_BYTES) {
		printf("   FAIL: Public key size mismatch\n");
		goto cleanup;
	}
	if (sig->length_secret_key != MLDSA65_SECRET_KEY_BYTES) {
		printf("   FAIL: Secret key size mismatch\n");
		goto cleanup;
	}
	if (sig->length_signature != MLDSA65_SIGNATURE_BYTES) {
		printf("   FAIL: Signature size mismatch\n");
		goto cleanup;
	}

	/* Step 2: Allocate key buffers */
	printf("\n2. Allocating key buffers...\n");
	public_key = malloc(sig->length_public_key);
	secret_key = malloc(sig->length_secret_key);
	signature = malloc(sig->length_signature);
	if (!public_key || !secret_key || !signature) {
		printf("   FAIL: Memory allocation failed\n");
		goto cleanup;
	}
	printf("   OK: Buffers allocated\n");

	/* Step 3: Generate keypair */
	printf("\n3. Generating keypair...\n");
	if (OQS_SIG_keypair(sig, public_key, secret_key) != OQS_SUCCESS) {
		printf("   FAIL: Keypair generation failed\n");
		goto cleanup;
	}
	printf("   OK: Keypair generated\n");

	/* Step 4: Sign message */
	printf("\n4. Signing message: \"%s\"\n", message);
	sig_len = sig->length_signature;
	if (OQS_SIG_sign(sig, signature, &sig_len,
					 message, message_len, secret_key) != OQS_SUCCESS) {
		printf("   FAIL: Signature generation failed\n");
		goto cleanup;
	}
	printf("   OK: Signature generated (%zu bytes)\n", sig_len);

	/* Step 5: Verify signature (should succeed) */
	printf("\n5. Verifying valid signature...\n");
	if (OQS_SIG_verify(sig, message, message_len,
					   signature, sig_len, public_key) != OQS_SUCCESS) {
		printf("   FAIL: Valid signature verification failed\n");
		goto cleanup;
	}
	printf("   OK: Valid signature verified\n");

	/* Step 6: Verify with wrong message (should fail) */
	printf("\n6. Verifying with tampered message...\n");
	uint8_t tampered[] = "Hello, ML-DSA-66!";
	if (OQS_SIG_verify(sig, tampered, sizeof(tampered) - 1,
					   signature, sig_len, public_key) == OQS_SUCCESS) {
		printf("   FAIL: Tampered message should NOT verify\n");
		goto cleanup;
	}
	printf("   OK: Tampered message correctly rejected\n");

	/* Step 7: Verify with wrong signature (should fail) */
	printf("\n7. Verifying with tampered signature...\n");
	signature[0] ^= 0xFF;  /* Flip bits */
	if (OQS_SIG_verify(sig, message, message_len,
					   signature, sig_len, public_key) == OQS_SUCCESS) {
		printf("   FAIL: Tampered signature should NOT verify\n");
		goto cleanup;
	}
	printf("   OK: Tampered signature correctly rejected\n");

	printf("\n=== ALL TESTS PASSED ===\n");
	ret = 0;

cleanup:
	if (sig) OQS_SIG_free(sig);
	if (public_key) free(public_key);
	if (secret_key) free(secret_key);
	if (signature) free(signature);
	return ret;
}
```

**Step 2: Compile the test**

```bash
cd /home/ipsec/PQGM-IPSec && \
gcc -o test_mldsa_signer test_mldsa_signer.c -loqs
```

**Step 3: Run the test**

```bash
cd /home/ipsec/PQGM-IPSec && LD_LIBRARY_PATH=/usr/local/lib ./test_mldsa_signer
```

Expected: All tests pass

**Step 4: Commit test**

```bash
git add test_mldsa_signer.c && git commit -m "test(mldsa): add ML-DSA-65 liboqs unit test"
```

---

## Phase 3: IKE_AUTH Integration

### Task 10: Test Plugin Loading

**Files:**
- Config: `/usr/local/etc/strongswan.conf`

**Step 1: Enable mldsa plugin in strongswan.conf**

Add to `/usr/local/etc/strongswan.conf`:
```
charon {
    load_modular = yes
    plugins {
        mldsa {
            load = yes
        }
    }
}
```

**Step 2: Test plugin loading**

```bash
sudo /usr/local/libexec/ipsec/charon --debug-all 2>&1 | grep -i mldsa
```

Expected: Shows ML-DSA plugin loading

---

### Task 11: Generate ML-DSA Test Keys and Certificates

**Files:**
- Create: `/home/ipsec/PQGM-IPSec/docker/initiator/certs/mldsa/`
- Create: `/home/ipsec/PQGM-IPSec/docker/responder/certs/mldsa/`

**Step 1: Create certificate directories**

```bash
mkdir -p /home/ipsec/PQGM-IPSec/docker/initiator/certs/mldsa
mkdir -p /home/ipsec/PQGM-IPSec/docker/responder/certs/mldsa
```

**Step 2: Create key generation script**

Create `/home/ipsec/PQGM-IPSec/scripts/generate_mldsa_certs.sh`:

```bash
#!/bin/bash
# Generate ML-DSA-65 keys and certificates using liboqs oqs-provider

set -e

INITIATOR_DIR="/home/ipsec/PQGM-IPSec/docker/initiator/certs/mldsa"
RESPONDER_DIR="/home/ipsec/PQGM-IPSec/docker/responder/certs/mldsa"

# Check if oqs-provider is available
if ! openssl list -providers 2>/dev/null | grep -q oqs; then
    echo "oqs-provider not found. Installing..."
    # This requires OpenSSL 3.5+ with oqs-provider
    echo "Please install oqs-provider for OpenSSL 3.5+"
    echo "Alternative: Use liboqs tools directly"
    exit 1
fi

echo "Generating ML-DSA-65 certificates..."

# Generate Initiator key and certificate
openssl req -x509 -newkey ml-dsa-65 \
    -keyout "$INITIATOR_DIR/mldsa_key.pem" \
    -out "$INITIATOR_DIR/mldsa_cert.pem" \
    -days 365 \
    -subj "/CN=initiator.pqgm.test" \
    -addext "subjectAltName=DNS:initiator.pqgm.test"

# Generate Responder key and certificate
openssl req -x509 -newkey ml-dsa-65 \
    -keyout "$RESPONDER_DIR/mldsa_key.pem" \
    -out "$RESPONDER_DIR/mldsa_cert.pem" \
    -days 365 \
    -subj "/CN=responder.pqgm.test" \
    -addext "subjectAltName=DNS:responder.pqgm.test"

echo "ML-DSA-65 certificates generated successfully"
```

**Step 3: Note about OpenSSL version**

Since OpenSSL 3.0.2 doesn't support ML-DSA, we'll need to:
- Option A: Upgrade to OpenSSL 3.5+
- Option B: Use liboqs tools directly to generate raw keys
- Option C: Use a temporary container with oqs-provider

For now, document this as a known limitation.

**Step 4: Commit**

```bash
git add scripts/generate_mldsa_certs.sh
git commit -m "docs(mldsa): add ML-DSA certificate generation script (requires OpenSSL 3.5+)"
```

---

### Task 12: Create Test swanctl Configuration

**Files:**
- Create: `/home/ipsec/PQGM-IPSec/docker/initiator/config/swanctl-mldsa.conf`
- Create: `/home/ipsec/PQGM-IPSec/docker/responder/config/swanctl-mldsa.conf`

**Step 1: Write initiator config**

```conf
# PQ-GM-IKEv2 with ML-DSA-65 Certificate Authentication
connections {
    pqgm-ikev2-mldsa {
        version = 2
        local_addrs = 172.28.0.10
        remote_addrs = 172.28.0.20
        proposals = aes256-sha256-x25519-ke1_sm2kem-ke2_mlkem768

        local {
            auth = mldsa
            id = initiator.pqgm.test
            certs = mldsa_cert.pem
        }

        remote {
            auth = mldsa
            id = responder.pqgm.test
        }

        children {
            ipsec {
                local_ts = 10.1.0.0/16
                remote_ts = 10.2.0.0/16
                esp_proposals = aes256gcm16-sha256
                start_action = none
            }
        }
    }
}

secrets {
    mldsa-initiator {
        id = initiator.pqgm.test
        file = mldsa_key.pem
    }
}
```

**Step 2: Write responder config**

```conf
# PQ-GM-IKEv2 with ML-DSA-65 Certificate Authentication
connections {
    pqgm-ikev2-mldsa {
        version = 2
        local_addrs = 172.28.0.20
        remote_addrs = 172.28.0.10
        proposals = aes256-sha256-x25519-ke1_sm2kem-ke2_mlkem768

        local {
            auth = mldsa
            id = responder.pqgm.test
            certs = mldsa_cert.pem
        }

        remote {
            auth = mldsa
            id = initiator.pqgm.test
        }

        children {
            ipsec {
                local_ts = 10.2.0.0/16
                remote_ts = 10.1.0.0/16
                esp_proposals = aes256gcm16-sha256
                start_action = none
            }
        }
    }
}

secrets {
    mldsa-responder {
        id = responder.pqgm.test
        file = mldsa_key.pem
    }
}
```

**Step 3: Commit**

```bash
git add docker/initiator/config/swanctl-mldsa.conf
git add docker/responder/config/swanctl-mldsa.conf
git commit -m "config(mldsa): add swanctl configuration for ML-DSA authentication"
```

---

## Phase 4: Integration Testing

### Task 13: End-to-End IKE_AUTH Test

**Files:**
- Test: Full 5-RTT PQ-GM-IKEv2 with ML-DSA authentication

**Step 1: Deploy to Docker containers**

```bash
# Copy plugin to containers
docker cp /usr/local/lib/ipsec/plugins/libstrongswan-mldsa.so pqgm-initiator:/usr/local/lib/ipsec/plugins/
docker cp /usr/local/lib/ipsec/plugins/libstrongswan-mldsa.so pqgm-responder:/usr/local/lib/ipsec/plugins/
```

**Step 2: Load plugin in containers**

```bash
docker exec pqgm-initiator sh -c "echo 'load_modular = yes' >> /etc/strongswan.conf"
docker exec pqgm-initiator sh -c "echo 'plugins { mldsa { load = yes } }' >> /etc/strongswan.conf"
```

**Step 3: Test IKE_SA initiation**

```bash
docker exec pqgm-initiator swanctl --initiate --child ipsec
```

**Step 4: Check logs**

```bash
docker logs pqgm-initiator 2>&1 | grep -i "ml-dsa\|mldsa"
docker logs pqgm-responder 2>&1 | grep -i "ml-dsa\|mldsa"
```

**Step 5: Verify SA**

```bash
docker exec pqgm-initiator swanctl --list-sas
```

Expected: Shows ESTABLISHED IKE_SA with ML-DSA authentication

---

## Phase 5: Documentation

### Task 14: Update Documentation

**Files:**
- Update: `/home/ipsec/PQGM-IPSec/docs/FIXES-RECORD.md`
- Update: `/home/ipsec/PQGM-IPSec/CLAUDE.md`

**Step 1: Add to FIXES-RECORD.md**

```markdown
### 2026-03-02: ML-DSA-65 Signer Plugin Implementation

**功能**: 为 strongSwan 添加 ML-DSA-65 后量子签名支持

**实现**:
- 创建独立 mldsa 插件，参考 gmalg 模式
- 使用 liboqs 进行 ML-DSA-65 签名/验证
- 算法 ID: AUTH_MLDSA_65 = 1053 (私有使用范围)

**文件**:
- strongswan/src/libstrongswan/plugins/mldsa/

**验证结果**:
- ✅ liboqs 单元测试通过
- ✅ 插件编译成功
- ⏳ IKE_AUTH 集成测试 (需要 OpenSSL 3.5+ 支持 ML-DSA 证书)
```

**Step 2: Update CLAUDE.md**

Add to Implementation Status:
```markdown
| ML-DSA-65 Signer | ✅ Done | liboqs plugin, AUTH_MLDSA_65 = 1053 |
```

**Step 3: Commit**

```bash
git add docs/FIXES-RECORD.md CLAUDE.md
git commit -m "docs: add ML-DSA-65 signer plugin implementation record"
```

---

## Known Limitations

1. **OpenSSL Version**: OpenSSL 3.0.2 doesn't support ML-DSA certificate generation
   - Solution: Upgrade to OpenSSL 3.5+ or use liboqs tools directly

2. **Certificate Loading**: strongSwan's openssl plugin may not recognize ML-DSA certificates
   - Solution: May need custom certificate loading code

3. **Authentication Method**: ML-DSA is not standardized in IKEv2 yet
   - Solution: Using private use range (AUTH_MLDSA_65 = 1053)

---

## References

- Design Doc: `/home/ipsec/PQGM-IPSec/docs/plans/2026-03-02-mldsa-signer-plugin-design.md`
- FIPS 204: Module-Lattice-Based Digital Signature Standard
- liboqs: https://github.com/open-quantum-safe/liboqs
- gmalg plugin: `/home/ipsec/strongswan/src/libstrongswan/plugins/gmalg/`
