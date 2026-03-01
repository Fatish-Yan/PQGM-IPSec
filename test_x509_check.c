#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gmssl/x509_cer.h>
#include <gmssl/x509_key.h>
#include <gmssl/sm2.h>

int main(int argc, char *argv[])
{
	FILE *fp;
	uint8_t cert_der[4096];
	size_t cert_len;
	int ret;
	int path_len;

	if (argc < 2)
	{
		fprintf(stderr, "Usage: %s <cert.pem>\n", argv[0]);
		return 1;
	}

	/* Read PEM certificate and convert to DER */
	fp = fopen(argv[1], "r");
	if (!fp)
	{
		perror("fopen");
		return 1;
	}

	/* Simple PEM to DER conversion (skip header/footer and base64 decode) */
	char line[256];
	uint8_t *p = cert_der;
	cert_len = 0;

	/* Skip until we find the certificate data */
	while (fgets(line, sizeof(line), fp))
	{
		if (strncmp(line, "-----BEGIN", 10) == 0)
			break;
	}
	fclose(fp);

	/* Use GmSSL to load the certificate properly */
	fp = fopen(argv[1], "r");
	if (!fp)
	{
		perror("fopen");
		return 1;
	}

	/* Read entire file */
	char pem_data[8192];
	size_t pem_len = fread(pem_data, 1, sizeof(pem_data), fp);
	fclose(fp);
	pem_data[pem_len] = '\0';

	/* Use GmSSL x509_cert_from_pem */
	extern int x509_cert_from_pem(uint8_t *cert, size_t *certlen, size_t maxlen, FILE *fp);
	fp = fopen(argv[1], "r");
	if (!x509_cert_from_pem(cert_der, &cert_len, sizeof(cert_der), fp))
	{
		fprintf(stderr, "Failed to parse PEM certificate\n");
		fclose(fp);
		return 1;
	}
	fclose(fp);

	printf("Certificate loaded: %zu bytes DER\n", cert_len);

	/* Check different cert types */
	printf("\nTesting x509_cert_check:\n");

	ret = x509_cert_check(cert_der, cert_len, X509_cert_server_auth, &path_len);
	printf("  X509_cert_server_auth (0): %d\n", ret);

	ret = x509_cert_check(cert_der, cert_len, X509_cert_client_auth, &path_len);
	printf("  X509_cert_client_auth (1): %d\n", ret);

	ret = x509_cert_check(cert_der, cert_len, X509_cert_server_key_encipher, &path_len);
	printf("  X509_cert_server_key_encipher (2): %d\n", ret);

	ret = x509_cert_check(cert_der, cert_len, X509_cert_client_key_encipher, &path_len);
	printf("  X509_cert_client_key_encipher (3): %d\n", ret);

	ret = x509_cert_check(cert_der, cert_len, X509_cert_ca, &path_len);
	printf("  X509_cert_ca (4): %d\n", ret);

	ret = x509_cert_check(cert_der, cert_len, X509_cert_root_ca, &path_len);
	printf("  X509_cert_root_ca (5): %d\n", ret);

	/* Extract public key */
	X509_KEY x509_key;
	memset(&x509_key, 0, sizeof(x509_key));
	ret = x509_cert_get_subject_public_key(cert_der, cert_len, &x509_key);
	printf("\nx509_cert_get_subject_public_key: %d\n", ret);
	if (ret == 1)
	{
		printf("  x509_key.algor = %d\n", x509_key.algor);
		printf("  x509_key.algor_param = %d\n", x509_key.algor_param);

		/* Check for SM2 OID values */
		extern int x509_oid_get_algor(int oid, int *algor);
		printf("\n  OID values reference:\n");
		printf("    OID_sm2 = %d (from gmssl/oid.h)\n", 17); /* Known value */
	}

	return 0;
}
