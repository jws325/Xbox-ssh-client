#ifndef TEST_LWIP_IP4_H
#define TEST_LWIP_IP4_H
#include <stdint.h>
typedef struct { uint8_t octet[4]; } ip4_addr_t;
#define IP4_ADDR(a,w,x,y,z) do { (a)->octet[0]=(w);(a)->octet[1]=(x);(a)->octet[2]=(y);(a)->octet[3]=(z); } while(0)
#define ip4_addr1(a) ((a)->octet[0])
#define ip4_addr2(a) ((a)->octet[1])
#define ip4_addr3(a) ((a)->octet[2])
#define ip4_addr4(a) ((a)->octet[3])
#define ip4_addr_isany_val(a) (!(a).octet[0] && !(a).octet[1] && !(a).octet[2] && !(a).octet[3])
#endif
