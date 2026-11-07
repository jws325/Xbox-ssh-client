#ifndef XSSH_KNOWN_HOSTS_H
#define XSSH_KNOWN_HOSTS_H
/* Host key trust is scoped to IPv4 address and TCP port.
   -1: file read/error or malformed arguments
    0: not previously trusted
    1: fingerprint matches stored pin
    2: fingerprint changed (MUST fail closed) */
int xssh_known_host_check(const char *file, const char *host, int port,
                          const char *sha256_hex);
/* Returns 0 on persisted trust; no silent acceptance of untrusted hosts. */
int xssh_known_host_add(const char *file, const char *host, int port,
                        const char *sha256_hex);
#endif
