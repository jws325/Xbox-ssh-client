#include "known_hosts.h"
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

static int hex64(const char *s) {
    if(strlen(s)!=64)return 0;
    for(int i=0;i<64;i++)if(!isxdigit((unsigned char)s[i]))return 0;
    return 1;
}
static int hex_equal(const char *a,const char *b) {
    unsigned char diff=0;
    for(int i=0;i<64;i++)diff|=(unsigned char)(tolower((unsigned char)a[i])^tolower((unsigned char)b[i]));
    return diff==0;
}
int xssh_known_host_check(const char *path,const char *host,int port,const char *hex) {
    if(!path||!host||!hex||!hex64(hex)||port<1||port>65535)return -1;
    FILE *f=fopen(path,"rb");
    if(!f)return errno==ENOENT?0:-1; /* Only a missing file means first use. */
    char line[256], saved_host[80], saved_hash[65];
    int saved_port, found=0;
    while(fgets(line,sizeof(line),f)) {
        if(sscanf(line,"%79s %d %64s",saved_host,&saved_port,saved_hash)==3 &&
            strcmp(host,saved_host)==0 && port==saved_port) {
            if(!hex64(saved_hash)||!hex_equal(hex,saved_hash)){fclose(f);return 2;}
            found=1;
        }
    }
    if(ferror(f)){fclose(f);return -1;}
    fclose(f);
    return found?1:0;
}
int xssh_known_host_add(const char *path,const char *host,int port,const char *hex) {
    if(!path||!host||!hex||!hex64(hex)||strlen(host)>=80||port<1||port>65535)return -1;
    /* Refuse to append any conflicting host entry. */
    int result=xssh_known_host_check(path,host,port,hex);
    if(result!=0)return result==1?0:-1;
    FILE *f=fopen(path,"ab");
    if(!f)return -1;
    int ok=(fprintf(f,"%s %d %s\n",host,port,hex)>0 && fflush(f)==0);
    if(fclose(f)!=0)ok=0;
    return ok?0:-1;
}
