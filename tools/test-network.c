#include "network_config.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    XsshNetworkConfig c;
    XsshNetworkMode mode;
    char msg[128];
    uint8_t octets[4];
    xssh_network_default(&c);
    assert(c.mode == XSSH_NET_DHCP);
    assert(xssh_network_validate(&c, msg, sizeof(msg)));
    assert(!strcmp(xssh_network_mode_name(c.mode), "dhcp"));
    assert(xssh_network_parse_mode("dhcp", &mode) && mode == XSSH_NET_DHCP);
    assert(xssh_network_parse_mode("static", &mode) && mode == XSSH_NET_STATIC);
    assert(!strcmp(xssh_network_mode_name(XSSH_NET_STATIC), "static"));
    assert(!xssh_network_parse_mode("auto", &mode));
    assert(!xssh_network_parse_mode("bogus", &mode));
    c.mode = (XsshNetworkMode)42;
    assert(!xssh_network_validate(&c, msg, sizeof(msg)));
    xssh_network_default(&c);
    assert(c.mode == XSSH_NET_DHCP);
    assert(!xssh_ipv4_parse("1.2.3.999", octets));
    assert(!xssh_ipv4_parse("1.2.3", octets));
    assert(!xssh_ipv4_parse("1.2.3.4.5", octets));
    assert(!xssh_ipv4_parse("1.2.3.-1", octets));
    assert(!xssh_ipv4_parse(" 1.2.3.4", octets));
    assert(xssh_ipv4_parse("192.168.10.20", octets));
    assert(octets[0]==192 && octets[3]==20);
    c.mode=XSSH_NET_STATIC;
    assert(!xssh_network_validate(&c,msg,sizeof(msg)));
    strcpy(c.ip,"192.168.1.50");
    strcpy(c.netmask,"255.255.255.0");
    strcpy(c.gateway,"192.168.1.1");
    assert(xssh_network_validate(&c,msg,sizeof(msg)));
    strcpy(c.netmask,"255.0.255.0");
    assert(!xssh_network_validate(&c,msg,sizeof(msg)));
    strcpy(c.netmask,"255.255.255.0");
    strcpy(c.gateway,"192.168.2.1");
    assert(!xssh_network_validate(&c,msg,sizeof(msg)));
    strcpy(c.gateway,"0.0.0.0");
    assert(xssh_network_validate(&c,msg,sizeof(msg)));
    strcpy(c.ip,"192.168.1.0");
    assert(!xssh_network_validate(&c,msg,sizeof(msg)));
    strcpy(c.ip,"192.168.1.255");
    assert(!xssh_network_validate(&c,msg,sizeof(msg)));
    strcpy(c.ip,"192.168.1.50");
    assert(xssh_network_validate(&c,msg,sizeof(msg)));
    puts("Network configuration tests passed");
    return 0;
}
