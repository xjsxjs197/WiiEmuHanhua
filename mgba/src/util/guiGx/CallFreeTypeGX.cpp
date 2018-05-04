#include "FreeTypeGX.h"
#include "font_zh_ttf.h"

extern "C" FreeTypeGX *fontSystemOne;

extern "C" void initFreeTypeGX();
void initFreeTypeGX() {
    if (!fontSystemOne) {
        fontSystemOne = new FreeTypeGX();
        fontSystemOne->loadFont(font_zh_ttf, font_zh_ttf_size, 28, false);
    }
}

extern "C" void deleteFreeTypeGX();
void deleteFreeTypeGX() {
    if (fontSystemOne) {
        delete fontSystemOne;
    }
}

extern "C" uint16_t drawText(int16_t x, int16_t y, wchar_t *text);
uint16_t drawText(int16_t x, int16_t y, wchar_t *text) {
    fontSystemOne->drawText(x, y, text, (GXColor){0xff, 0xff, 0xff, 0xff}, (u16)(FTGX_ALIGN_MASK | FTGX_JUSTIFY_MASK));
}

extern "C" uint16_t getWidth(wchar_t *text);
uint16_t getWidth(wchar_t *text) {
    return fontSystemOne->getWidth(text);
}

extern "C" wchar_t* charToWideChar(char* strChar);
wchar_t* charToWideChar(char* strChar) {
	return fontSystemOne->charToWideChar(strChar);
}
