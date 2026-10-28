#ifndef TEST_LWIP_NETIF_H
#define TEST_LWIP_NETIF_H
#include <lwip/ip4_addr.h>
struct netif { ip4_addr_t ip_addr; ip4_addr_t netmask; ip4_addr_t gw; };
extern struct netif *netif_default;
#define netif_ip4_addr(n) (&(n)->ip_addr)
#define netif_ip4_netmask(n) (&(n)->netmask)
#define netif_ip4_gw(n) (&(n)->gw)
void netif_set_addr(struct netif *n, const ip4_addr_t *a, const ip4_addr_t *m, const ip4_addr_t *g);
#endif
