/* Test PRF-SM3 with strongSwan interface */
#include <stdio.h>
#include <string.h>
#include <gmssl/sm3.h>

/* Simulate HMAC-SM3 PRF */
int hmac_sm3_prf(const uint8_t *key, size_t key_len,
                 const uint8_t *seed, size_t seed_len,
                 uint8_t *output)
{
    SM3_CTX ctx;
    uint8_t digest[SM3_DIGEST_SIZE];
    uint8_t k_ipad[64], k_opad[64];
    size_t i;

    /* Initialize ipad and opad */
    memset(k_ipad, 0x36, 64);
    memset(k_opad, 0x5c, 64);

    /* XOR key into ipad and opad */
    for (i = 0; i < key_len && i < 64; i++)
    {
        k_ipad[i] ^= key[i];
        k_opad[i] ^= key[i];
    }

    /* Inner hash: SM3(K ⊕ ipad || seed) */
    sm3_init(&ctx);
    sm3_update(&ctx, k_ipad, 64);
    if (seed_len > 0)
        sm3_update(&ctx, seed, seed_len);
    sm3_finish(&ctx, digest);

    /* Outer hash: SM3(K ⊕ opad || inner_digest) */
    sm3_init(&ctx);
    sm3_update(&ctx, k_opad, 64);
    sm3_update(&ctx, digest, SM3_DIGEST_SIZE);
    sm3_finish(&ctx, digest);

    memcpy(output, digest, SM3_DIGEST_SIZE);
    return SM3_DIGEST_SIZE;
}

int main()
{
    /* Test case: key = "key", seed = "message" */
    uint8_t key[] = "key";
    uint8_t seed[] = "message";
    uint8_t output[32];
    
    int len = hmac_sm3_prf(key, 3, seed, 7, output);
    
    printf("HMAC-SM3 output (%d bytes): ", len);
    for (int i = 0; i < len; i++) {
        printf("%02x", output[i]);
    }
    printf("\n");
    
    /* Test with empty key */
    len = hmac_sm3_prf(NULL, 0, seed, 7, output);
    printf("HMAC-SM3 with empty key (%d bytes): ", len);
    for (int i = 0; i < len; i++) {
        printf("%02x", output[i]);
    }
    printf("\n");
    
    return 0;
}
