/*****************************************************************************
 * font.h - C functions to call IPLFont singleton class
 *****************************************************************************/
#ifdef __cplusplus
extern "C" {
#endif

#define MAX_FRAME_PIXEL_LEN 280

void wiiFont_getMsgMaxLen(int lenInfo[3], const char* msg, int maxPixelLen);
int wiiFont_getMaxLen(const char* msg, int maxPixelLen);
int wiiFont_getMsgPxLen(const char* msg);
int wiiFont_getAllMsgPxLen(const char* msg);
void wiiFont_setFontBufByMsg(uint16_t* rguiFramebuf, const char* msg, size_t pitch, int x, int y, uint16_t color);
void wiiFont_setTextMsg(const char* msg, int x, int y, int width, int height, bool double_width, bool double_strike);
char* wiiFont_getChTitle(char* title);

#ifdef __cplusplus
}
#endif
