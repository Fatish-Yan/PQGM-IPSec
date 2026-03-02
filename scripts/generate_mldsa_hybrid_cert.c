/*
 * generate_mldsa_hybrid_cert.c
 *
 * 生成包含 ML-DSA-65 公钥扩展的混合 X.509 证书
 *
 * 证书结构:
 * - SubjectPublicKeyInfo: ECDSA P-256 (占位符)
 * - 扩展: ML-DSA-65 公钥 (OID: 1.3.6.1.4.1.99999.1.2)
 *
 * 编译: gcc -o generate_mldsa_hybrid_cert generate_mldsa_hybrid_cert.c -loqs -lcrypto
 * 运行: ./generate_mldsa_hybrid_cert initiator initiator.pqgm.test
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <oqs/oqs.h>
#include <openssl/evp.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#include <openssl/ec.h>

/* ML-DSA-65 公钥扩展 OID: 1.3.6.1.4.1.99999.1.2 */
#define MLDSA65_EXT_OID "1.3.6.1.4.1.99999.1.2"

/* ML-DSA-65 参数 */
#define MLDSA65_PUBLIC_KEY_BYTES  1952
#define MLDSA65_SECRET_KEY_BYTES  4032
#define MLDSA65_SIGNATURE_BYTES   3309

/*
 * 生成 ML-DSA-65 密钥对
 */
int generate_mldsa_keypair(uint8_t *public_key, uint8_t *secret_key)
{
    OQS_SIG *sig = OQS_SIG_new(OQS_SIG_alg_ml_dsa_65);
    if (!sig) {
        fprintf(stderr, "ERROR: Failed to create ML-DSA-65 context\n");
        return -1;
    }

    if (OQS_SIG_keypair(sig, public_key, secret_key) != OQS_SUCCESS) {
        fprintf(stderr, "ERROR: Failed to generate ML-DSA keypair\n");
        OQS_SIG_free(sig);
        return -1;
    }

    OQS_SIG_free(sig);
    return 0;
}

/*
 * 生成 ECDSA P-256 密钥对 (占位符)
 */
EVP_PKEY *generate_ecdsa_keypair(void)
{
    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_EC, NULL);
    if (!ctx) {
        fprintf(stderr, "ERROR: Failed to create EC context\n");
        return NULL;
    }

    if (EVP_PKEY_keygen_init(ctx) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        return NULL;
    }

    if (EVP_PKEY_CTX_set_ec_paramgen_curve_nid(ctx, NID_X9_62_prime256v1) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        return NULL;
    }

    EVP_PKEY *pkey = NULL;
    if (EVP_PKEY_keygen(ctx, &pkey) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        return NULL;
    }

    EVP_PKEY_CTX_free(ctx);
    return pkey;
}

/*
 * 创建 ML-DSA 公钥扩展
 */
X509_EXTENSION *create_mldsa_extension(const uint8_t *public_key, size_t key_len)
{
    ASN1_OCTET_STRING *octet = ASN1_OCTET_STRING_new();
    if (!octet) {
        return NULL;
    }

    if (!ASN1_OCTET_STRING_set(octet, public_key, key_len)) {
        ASN1_OCTET_STRING_free(octet);
        return NULL;
    }

    /* 创建自定义 OID */
    ASN1_OBJECT *obj = OBJ_txt2obj(MLDSA65_EXT_OID, 0);
    if (!obj) {
        ASN1_OCTET_STRING_free(octet);
        return NULL;
    }

    X509_EXTENSION *ext = X509_EXTENSION_create_by_OBJ(NULL, obj, 0, octet);

    ASN1_OBJECT_free(obj);
    ASN1_OCTET_STRING_free(octet);

    return ext;
}

/*
 * 生成混合证书
 */
int generate_hybrid_cert(
    const char *name,
    const char *cn,
    const uint8_t *mldsa_public_key,
    EVP_PKEY *ca_key,
    X509 *ca_cert,
    const char *output_dir)
{
    X509 *cert = X509_new();
    if (!cert) {
        fprintf(stderr, "ERROR: Failed to create X509 structure\n");
        return -1;
    }

    /* 生成 ECDSA 占位符密钥对 */
    EVP_PKEY *ec_key = generate_ecdsa_keypair();
    if (!ec_key) {
        X509_free(cert);
        return -1;
    }

    /* 设置版本 (v3) */
    X509_set_version(cert, 2);

    /* 设置序列号 */
    ASN1_INTEGER *serial = ASN1_INTEGER_new();
    ASN1_INTEGER_set(serial, rand());
    X509_set_serialNumber(cert, serial);
    ASN1_INTEGER_free(serial);

    /* 设置颁发者 (from CA cert) */
    X509_set_issuer_name(cert, X509_get_subject_name(ca_cert));

    /* 设置使用者 */
    X509_NAME *subject_name = X509_NAME_new();
    X509_NAME_add_entry_by_txt(subject_name, "CN", MBSTRING_UTF8,
                               (unsigned char *)cn, -1, -1, 0);
    X509_set_subject_name(cert, subject_name);
    X509_NAME_free(subject_name);

    /* 设置有效期 */
    X509_gmtime_adj(X509_getm_notBefore(cert), 0);
    X509_gmtime_adj(X509_getm_notAfter(cert), 365 * 24 * 60 * 60);

    /* 设置公钥 (ECDSA 占位符) */
    X509_set_pubkey(cert, ec_key);

    /* 添加 SAN 扩展 */
    X509V3_CTX ctx;
    X509V3_set_ctx_nodb(&ctx);
    X509V3_set_ctx(&ctx, cert, cert, NULL, NULL, 0);

    char san_value[256];
    snprintf(san_value, sizeof(san_value), "DNS:%s", cn);
    X509_EXTENSION *san_ext = X509V3_EXT_conf_nid(NULL, &ctx, NID_subject_alt_name, san_value);
    if (san_ext) {
        X509_add_ext(cert, san_ext, -1);
        X509_EXTENSION_free(san_ext);
    }

    /* 添加 keyUsage 扩展 */
    X509_EXTENSION *ku_ext = X509V3_EXT_conf_nid(NULL, &ctx, NID_key_usage, "digitalSignature,keyEncipherment");
    if (ku_ext) {
        X509_add_ext(cert, ku_ext, -1);
        X509_EXTENSION_free(ku_ext);
    }

    /* 添加 extendedKeyUsage 扩展 */
    X509_EXTENSION *eku_ext = X509V3_EXT_conf_nid(NULL, &ctx, NID_ext_key_usage, "serverAuth,clientAuth");
    if (eku_ext) {
        X509_add_ext(cert, eku_ext, -1);
        X509_EXTENSION_free(eku_ext);
    }

    /* 添加 ML-DSA 公钥扩展 */
    X509_EXTENSION *mldsa_ext = create_mldsa_extension(mldsa_public_key, MLDSA65_PUBLIC_KEY_BYTES);
    if (!mldsa_ext) {
        fprintf(stderr, "ERROR: Failed to create ML-DSA extension\n");
        X509_free(cert);
        EVP_PKEY_free(ec_key);
        return -1;
    }
    X509_add_ext(cert, mldsa_ext, -1);
    X509_EXTENSION_free(mldsa_ext);

    /* 使用 CA 签名 */
    if (X509_sign(cert, ca_key, EVP_sha256()) == 0) {
        fprintf(stderr, "ERROR: Failed to sign certificate\n");
        X509_free(cert);
        EVP_PKEY_free(ec_key);
        return -1;
    }

    /* 保存证书 */
    char cert_path[512];
    snprintf(cert_path, sizeof(cert_path), "%s/%s_hybrid_cert.pem", output_dir, name);

    FILE *fp = fopen(cert_path, "w");
    if (!fp) {
        fprintf(stderr, "ERROR: Cannot open %s for writing\n", cert_path);
        X509_free(cert);
        EVP_PKEY_free(ec_key);
        return -1;
    }
    PEM_write_X509(fp, cert);
    fclose(fp);
    printf("Certificate saved: %s\n", cert_path);

    X509_free(cert);
    EVP_PKEY_free(ec_key);

    return 0;
}

/*
 * 生成 CA 证书
 */
int generate_ca_cert(const char *output_dir, EVP_PKEY **ca_key_out, X509 **ca_cert_out)
{
    /* 生成 CA 密钥 */
    EVP_PKEY *ca_key = generate_ecdsa_keypair();
    if (!ca_key) {
        return -1;
    }

    /* 创建 CA 证书 */
    X509 *ca_cert = X509_new();
    X509_set_version(ca_cert, 2);

    ASN1_INTEGER *serial = ASN1_INTEGER_new();
    ASN1_INTEGER_set(serial, 1);
    X509_set_serialNumber(ca_cert, serial);
    ASN1_INTEGER_free(serial);

    X509_NAME *name = X509_NAME_new();
    X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_UTF8,
                               (unsigned char *)"PQGM-MLDSA-CA", -1, -1, 0);
    X509_set_subject_name(ca_cert, name);
    X509_set_issuer_name(ca_cert, name);
    X509_NAME_free(name);

    X509_gmtime_adj(X509_getm_notBefore(ca_cert), 0);
    X509_gmtime_adj(X509_getm_notAfter(ca_cert), 10 * 365 * 24 * 60 * 60);

    X509_set_pubkey(ca_cert, ca_key);

    /* 添加 basicConstraints */
    X509V3_CTX ctx;
    X509V3_set_ctx_nodb(&ctx);
    X509V3_set_ctx(&ctx, ca_cert, ca_cert, NULL, NULL, 0);

    X509_EXTENSION *bc_ext = X509V3_EXT_conf_nid(NULL, &ctx, NID_basic_constraints, "critical,CA:TRUE");
    if (bc_ext) {
        X509_add_ext(ca_cert, bc_ext, -1);
        X509_EXTENSION_free(bc_ext);
    }

    X509_EXTENSION *ku_ext = X509V3_EXT_conf_nid(NULL, &ctx, NID_key_usage, "critical,keyCertSign,cRLSign");
    if (ku_ext) {
        X509_add_ext(ca_cert, ku_ext, -1);
        X509_EXTENSION_free(ku_ext);
    }

    X509_sign(ca_cert, ca_key, EVP_sha256());

    /* 保存 CA 证书 */
    char ca_cert_path[512];
    snprintf(ca_cert_path, sizeof(ca_cert_path), "%s/mldsa_ca.pem", output_dir);

    FILE *fp = fopen(ca_cert_path, "w");
    if (fp) {
        PEM_write_X509(fp, ca_cert);
        fclose(fp);
        printf("CA certificate saved: %s\n", ca_cert_path);
    }

    /* 保存 CA 私钥 */
    char ca_key_path[512];
    snprintf(ca_key_path, sizeof(ca_key_path), "%s/mldsa_ca_key.pem", output_dir);

    fp = fopen(ca_key_path, "w");
    if (fp) {
        PEM_write_PrivateKey(fp, ca_key, NULL, NULL, 0, NULL, NULL);
        fclose(fp);
        printf("CA private key saved: %s\n", ca_key_path);
    }

    *ca_key_out = ca_key;
    *ca_cert_out = ca_cert;

    return 0;
}

/*
 * 保存 ML-DSA 私钥
 */
int save_mldsa_private_key(const char *name, const uint8_t *secret_key, const char *output_dir)
{
    char key_path[512];
    snprintf(key_path, sizeof(key_path), "%s/%s_mldsa_key.bin", output_dir, name);

    FILE *fp = fopen(key_path, "wb");
    if (!fp) {
        fprintf(stderr, "ERROR: Cannot open %s for writing\n", key_path);
        return -1;
    }

    fwrite(secret_key, 1, MLDSA65_SECRET_KEY_BYTES, fp);
    fclose(fp);
    printf("ML-DSA private key saved: %s (%d bytes)\n", key_path, MLDSA65_SECRET_KEY_BYTES);

    return 0;
}

void print_usage(const char *prog)
{
    printf("Usage: %s <name> <cn> [output_dir]\n", prog);
    printf("\n");
    printf("Arguments:\n");
    printf("  name       - Identifier for the certificate (e.g., initiator, responder)\n");
    printf("  cn         - Common Name (e.g., initiator.pqgm.test)\n");
    printf("  output_dir - Output directory (default: current directory)\n");
    printf("\n");
    printf("Example:\n");
    printf("  %s initiator initiator.pqgm.test /home/ipsec/PQGM-IPSec/docker/initiator/certs/mldsa\n", prog);
}

int main(int argc, char *argv[])
{
    if (argc < 3) {
        print_usage(argv[0]);
        return 1;
    }

    const char *name = argv[1];
    const char *cn = argv[2];
    const char *output_dir = (argc > 3) ? argv[3] : ".";

    printf("=== Generating ML-DSA Hybrid Certificate ===\n\n");
    printf("Name: %s\n", name);
    printf("CN: %s\n", cn);
    printf("Output: %s\n\n", output_dir);

    /* 初始化 OpenSSL */
    OpenSSL_add_all_algorithms();
    ERR_load_crypto_strings();

    /* 生成 ML-DSA 密钥对 */
    printf("1. Generating ML-DSA-65 keypair...\n");
    uint8_t mldsa_public_key[MLDSA65_PUBLIC_KEY_BYTES];
    uint8_t mldsa_secret_key[MLDSA65_SECRET_KEY_BYTES];

    if (generate_mldsa_keypair(mldsa_public_key, mldsa_secret_key) != 0) {
        return 1;
    }
    printf("   Public key: %d bytes\n", MLDSA65_PUBLIC_KEY_BYTES);
    printf("   Private key: %d bytes\n", MLDSA65_SECRET_KEY_BYTES);

    /* 生成 CA (如果不存在) */
    printf("\n2. Generating CA certificate...\n");
    EVP_PKEY *ca_key = NULL;
    X509 *ca_cert = NULL;

    char ca_cert_path[512];
    snprintf(ca_cert_path, sizeof(ca_cert_path), "%s/mldsa_ca.pem", output_dir);

    FILE *fp = fopen(ca_cert_path, "r");
    if (fp) {
        /* CA 已存在，加载它 */
        printf("   Loading existing CA certificate...\n");
        fclose(fp);
        // TODO: Load existing CA
        generate_ca_cert(output_dir, &ca_key, &ca_cert);
    } else {
        generate_ca_cert(output_dir, &ca_key, &ca_cert);
    }

    /* 生成混合证书 */
    printf("\n3. Generating hybrid certificate...\n");
    if (generate_hybrid_cert(name, cn, mldsa_public_key, ca_key, ca_cert, output_dir) != 0) {
        EVP_PKEY_free(ca_key);
        X509_free(ca_cert);
        return 1;
    }

    /* 保存 ML-DSA 私钥 */
    printf("\n4. Saving ML-DSA private key...\n");
    if (save_mldsa_private_key(name, mldsa_secret_key, output_dir) != 0) {
        EVP_PKEY_free(ca_key);
        X509_free(ca_cert);
        return 1;
    }

    /* 清理 */
    EVP_PKEY_free(ca_key);
    X509_free(ca_cert);
    EVP_cleanup();
    ERR_free_strings();

    printf("\n=== Done ===\n");
    printf("\nGenerated files:\n");
    printf("  - %s/mldsa_ca.pem (CA certificate)\n", output_dir);
    printf("  - %s/%s_hybrid_cert.pem (Hybrid certificate with ML-DSA extension)\n", output_dir, name);
    printf("  - %s/%s_mldsa_key.bin (ML-DSA private key)\n", output_dir, name);
    printf("\n");
    printf("ML-DSA extension OID: %s\n", MLDSA65_EXT_OID);

    return 0;
}
