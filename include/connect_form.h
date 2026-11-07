#ifndef XSSH_CONNECT_FORM_H
#define XSSH_CONNECT_FORM_H
#include <stddef.h>

#define XSSH_HOST_MAX 80
#define XSSH_PORT_MAX 6
#define XSSH_USER_MAX 80
#define XSSH_PASSWORD_MAX 128

typedef enum {
    XSSH_FORM_HOST = 0,
    XSSH_FORM_PORT,
    XSSH_FORM_USER,
    XSSH_FORM_PASSWORD,
    XSSH_FORM_COUNT
} XsshFormField;

typedef struct {
    char host[XSSH_HOST_MAX];
    char port[XSSH_PORT_MAX];
    char user[XSSH_USER_MAX];
    char password[XSSH_PASSWORD_MAX];
    int field;
    unsigned char replace_on_type[XSSH_FORM_COUNT];
} XsshConnectForm;

void xssh_form_init(XsshConnectForm *f, const char *host, int port, const char *user);
void xssh_form_zero_password(XsshConnectForm *f);
int xssh_form_type(XsshConnectForm *f, unsigned char c);
void xssh_form_backspace(XsshConnectForm *f);
/* Returns 1 if this field is valid and advancing is allowed. */
int xssh_form_validate(const XsshConnectForm *f, int field);
int xssh_form_port(const XsshConnectForm *f);
const char *xssh_form_field_name(int field);
#endif
