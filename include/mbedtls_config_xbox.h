/* Mbed TLS v3.6.x crypto-only build configuration for NXDK / original Xbox. */
#ifndef XSSH_MBEDTLS_CONFIG_H
#define XSSH_MBEDTLS_CONFIG_H
#define MBEDTLS_CONFIG_VERSION 0x03000000
/* nxdk uses a Clang-based MSVC-compatible target. Some builds define
 * _MSC_VER without providing the Microsoft SDK <sal.h>. Preselect the
 * compiler's supported return-value annotation so Mbed TLS 3.6.5 does
 * not enter its MSVC-specific <sal.h> branch in platform_util.h.
 * Retains checks for ignored cryptographic function return values.
 */
#define MBEDTLS_CHECK_RETURN __attribute__((__warn_unused_result__))
#define MBEDTLS_NO_PLATFORM_ENTROPY
#define MBEDTLS_ENTROPY_HARDWARE_ALT
/* libssh2 references the mbedtls_pk_parse_keyfile() API. */
#define MBEDTLS_FS_IO
#define MBEDTLS_PLATFORM_C
#define MBEDTLS_AES_C
#define MBEDTLS_ASN1_PARSE_C
#define MBEDTLS_ASN1_WRITE_C
#define MBEDTLS_BASE64_C
#define MBEDTLS_BIGNUM_C
#define MBEDTLS_CIPHER_C
#define MBEDTLS_CIPHER_MODE_CBC
#define MBEDTLS_CIPHER_MODE_CTR
#define MBEDTLS_CTR_DRBG_C
#define MBEDTLS_DES_C
#define MBEDTLS_DHM_C
#define MBEDTLS_ECP_C
#define MBEDTLS_ECDH_C
#define MBEDTLS_ECDSA_C
#define MBEDTLS_ECP_DP_SECP256R1_ENABLED
#define MBEDTLS_ECP_DP_SECP384R1_ENABLED
#define MBEDTLS_ECP_DP_SECP521R1_ENABLED
/* Provide mbedtls_strerror() used by the libssh2 Mbed TLS backend. */
#define MBEDTLS_ERROR_C
#define MBEDTLS_ENTROPY_C
#define MBEDTLS_MD_C
#define MBEDTLS_MD5_C
#define MBEDTLS_OID_C
#define MBEDTLS_PEM_PARSE_C
#define MBEDTLS_PEM_WRITE_C
#define MBEDTLS_PK_C
#define MBEDTLS_PK_PARSE_C
#define MBEDTLS_PK_WRITE_C
#define MBEDTLS_PKCS5_C
#define MBEDTLS_PKCS12_C
#define MBEDTLS_RIPEMD160_C
/* SSH RSA signatures use PKCS#1 v1.5 padding with SHA-2 hashes. */
#define MBEDTLS_PKCS1_V15
#define MBEDTLS_RSA_C
#define MBEDTLS_SHA1_C
#define MBEDTLS_SHA256_C
#define MBEDTLS_SHA512_C
#endif
