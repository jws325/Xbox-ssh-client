#include "network_config.h"
#include <stdio.h>
#include <string.h>

void xssh_network_default(XsshNetworkConfig *cfg) {
    if (!cfg) return;
    memset(cfg, 0, sizeof(*cfg));
    cfg->mode = XSSH_NET_DHCP;
}

int xssh_network_parse_mode(const char *name, XsshNetworkMode *out) {
    if (!name || !out) return 0;
    if (!strcmp(name, "dhcp")) *out = XSSH_NET_DHCP;
    else if (!strcmp(name, "static")) *out = XSSH_NET_STATIC;
    else return 0;
    return 1;
}

const char *xssh_network_mode_name(XsshNetworkMode mode) {
    switch (mode) {
    case XSSH_NET_DHCP: return "dhcp";
    case XSSH_NET_STATIC: return "static";
    default: return "invalid";
    }
}

/* Strict dotted-quad validation; don't permit octal, hex, + signs, whitespace,
   missing components, or atoi()/inet_addr() ambiguity. */
int xssh_ipv4_parse(const char *text, uint8_t out[4]) {
    if (!text || !out) return 0;
    const char *p = text;
    for (int i = 0; i < 4; ++i) {
        unsigned value = 0;
        int digits = 0;
        while (*p >= '0' && *p <= '9') {
            if (++digits > 3) return 0;
            value = value * 10u + (unsigned)(*p++ - '0');
            if (value > 255u) return 0;
        }
        if (!digits) return 0;
        out[i] = (uint8_t)value;
        if (i == 3) return *p == '\0';
        if (*p++ != '.') return 0;
    }
    return 0;
}

static int mask_contiguous(const uint8_t m[4]) {
    int saw_zero = 0, saw_one = 0;
    for (int i = 0; i < 4; ++i) {
        for (unsigned b = 0x80; b != 0; b >>= 1) {
            if (m[i] & b) {
                if (saw_zero) return 0;
                saw_one = 1;
            } else saw_zero = 1;
        }
    }
    return saw_one && saw_zero;
}

static uint32_t ip_to_u32(const uint8_t ip[4]) {
    return ((uint32_t)ip[0] << 24) | ((uint32_t)ip[1] << 16) |
           ((uint32_t)ip[2] << 8) | ip[3];
}

static int fail(char *error, size_t cap, const char *why) {
    if (error && cap) snprintf(error, cap, "%s", why);
    return 0;
}

int xssh_network_validate(const XsshNetworkConfig *cfg, char *error, size_t cap) {
    uint8_t ip[4], mask[4], gw[4];
    uint32_t addr, netmask, gateway, hostpart;
    if (error && cap) error[0] = '\0';
    if (!cfg) return fail(error, cap, "Missing network configuration");
    if (cfg->mode == XSSH_NET_DHCP) return 1;
    if (cfg->mode != XSSH_NET_STATIC) return fail(error, cap, "Unknown network_mode");
    if (!xssh_ipv4_parse(cfg->ip, ip)) return fail(error, cap, "Invalid network_ip");
    if (!xssh_ipv4_parse(cfg->netmask, mask) || !mask_contiguous(mask))
        return fail(error, cap, "Invalid network_netmask (use contiguous mask)");
    if (!xssh_ipv4_parse(cfg->gateway, gw)) return fail(error, cap, "Invalid network_gateway");
    addr = ip_to_u32(ip);
    netmask = ip_to_u32(mask);
    gateway = ip_to_u32(gw);
    hostpart = addr & ~netmask;
    if (ip[0] == 0 || ip[0] >= 224 || hostpart == 0 || hostpart == ~netmask)
        return fail(error, cap, "network_ip must be a unicast host address");
    /* 0.0.0.0 permits on-link SSH servers without an external gateway. */
    if (gateway && ((gateway & netmask) != (addr & netmask) || gateway == addr ||
                    (gateway & ~netmask) == 0 || (gateway & ~netmask) == ~netmask))
        return fail(error, cap, "network_gateway must be reachable on the local subnet");
    return 1;
}
