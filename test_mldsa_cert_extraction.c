/*
 * test_mldsa_cert_extraction.c - 测试从混合证书提取 ML-DSA 公钥
 *
 * 编译: gcc -o test_mldsa_cert_extraction test_mldsa_cert_extraction.c -loqs -lcrypto
 * 运行: LD_LIBRARY_PATH=/usr/local/lib ./test_mldsa_cert_extraction
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <oqs/oqs.h>
#include <openssl/bio.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/err.h>

#define MLDSA65_PUBLIC_KEY_BYTES  1952
#define MLDSA65_SECRET_KEY_BYTES  4032
#define MLDSA65_SIGNATURE_BYTES   3309
#define MLDSA65_EXT_OID "1.3.6.1.4.1.99999.1.2"

/* ML-DSA OID: 1.3.6.1.4.1.99999.1.2 = 06 0A 2B 06 01 04 01 86 8D 1F 01 02 */
static const uint8_t mldsa_oid[] = {0x06, 0x0A, 0x2B, 0x06, 0x01, 0x04, 0x01, 0x86, 0x8D, 0x1F, 0x01, 0x02};

int extract_mldsa_pubkey_from_der(const uint8_t *der_data, size_t der_len, uint8_t *pubkey)
{
    size_t i;
    int found = 0;
    size_t ext_start = 0;

    /* Search for the OID in the certificate */
    for (i = 0; i < der_len - sizeof(mldsa_oid); i++)
    {
        if (memcmp(der_data + i, mldsa_oid, sizeof(mldsa_oid)) == 0)
        {
            found = 1;
            ext_start = i + sizeof(mldsa_oid);
            printf("Found ML-DSA OID at position %zu\n", i);
            break;
        }
    }

    if (!found)
    {
        printf("ERROR: ML-DSA extension OID not found in certificate\n");
        return -1;
    }

    /* Skip to OCTET STRING */
    size_t pos = ext_start;

    /* Skip any critical flag (BOOLEAN) */
    if (pos < der_len && der_data[pos] == 0x01)
    {
        pos += 3;
    }

    /* Expect OCTET STRING (0x04) */
    if (pos >= der_len || der_data[pos] != 0x04)
    {
        printf("ERROR: Expected OCTET STRING at position %zu, got 0x%02x\n",
               pos, pos < der_len ? der_data[pos] : 0);
        return -1;
    }
    pos++;

    /* Parse length */
    size_t length;
    if (pos >= der_len)
        return -1;

    if ((der_data[pos] & 0x80) == 0)
    {
        length = der_data[pos];
        pos++;
    }
    else if ((der_data[pos] & 0x7F) == 1)
    {
        pos++;
        if (pos >= der_len)
            return -1;
        length = der_data[pos];
        pos++;
    }
    else if ((der_data[pos] & 0x7F) == 2)
    {
        pos++;
        if (pos + 1 >= der_len)
            return -1;
        length = (der_data[pos] << 8) | der_data[pos + 1];
        pos += 2;
    }
    else
    {
        printf("ERROR: Unsupported length encoding\n");
        return -1;
    }

    printf("Extension length: %zu bytes\n", length);

    if (length != MLDSA65_PUBLIC_KEY_BYTES)
    {
        printf("ERROR: Extension length %zu != expected %d\n",
               length, MLDSA65_PUBLIC_KEY_BYTES);
        return -1;
    }

    if (pos + length > der_len)
    {
        printf("ERROR: Extension data exceeds certificate bounds\n");
        return -1;
    }

    /* Extract public key */
    memcpy(pubkey, der_data + pos, MLDSA65_PUBLIC_KEY_BYTES);
    printf("Successfully extracted ML-DSA public key!\n");

    return 0;
}

int main(void)
{
    printf("=== ML-DSA Certificate Extraction Test ===\n\n");

    /* Load the hybrid certificate */
    const char *cert_path = "/home/ipsec/PQGM-IPSec/docker/initiator/certs/mldsa/initiator_hybrid_cert.pem";

    printf("1. Loading certificate from: %s\n", cert_path);
    FILE *fp = fopen(cert_path, "r");
    if (!fp)
    {
        printf("ERROR: Cannot open certificate file\n");
        return 1;
    }

    X509 *cert = PEM_read_X509(fp, NULL, NULL, NULL);
    fclose(fp);

    if (!cert)
    {
        printf("ERROR: Failed to parse certificate\n");
        return 1;
    }
    printf("   Certificate loaded successfully\n");

    /* Get certificate subject */
    char subject[256];
    X509_NAME_oneline(X509_get_subject_name(cert), subject, sizeof(subject));
    printf("   Subject: %s\n", subject);

    /* Convert to DER for parsing */
    printf("\n2. Converting to DER format...\n");
    int der_len = i2d_X509(cert, NULL);
    if (der_len <= 0)
    {
        printf("ERROR: Failed to get DER length\n");
        X509_free(cert);
        return 1;
    }

    uint8_t *der_data = malloc(der_len);
    uint8_t *der_ptr = der_data;
    i2d_X509(cert, &der_ptr);
    printf("   DER length: %d bytes\n", der_len);

    /* Extract ML-DSA public key */
    printf("\n3. Extracting ML-DSA public key from extension...\n");
    uint8_t mldsa_pubkey[MLDSA65_PUBLIC_KEY_BYTES];

    if (extract_mldsa_pubkey_from_der(der_data, der_len, mldsa_pubkey) != 0)
    {
        free(der_data);
        X509_free(cert);
        return 1;
    }

    /* Print first 32 bytes of the public key */
    printf("\n4. Public key (first 32 bytes):\n   ");
    for (int i = 0; i < 32 && i < MLDSA65_PUBLIC_KEY_BYTES; i++)
    {
        printf("%02x", mldsa_pubkey[i]);
        if ((i + 1) % 16 == 0)
            printf("\n   ");
    }
    printf("\n");

    /* Load the private key */
    printf("\n5. Loading ML-DSA private key...\n");
    const char *key_path = "/home/ipsec/PQGM-IPSec/docker/initiator/certs/mldsa/initiator_mldsa_key.bin";
    fp = fopen(key_path, "rb");
    if (!fp)
    {
        printf("ERROR: Cannot open private key file\n");
        free(der_data);
        X509_free(cert);
        return 1;
    }

    uint8_t mldsa_privkey[MLDSA65_SECRET_KEY_BYTES];
    if (fread(mldsa_privkey, 1, MLDSA65_SECRET_KEY_BYTES, fp) != MLDSA65_SECRET_KEY_BYTES)
    {
        printf("ERROR: Failed to read private key\n");
        fclose(fp);
        free(der_data);
        X509_free(cert);
        return 1;
    }
    fclose(fp);
    printf("   Private key loaded (%d bytes)\n", MLDSA65_SECRET_KEY_BYTES);

    /* Test sign/verify */
    printf("\n6. Testing sign/verify with extracted public key...\n");
    OQS_SIG *sig = OQS_SIG_new(OQS_SIG_alg_ml_dsa_65);
    if (!sig)
    {
        printf("ERROR: Failed to create OQS_SIG context\n");
        free(der_data);
        X509_free(cert);
        return 1;
    }

    uint8_t message[] = "Test message for ML-DSA hybrid certificate";
    size_t message_len = sizeof(message) - 1;
    uint8_t signature[MLDSA65_SIGNATURE_BYTES];
    size_t sig_len = MLDSA65_SIGNATURE_BYTES;

    /* Sign */
    if (OQS_SIG_sign(sig, signature, &sig_len, message, message_len, mldsa_privkey) != OQS_SUCCESS)
    {
        printf("ERROR: Signature generation failed\n");
        OQS_SIG_free(sig);
        free(der_data);
        X509_free(cert);
        return 1;
    }
    printf("   Signature generated (%zu bytes)\n", sig_len);

    /* Verify with extracted public key */
    if (OQS_SIG_verify(sig, message, message_len, signature, sig_len, mldsa_pubkey) != OQS_SUCCESS)
    {
        printf("ERROR: Signature verification FAILED\n");
        OQS_SIG_free(sig);
        free(der_data);
        X509_free(cert);
        return 1;
    }
    printf("   Signature verification PASSED\n");

    /* Cleanup */
    OQS_SIG_free(sig);
    free(der_data);
    X509_free(cert);

    printf("\n=== ALL TESTS PASSED ===\n");
    printf("\nConclusion:\n");
    printf("- ML-DSA public key can be extracted from certificate extension\n");
    printf("- The extracted public key works for signature verification\n");
    printf("- Hybrid certificate approach is viable for ML-DSA authentication\n");

    return 0;
}
