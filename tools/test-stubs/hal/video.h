#ifndef TEST_HAL_VIDEO_H
#define TEST_HAL_VIDEO_H
typedef unsigned long DWORD;
typedef struct { int width, height, bpp, refresh; } VIDEO_MODE;
#define VIDEO_ADAPTER_MASK 0xff
#define VIDEO_MODE_480P 0x80000
#define VIDEO_STANDARD_MASK 0xff00
unsigned long xssh_test_video_reg(unsigned offset);
#define VIDEOREG(offset) xssh_test_video_reg(offset)
#define REFRESH_DEFAULT 0
DWORD XVideoGetEncoderSettings(void);
int XVideoListModes(VIDEO_MODE *out,int bpp,int refresh,void **position);
int XVideoSetMode(int width,int height,int bpp,int refresh);
VIDEO_MODE XVideoGetMode(void);
#endif
