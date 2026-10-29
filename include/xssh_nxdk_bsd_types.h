/* Compatibility types for libssh2 1.11.1 OpenBSD ChaCha20/Poly1305. */
#ifndef XSSH_NXDK_BSD_TYPES_H
#define XSSH_NXDK_BSD_TYPES_H
#if defined(NXDK)
#include <stddef.h>
#include <stdint.h>
/* Original Xbox is a 32-bit target. These are the BSD aliases used
 * in the vendored algorithm signatures and state structures. */
typedef uint8_t u_char;
typedef uint32_t u_int;
#endif
#endif
