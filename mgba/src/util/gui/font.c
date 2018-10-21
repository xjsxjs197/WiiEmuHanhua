/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba-util/gui/font.h>
#include <mgba-util/string.h>

extern const char *gettext(const char *msg);
extern uint16_t getWidth(wchar_t *text);
extern wchar_t* charToWideChar(char* strChar);
extern uint16_t drawText(int16_t x, int16_t y, wchar_t *text);
extern void freeWideCharBuf(wchar_t* strWChar);

unsigned GUIFontSpanWidth(const struct GUIFont* font, const char* text) {
	unsigned width = 0;
	char txtWithoutIcon[5];
	char *utf8Txt = (char *)gettext(text);

	wchar_t * widText = charToWideChar(utf8Txt);
	return getWidth(widText);


	/*size_t len = strlen(utf8Txt);
	while (len) {
		uint32_t c = utf8Char(&utf8Txt, &len);
		if (c == '\1') {
			c = utf8Char(&utf8Txt, &len);
			if (c < GUI_ICON_MAX) {
				unsigned w;
				GUIFontIconMetrics(font, c, &w, 0);
				width += w;
			}
		} else {
		    size_t charLen = toUtf8(c, txtWithoutIcon);
		    txtWithoutIcon[charLen] = '\0';
		    wchar_t * widText = charToWideChar(txtWithoutIcon);
			width += getWidth(widText);

			//freeWideCharBuf(widText);
		}
	}

	return width;*/
}

void GUIFontPrint(const struct GUIFont* font, int x, int y, enum GUIAlignment align, uint32_t color, const char* text) {

    char txtWithoutIcon[5];
	switch (align & GUI_ALIGN_HCENTER) {
	case GUI_ALIGN_HCENTER:
		x -= GUIFontSpanWidth(font, text) / 2;
		break;
	case GUI_ALIGN_RIGHT:
		x -= GUIFontSpanWidth(font, text);
		break;
	default:
		break;
	}

    char *utf8Txt = (char *)(gettext(text));
    drawText(x, y, charToWideChar(utf8Txt));
	/*size_t len = strlen(utf8Txt);
	while (len) {
		uint32_t c = utf8Char(&utf8Txt, &len);
		if (c == '\1') {
			c = utf8Char(&utf8Txt, &len);
			if (c < GUI_ICON_MAX) {
				GUIFontDrawIcon(font, x, y, GUI_ALIGN_BOTTOM, GUI_ORIENT_0, color, c);
				unsigned w;
				GUIFontIconMetrics(font, c, &w, 0);
				x += w;
			}
		} else {
			size_t charLen = toUtf8(c, txtWithoutIcon);
		    txtWithoutIcon[charLen] = '\0';
		    wchar_t * widText = charToWideChar(txtWithoutIcon);
			drawText(x, y, widText);
            x += getWidth(widText);

            //freeWideCharBuf(widText);
		}
	}*/
}

void GUIFontPrintf(const struct GUIFont* font, int x, int y, enum GUIAlignment align, uint32_t color, const char* text, ...) {
	char buffer[256];
	va_list args;
	va_start(args, text);
	vsnprintf(buffer, sizeof(buffer), text, args);
	va_end(args);
	GUIFontPrint(font, x, y, align, color, buffer);
}
