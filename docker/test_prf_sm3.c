/* Test PRF-SM3 implementation */
#include <stdio.h>
#include <string.h>
#include <gmssl/sm3.h>

/* Simple test: HMAC-SM3 with test key and message */
int main() {
    SM3_CTX ctx;
    uint8_t digest[SM3_DIGEST_SIZE];
    uint8_t k_ipad[64], k_opad[64];
    
    /* Test key "key" */
    uint8_t key[] = "key";
    size_t key_len = 3;
    
    /* Test message "message" */
    uint8_t msg[] = "message";
    size_t msg_len = 7;
    
    /* Initialize ipad and opad */
    memset(k_ipad, 0x36, 64);
    memset(k_opad, 0x5c, 64);
    
    /* XOR key into ipad and opad */
    for (size_t i = 0; i < key_len && i < 64; i++) {
        k_ipad[i] ^= key[i];
        k_opad[i] ^= key[i];
    }
    
    /* Inner hash: SM3(K ⊕ ipad || msg) */
    sm3_init(&ctx);
    sm3_update(&ctx, k_ipad, 64);
    sm3_update(&ctx, msg, msg_len);
    sm3_finish(&ctx, digest);
    
    /* Outer hash: SM3(K ⊕ opad || inner_digest) */
    sm3_init(&ctx);
    sm3_update(&ctx, k_opad, 64);
    sm3_update(&ctx, digest, SM3_DIGEST_SIZE);
    sm3_finish(&ctx, digest);
    
    printf("HMAC-SM3(\"key\", \"message\") =\n");
    for (int i = 0; i < SM3_DIGEST_SIZE; i++) {
        printf("%02x", digest[i]);
    }
    printf("\n");
    
    /* Expected HMAC-SM3("key", "message") - verify with known answer */
    /* For reference, SM3-HMAC test vector */
    
    return 0;
}
