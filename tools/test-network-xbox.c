#include "network_xbox.h"
#include <lwip/netif.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

static struct netif eth;
struct netif *netif_default = &eth;
static int init_calls, stop_calls, set_calls;
void nxNetInit(void *cfg) {
    assert(cfg==NULL);
    init_calls++;
    memset(&eth,0,sizeof(eth));
}
void dhcp_stop(struct netif *n) { assert(n==&eth); stop_calls++; }
void netif_set_addr(struct netif *n,const ip4_addr_t *ip,const ip4_addr_t *mask,const ip4_addr_t *gw) {
    assert(n==&eth);set_calls++;
    n->ip_addr=*ip;n->netmask=*mask;n->gw=*gw;
}
int main(void) {
    XsshNetworkConfig cfg;
    XsshNetworkState state;
    char error[128];
    xssh_network_default(&cfg);
    assert(!xssh_network_initialize(&cfg,&state,error,sizeof(error)));
    assert(init_calls==1&&stop_calls==0&&set_calls==0);
    assert(!xssh_network_ready(&state));
    IP4_ADDR(&eth.ip_addr,192,168,1,73);
    IP4_ADDR(&eth.netmask,255,255,255,0);
    IP4_ADDR(&eth.gw,192,168,1,1);
    xssh_network_poll(&state);
    assert(!strcmp(state.address,"192.168.1.73"));
    assert(xssh_network_ready(&state));
    /* In static mode, nxdk startup DHCP must be stopped before installing
       the selected address. No DHCP-offer can overwrite it afterwards. */
    cfg.mode=XSSH_NET_STATIC;
    strcpy(cfg.ip,"10.0.0.42");strcpy(cfg.netmask,"255.255.255.0");
    strcpy(cfg.gateway,"10.0.0.1");
    assert(!xssh_network_initialize(&cfg,&state,error,sizeof(error)));
    assert(init_calls==2&&stop_calls==1&&set_calls==1);
    assert(!strcmp(state.address,"10.0.0.42"));
    assert(!strcmp(state.netmask,"255.255.255.0"));
    assert(!strcmp(state.gateway,"10.0.0.1"));
    assert(xssh_network_ready(&state));
    strcpy(cfg.ip,"not-an-ip");
    assert(xssh_network_initialize(&cfg,&state,error,sizeof(error))!=0);
    assert(init_calls==2);
    printf("Mock nxdk/lwIP network adapter tests passed\n");
    return 0;
}
