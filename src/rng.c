/*
 * Original Xbox entropy adapter. No claim that the Xbox has a hardware TRNG.
 * Supply a 64-byte secret from a PC CSPRNG at E:\\xssh-seed.bin.
 * The seed file is replaced with a SHA-512-derived successor BEFORE use.
 * The one-way ratchet avoids reusing session seeds across normal boots.
 * This is not a substitute for a real OS CSPRNG, and flash/hard-disk
 * snapshots or compromise of the seed file can compromise future sessions.
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <mbedtls/sha512.h>
#include <mbedtls/entropy.h>
#include "rng.h"

#define SEED_FILE "E:\\xssh-seed.bin"
static unsigned char seed[64];
static uint32_t request_counter;
static int seeded;

static void wipe(void *ptr, size_t n) {
    volatile unsigned char *p=(volatile unsigned char *)ptr;
    while(n--) *p++=0;
}

int xssh_rng_prepare(void) {
    unsigned char next[64], msg[96];
    FILE *f=fopen(SEED_FILE,"rb");
    seeded=0;
    if(!f) return -1;
    if(fread(seed,1,sizeof(seed),f)!=sizeof(seed) || fgetc(f)!=EOF) {
        fclose(f);wipe(seed,sizeof(seed));return -1;
    }
    fclose(f);
    memset(msg,0,sizeof(msg));
    memcpy(msg,"xssh seed ratchet v1",20);
    memcpy(msg+32,seed,64);
    if(mbedtls_sha512(msg,sizeof(msg),next,0)!=0){wipe(seed,64);return -1;}
    wipe(msg,sizeof(msg));
    f=fopen(SEED_FILE,"wb");
    if(!f){wipe(seed,64);wipe(next,64);return -1;}
    size_t written=fwrite(next,1,64,f);
    int closed=fclose(f);
    wipe(next,64);
    if(written!=64||closed!=0){wipe(seed,64);return -1;}
    request_counter=0;
    seeded=1;
    return 0;
}

/* Hook selected with MBEDTLS_ENTROPY_HARDWARE_ALT. */
int mbedtls_hardware_poll(void *data, unsigned char *output,
                          size_t len, size_t *olen) {
    unsigned char msg[72], digest[64];
    size_t produced=0;
    (void)data;
    *olen=0;
    if(!seeded) return MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;
    while(produced<len) {
        memcpy(msg,seed,64);
        uint32_t ctr=++request_counter;
        msg[64]=(unsigned char)(ctr>>24);
        msg[65]=(unsigned char)(ctr>>16);
        msg[66]=(unsigned char)(ctr>>8);
        msg[67]=(unsigned char)ctr;
        memcpy(msg+68,"XSSH",4);
        if(mbedtls_sha512(msg,sizeof(msg),digest,0)!=0)return MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;
        size_t n=(len-produced>64)?64:len-produced;
        memcpy(output+produced,digest,n);
        produced+=n;
    }
    wipe(msg,sizeof(msg));wipe(digest,sizeof(digest));
    *olen=produced;
    return 0;
}
