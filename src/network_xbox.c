#include "network_xbox.h"
#include <nxdk/net.h>
#include <lwip/netif.h>
#include <lwip/dhcp.h>
#include <lwip/ip4_addr.h>
#include <stdio.h>
#include <string.h>

/* nxdk owns the Ethernet interface. Do not create another one or change
   its MAC; nxNetInit(NULL) initializes that interface and starts DHCP.
   XSSH uses lwIP's default interface directly. This must be performed from
   the nxdk main thread, not a USB interrupt callback. */
static void address_text(const ip4_addr_t *a, char out[16]) {
    if (!a || ip4_addr_isany_val(*a)) {
        strcpy(out, "0.0.0.0");
        return;
    }
    snprintf(out, 16, "%u.%u.%u.%u",
             (unsigned)ip4_addr1(a), (unsigned)ip4_addr2(a),
             (unsigned)ip4_addr3(a), (unsigned)ip4_addr4(a));
}

void xssh_network_poll(XsshNetworkState *state) {
    if (!state || !state->initialized) return;
    struct netif *iface = netif_default;
    if (!iface) {
        strcpy(state->address, "0.0.0.0");
        strcpy(state->netmask, "0.0.0.0");
        strcpy(state->gateway, "0.0.0.0");
        return;
    }
    address_text(netif_ip4_addr(iface), state->address);
    address_text(netif_ip4_netmask(iface), state->netmask);
    address_text(netif_ip4_gw(iface), state->gateway);
}

int xssh_network_ready(const XsshNetworkState *state) {
    return state && state->initialized &&
           strcmp(state->address, "0.0.0.0") != 0 &&
           strcmp(state->address, "") != 0;
}

int xssh_network_initialize(const XsshNetworkConfig *cfg,
                            XsshNetworkState *state, char *error, size_t cap) {
    uint8_t ip[4], mask[4], gw[4];
    ip4_addr_t a, m, g;
    if (!state) return -1;
    memset(state, 0, sizeof(*state));
    if (!xssh_network_validate(cfg, error, cap)) return -1;
    state->mode = cfg->mode;
    nxNetInit(NULL);
    struct netif *iface = netif_default;
    if (!iface) {
        if (error && cap) snprintf(error, cap, "nxdk did not expose an Ethernet interface");
        return -1;
    }
    if (cfg->mode == XSSH_NET_STATIC) {
        /* Stops DHCP before setting the static address, preventing a late
           DHCP offer from overwriting the caller's chosen address. */
        dhcp_stop(iface);
        xssh_ipv4_parse(cfg->ip, ip);
        xssh_ipv4_parse(cfg->netmask, mask);
        xssh_ipv4_parse(cfg->gateway, gw);
        IP4_ADDR(&a, ip[0],ip[1],ip[2],ip[3]);
        IP4_ADDR(&m, mask[0],mask[1],mask[2],mask[3]);
        IP4_ADDR(&g, gw[0],gw[1],gw[2],gw[3]);
        netif_set_addr(iface, &a, &m, &g);
    }
    state->initialized = 1;
    xssh_network_poll(state);
    if (error && cap) error[0] = '\0';
    return 0;
}
