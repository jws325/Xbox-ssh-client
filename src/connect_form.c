#include "connect_form.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void copy_field(char *dst, size_t cap, const char *src) {
    if(!src)src="";
    snprintf(dst,cap,"%s",src);
}
static char *field_ptr(XsshConnectForm *f, int field, size_t *cap) {
    switch(field) {
    case XSSH_FORM_HOST: *cap=sizeof(f->host);return f->host;
    case XSSH_FORM_PORT: *cap=sizeof(f->port);return f->port;
    case XSSH_FORM_USER: *cap=sizeof(f->user);return f->user;
    case XSSH_FORM_PASSWORD: *cap=sizeof(f->password);return f->password;
    default: *cap=0;return NULL;
    }
}
void xssh_form_init(XsshConnectForm *f, const char *host, int port, const char *user) {
    memset(f,0,sizeof(*f));
    copy_field(f->host,sizeof(f->host),host);
    if(port<1 || port>65535)port=22;
    snprintf(f->port,sizeof(f->port),"%d",port);
    copy_field(f->user,sizeof(f->user),user);
    f->field=XSSH_FORM_HOST;
    f->replace_on_type[XSSH_FORM_HOST]=f->host[0]!=0;
    f->replace_on_type[XSSH_FORM_PORT]=1;
    f->replace_on_type[XSSH_FORM_USER]=f->user[0]!=0;
}
void xssh_form_zero_password(XsshConnectForm *f) {
    volatile char *p=f->password;
    for(size_t i=0;i<sizeof(f->password);i++)p[i]=0;
}
int xssh_form_type(XsshConnectForm *f, unsigned char c) {
    size_t cap=0;
    char *p=field_ptr(f,f->field,&cap);
    if(!p || c<32 || c>126)return 0;
    if(f->field==XSSH_FORM_PORT && (c<'0' || c>'9'))return 0;
    if(f->field==XSSH_FORM_HOST && !((c>='0' && c<='9') || c=='.'))return 0;
    if(f->replace_on_type[f->field]) {p[0]=0;f->replace_on_type[f->field]=0;}
    size_t len=strlen(p);
    if(len+1>=cap)return 0;
    p[len]=(char)c;p[len+1]=0;
    return 1;
}
void xssh_form_backspace(XsshConnectForm *f) {
    size_t cap=0;
    char *p=field_ptr(f,f->field,&cap);
    if(!p)return;
    f->replace_on_type[f->field]=0;
    size_t len=strlen(p);
    if(len)p[len-1]=0;
}
/* Accept only dotted decimal IPv4, not hostnames, partial addresses or junk. */
static int valid_ipv4(const char *s) {
    for(int part=0;part<4;part++) {
        unsigned int v=0;
        int count=0;
        while(*s>='0' && *s<='9') {
            v=v*10u+(unsigned)(*s++-'0');
            if(++count>3 || v>255)return 0;
        }
        if(!count)return 0;
        if(part<3) {if(*s++!='.')return 0;}
        else if(*s)return 0;
    }
    return 1;
}
int xssh_form_port(const XsshConnectForm *f) {
    const char *p=f->port;
    unsigned long port=0;
    if(!*p)return 0;
    for(;*p;p++) {
        if(*p<'0'||*p>'9')return 0;
        port=port*10u+(unsigned)(*p-'0');
        if(port>65535)return 0;
    }
    return port>0?(int)port:0;
}
int xssh_form_validate(const XsshConnectForm *f, int field) {
    switch(field) {
    case XSSH_FORM_HOST:return valid_ipv4(f->host) && strcmp(f->host,"255.255.255.255")!=0;
    case XSSH_FORM_PORT:return xssh_form_port(f)>0;
    case XSSH_FORM_USER:return f->user[0]!=0;
    case XSSH_FORM_PASSWORD:return f->password[0]!=0;
    default:return 0;
    }
}
const char *xssh_form_field_name(int field) {
    static const char *names[]={"SERVER IP","PORT","USERNAME","PASSWORD"};
    return field>=0 && field<XSSH_FORM_COUNT?names[field]:"UNKNOWN";
}
