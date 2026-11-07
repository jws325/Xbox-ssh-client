#ifndef XSSH_SAVER_H
#define XSSH_SAVER_H
#include <stdint.h>
#include <stdio.h>
#define XSSH_SAVER_HEADER 24
#define XSSH_SAVER_MAX_WIDTH 640
#define XSSH_SAVER_MAX_HEIGHT 480
typedef struct {
 FILE *file;
 uint32_t width,height,fps,frames;
 uint32_t current_frame;
 uint8_t *pixels;
} XsshSaver;
int xssh_saver_open(XsshSaver *s, const char *path);
int xssh_saver_frame(XsshSaver *s, uint32_t frame);
void xssh_saver_close(XsshSaver *s);
#endif
