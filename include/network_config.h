#ifndef XSSH_NETWORK_CONFIG_H
#define XSSH_NETWORK_CONFIG_H
#include <stddef.h>
#include <stdint.h>

typedef enum {
    XSSH_NET_DHCP = 0,
    XSSH_NET_STATIC = 1
} XsshNetworkMode;

typedef struct {
    XsshNetworkMode mode;
    char ip[16];
    char netmask[16];
    char gateway[16];
} XsshNetworkConfig;

/* In DHCP mode, nxdk obtains a fresh lease. Dashboard network settings
   are not assumed to survive launching an XBE. */
void xssh_network_default(XsshNetworkConfig *cfg);
int xssh_network_parse_mode(const char *name, XsshNetworkMode *out);
int xssh_ipv4_parse(const char *text, uint8_t octets[4]);
int xssh_network_validate(const XsshNetworkConfig *cfg, char *error, size_t cap);
const char *xssh_network_mode_name(XsshNetworkMode mode);
#endif
