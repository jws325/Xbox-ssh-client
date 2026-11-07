#include "connect_form.h"
#include "known_hosts.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

static void type_text(XsshConnectForm *f,const char *s) {
    for(;*s;s++)assert(xssh_form_type(f,(unsigned char)*s));
}
int main(void) {
    XsshConnectForm f;
    xssh_form_init(&f,"192.168.0.1",22,"root");
    assert(f.field==XSSH_FORM_HOST);
    assert(xssh_form_validate(&f,XSSH_FORM_HOST));
    type_text(&f,"10.5.0.3");
    assert(strcmp(f.host,"10.5.0.3")==0); /* overwrites config defaults */
    f.field=XSSH_FORM_PORT;
    type_text(&f,"2222");
    assert(xssh_form_port(&f)==2222);
    f.field=XSSH_FORM_USER;
    type_text(&f,"james");
    assert(!strcmp(f.user,"james"));
    f.field=XSSH_FORM_PASSWORD;
    type_text(&f,"MySecret!47");
    assert(xssh_form_validate(&f,XSSH_FORM_PASSWORD));
    xssh_form_backspace(&f);
    assert(!strcmp(f.password,"MySecret!4"));
    xssh_form_zero_password(&f);
    for(size_t i=0;i<sizeof(f.password);i++)assert(!f.password[i]);
    assert(!xssh_form_validate(&f,XSSH_FORM_PASSWORD));
    f.field=XSSH_FORM_HOST;
    strcpy(f.host,"256.2.3.4");assert(!xssh_form_validate(&f,XSSH_FORM_HOST));
    strcpy(f.host,"1.2.3");assert(!xssh_form_validate(&f,XSSH_FORM_HOST));
    strcpy(f.host,"1.2.3.4junk");assert(!xssh_form_validate(&f,XSSH_FORM_HOST));
    strcpy(f.host,"192.168.1.99");assert(xssh_form_validate(&f,XSSH_FORM_HOST));
    f.field=XSSH_FORM_PORT;strcpy(f.port,"0");assert(!xssh_form_validate(&f,XSSH_FORM_PORT));
    strcpy(f.port,"65535");assert(xssh_form_validate(&f,XSSH_FORM_PORT));
    strcpy(f.port,"65536");assert(!xssh_form_validate(&f,XSSH_FORM_PORT));

    char path[128];
    snprintf(path,sizeof(path),"/tmp/xssh-known-hosts-%ld.txt",(long)getpid());
    remove(path);
    const char *key="0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
    const char *bad="1123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
    assert(xssh_known_host_check(path,"10.5.0.3",22,key)==0);
    assert(xssh_known_host_add(path,"10.5.0.3",22,key)==0);
    assert(xssh_known_host_check(path,"10.5.0.3",22,key)==1);
    assert(xssh_known_host_check(path,"10.5.0.3",22,bad)==2);
    assert(xssh_known_host_check(path,"10.5.0.3",2222,key)==0);
    assert(xssh_known_host_add(path,"10.5.0.3",22,bad)==-1);
    assert(xssh_known_host_add(path,"10.5.0.3",22,key)==0);
    assert(xssh_known_host_add(path,"10.5.0.3",2222,key)==0);
    assert(xssh_known_host_check(path,"10.5.0.3",2222,key)==1);
    assert(xssh_known_host_check(path,"10.5.0.3",22,bad)==2);
    remove(path);
    puts("PASS: interactive form validation, prefill replacement, password scrub, host key trust/rotation");
    return 0;
}
