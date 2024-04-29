/**
 * Wii64 - IPLFont.h
 * Copyright (C) 2009 sepp256
 *
 * Wii64 homepage: http://www.emulatemii.com
 * email address: sepp256@gmail.com
 *
 *
 * This program is free software; you can redistribute it and/
 * or modify it under the terms of the GNU General Public Li-
 * cence as published by the Free Software Foundation; either
 * version 2 of the Licence, or any later version.
 *
 * This program is distributed in the hope that it will be use-
 * ful, but WITHOUT ANY WARRANTY; without even the implied war-
 * ranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public Licence for more details.
 *
**/

#ifndef WIIFONT_H
#define WIIFONT_H

class wiiFont
{
public:
	uint16_t* getPngPosByCharCode(wchar_t unicode);
	wchar_t* charToWideChar(char* strChar) ;
	wchar_t* charToWideChar(const char* strChar);
	void getMsgMaxLen(int lenInfo[3], const char* msg, int maxPixelLen);
	int getMaxLen(const char* msg, int maxPixelLen);
	int getMsgPxLen(const char* msg);
	int getAllMsgPxLen(const char* msg);
	void setFontBufByMsg(uint16_t* rguiFramebuf, const char* msg, size_t pitch, int x, int y, uint16_t color);
	void setFontBufByMsgW(uint16_t* rguiFramebuf, wchar_t* msg, size_t pitch, int x, int y, uint16_t color);
	void setTextMsg(const char* msg, int x, int y, int width, int height, bool double_width, bool double_strike);
	char* getChTitle(char* title);
    static wiiFont& getInstance()
	{
		static wiiFont obj;
		return obj;
	}

private:
    wiiFont();
    ~wiiFont();
    void wiiFontInit();
	void wiiFontClose();
	GXColor changeColor(uint16_t color);
};

#endif
