#ifndef XSSH_NETWORK_XBOX_H
#define XSSH_NETWORK_XBOX_H
#include "network_config.h"
#include <stddef.h>

typedef struct {
    XsshNetworkMode mode;
    int initialized;
    char address[16];
    char netmask[16];
    char gateway[16];
} XsshNetworkState;

/* Start nxdk/lwIP; static mode replaces the initial DHCP lease with an
   explicitly configured address. Returns zero on success. */
int xssh_network_initialize(const XsshNetworkConfig *cfg,
                            XsshNetworkState *state, char *error, size_t cap);
/* Update the visible address after DHCP assignment or carrier changes. */
void xssh_network_poll(XsshNetworkState *state);
int xssh_network_ready(const XsshNetworkState *state);
#endif
