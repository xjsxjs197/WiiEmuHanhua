/**
 * Wii64 - IPLFont.cpp
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

#include <map>
#include <malloc.h>
#include <string.h>
#include <ogc/gx.h>

#include "wiiFont.h"
#include "wiiFontC.h"
#include "../gfx/drivers_font_renderer/bitmap.h"

#define CHAR_IMG_SIZE 338
//#define CHAR_IMG_SIZE 512
//#define CHAR_IMG_SIZE 200
static std::map<wchar_t, int> charCodeMap;
static std::map<uint16_t, uint16_t> colorMap;
static uint8_t *ZhBufFont_dat;

wiiFont::wiiFont()
{
    wiiFontInit();
}

wiiFont::~wiiFont()
{
	wiiFontClose();
}

void wiiFont::wiiFontInit()
{
    int ZhBufFont_size = 2384766; // RGB5A3
	//int ZhBufFont_size = 3598068; // RGB5A3
	//int ZhBufFont_size = 1422492; // RGB5A3
	int searchLen = (int)(ZhBufFont_size / (4 + CHAR_IMG_SIZE));
	int bufIndex = 0;
	int skipSetp = (CHAR_IMG_SIZE + 4) / 2;
	FILE *charPngFile = fopen("sd:/retroarch/font/ZhBufFont13X13NoBlock_RGB5A3.dat", "rb");
	ZhBufFont_dat = (uint8_t *)memalign(32, ZhBufFont_size);

	fseek(charPngFile, 0, SEEK_SET);
	fread(ZhBufFont_dat, 1, ZhBufFont_size, charPngFile);
    fclose(charPngFile);
    charPngFile = NULL;

	uint16_t *zhFontBufTemp = (uint16_t *)ZhBufFont_dat;
    while (bufIndex < searchLen)
    {
        charCodeMap.insert(std::pair<wchar_t, int>(*zhFontBufTemp, bufIndex * skipSetp));
        zhFontBufTemp += skipSetp;
        bufIndex++;
    }

    colorMap.insert(std::pair<uint16_t, uint16_t>(0, 0));
    colorMap.insert(std::pair<uint16_t, uint16_t>(12287, 8304));
    colorMap.insert(std::pair<uint16_t, uint16_t>(20479, 16496));
    colorMap.insert(std::pair<uint16_t, uint16_t>(16383, 12400));
    colorMap.insert(std::pair<uint16_t, uint16_t>(8191, 4208));
    colorMap.insert(std::pair<uint16_t, uint16_t>(4095, 112));
    colorMap.insert(std::pair<uint16_t, uint16_t>(28671, 24704));
    colorMap.insert(std::pair<uint16_t, uint16_t>(24575, 20608));
    colorMap.insert(std::pair<uint16_t, uint16_t>(65535, 33280));
}

void wiiFont::wiiFontClose()
{
    if (ZhBufFont_dat)
    {
        free(ZhBufFont_dat);
        ZhBufFont_dat = NULL;
    }
}

uint16_t* wiiFont::getPngPosByCharCode(wchar_t unicode)
{
    return ((uint16_t *)ZhBufFont_dat + charCodeMap[unicode] + 1);
}

wchar_t* wiiFont::charToWideChar(char* strChar)
{
    char *tmpPtr = strChar;
    int msgLen = strlen(strChar);
	wchar_t *strWChar = new wchar_t[msgLen + 1];

	int bt = mbstowcs(strWChar, tmpPtr, msgLen);
	if (bt) {
		strWChar[bt] = (wchar_t)'\0';
		return strWChar;
	}

	wchar_t *tempDest = strWChar;
    tmpPtr = strChar;
	while((*tempDest++ = *tmpPtr++));

	return strWChar;
}

wchar_t* wiiFont::charToWideChar(const char* strChar)
{
	return charToWideChar((char*)strChar);
}

int wiiFont::getMaxLen(const char* msg, int maxPixelLen)
{
    int maxLen = 0;
    int retWid = 0;
    uint16_t * charDataPosPtr;
    uint8_t * charInfoPtr;
    wchar_t* unicodeMsg = charToWideChar(msg);
    wchar_t* freePtr = unicodeMsg;
    while (*unicodeMsg)
    {
        charDataPosPtr = getPngPosByCharCode(*unicodeMsg);
        charInfoPtr = (uint8_t *)charDataPosPtr;
        charDataPosPtr++;
        retWid -= *charInfoPtr; // x - paddingLeft

        if (*unicodeMsg < 256)
        {
            retWid += *(charInfoPtr + 1); // x + charWidth
        }
        else
        {
            retWid += *(charInfoPtr + 1) + 1; // x + charWidth
        }
        if (retWid > maxPixelLen)
        {
            return maxLen;
        }

        maxLen++;
        unicodeMsg++;
    }

    free(freePtr);
    return maxLen;
}

void wiiFont::getMsgMaxLen(int lenInfo[3], const char* msg, int maxPixelLen)
{
    int retWid = 0;
    uint16_t * charDataPosPtr;
    uint8_t * charInfoPtr;
    lenInfo[0] = 0;
    lenInfo[1] = 0;
    lenInfo[2] = 0;
    wchar_t* unicodeMsg = charToWideChar(msg);
    wchar_t* freePtr = unicodeMsg;
    while (*unicodeMsg)
    {
        charDataPosPtr = getPngPosByCharCode(*unicodeMsg);
        charInfoPtr = (uint8_t *)charDataPosPtr;
        charDataPosPtr++;
        retWid -= *charInfoPtr; // x - paddingLeft

        if (*unicodeMsg < 256)
        {
            retWid += *(charInfoPtr + 1); // x + charWidth
        }
        else
        {
            retWid += *(charInfoPtr + 1) + 1; // x + charWidth
        }
        if (retWid > maxPixelLen)
        {
            lenInfo[2] = 1;
            return;
        }

        lenInfo[0] += 1;
        lenInfo[1] = retWid;
        lenInfo[2] = 0;
        unicodeMsg++;
    }

    free(freePtr);
}

void wiiFont::setFontBufByMsgW(uint16_t* rguiFramebuf, wchar_t* unicodeMsg, size_t pitch, int x, int y, uint16_t color)
{
    int isHoverColor = color == 0x7FFF ? 0 : 1;
    unsigned i, j;
    uint16_t * charDataPosPtr;
    uint8_t * charInfoPtr;
    int tmpBufPos;
    wchar_t* tmpMsgPtr = unicodeMsg;
    while (*tmpMsgPtr)
    {
        charDataPosPtr = getPngPosByCharCode(*tmpMsgPtr);
        charInfoPtr = (uint8_t *)charDataPosPtr;
        charDataPosPtr++;
        x -= *charInfoPtr; // x - paddingLeft
        for (j = 0; j < FONT_HEIGHT; j++)
        {
            tmpBufPos = (y + j) * (pitch >> 1) + x;
            for (i = 0; i < FONT_WIDTH; i += 1)
            {
                if (*charDataPosPtr > 0)
                {
                    if (isHoverColor)
                    {
                        rguiFramebuf[tmpBufPos + i] = colorMap[*charDataPosPtr];
                    }
                    else
                    {
                        rguiFramebuf[tmpBufPos + i] = *charDataPosPtr;
                    }
                }
                charDataPosPtr++;
            }
        }

        if (*tmpMsgPtr < 256)
        {
            x += *(charInfoPtr + 1); // x + charWidth
        }
        else
        {
            x += *(charInfoPtr + 1) + 1; // x + charWidth
        }
        tmpMsgPtr++;
    }
}

#define Convert3To8(value) (uint8_t)((value << 5) | (value << 2) | (value >> 1))
#define Convert4To8(value) (uint8_t)((value << 4) | value)
#define Convert5To8(value) (uint8_t)((value << 3) | (value >> 2))

GXColor wiiFont::changeColor(uint16_t color)
{
    GXColor w = {
      .r = 0xff,
      .g = 0xff,
      .b = 0xff,
      .a = 0xff
   };

   if (color & 0x8000 == 0x8000)
   {
       w.r = Convert5To8((color >> 10) & 0x1F);
       w.g = Convert5To8((color >> 5) & 0x1F);
       w.b = Convert5To8(color & 0x1F);
   }
   else
   {
       w.a = Convert3To8((color >> 12) & 0x7);
       w.r = Convert4To8((color >> 8) & 0xF);
       w.g = Convert4To8((color >> 4) & 0xF);
       w.b = Convert4To8(color & 0xF);
   }

   return w;
}

void wiiFont::setTextMsg(const char* msg, int x, int y, int width, int height, bool double_width, bool double_strike)
{
    wchar_t* unicodeMsg = charToWideChar(msg);
    unsigned i, j, h;
    uint16_t * charDataPosPtr;
    uint8_t * charInfoPtr;
    int charWidth;
    wchar_t* tmpMsgPtr = unicodeMsg;
    const GXColor b = {
      .r = 0x00,
      .g = 0x00,
      .b = 0x00,
      .a = 0xff
    };
    GXColor tmpColor;
    while (*tmpMsgPtr)
    {
        charDataPosPtr = getPngPosByCharCode(*tmpMsgPtr);
        charInfoPtr = (uint8_t *)charDataPosPtr;
        charDataPosPtr++;
        x -= *charInfoPtr; // x - paddingLeft
        for (j = 0; j < FONT_HEIGHT; j++)
        {
            for (i = 0; i < FONT_WIDTH; i++)
            {
                tmpColor = *charDataPosPtr == 0 ? b : changeColor(*charDataPosPtr);
                if (!double_strike)
                {
                   GX_PokeARGB(x + (i * width),     y + (j * 2),     tmpColor);
                   if (double_width)
                   {
                      GX_PokeARGB(x + (i * width) + 1, y + (j * 2),     tmpColor);
                      GX_PokeARGB(x + (i * width) + 1, y + (j * 2) + 1, tmpColor);
                   }
                   GX_PokeARGB(x + (i * width),     y + (j * 2) + 1, tmpColor);
                }
                else
                {
                   GX_PokeARGB(x + (i * width),     y + j, tmpColor);
                   if (double_width)
                   {
                      GX_PokeARGB(x + (i * width) + 1, y + j, tmpColor);
                   }
                }

                //GX_PokeARGB(x + i, y + j, *charDataPosPtr == 0 ? b : changeColor(*charDataPosPtr));
                charDataPosPtr++;
            }
        }

        if (*tmpMsgPtr < 256)
        {
            charWidth = *(charInfoPtr + 1);
        }
        else
        {
            charWidth = *(charInfoPtr + 1) + 1;
        }

        for (h = 0; h < height; h++)
        {
            GX_PokeARGB(x + charWidth * width, y + h, b);
            if (double_width)
             {
                GX_PokeARGB(x + (charWidth * width) + 1, y + h, b);
             }
        }

        x += charWidth * (double_width ? 2 : 1); // x + charWidth
        tmpMsgPtr++;
    }

    free(unicodeMsg);
}

void wiiFont::setFontBufByMsg(uint16_t* rguiFramebuf, const char* msg, size_t pitch, int x, int y, uint16_t color)
{
    wchar_t* unicodeMsg = charToWideChar(msg);
    setFontBufByMsgW(rguiFramebuf, unicodeMsg, pitch, x, y, color);

    free(unicodeMsg);
}

extern "C" {
    int wiiFont_getMaxLen(const char* msg, int maxPixelLen)
    {
        return wiiFont::getInstance().getMaxLen(msg, maxPixelLen);
    }

    void wiiFont_getMsgMaxLen(int lenInfo[3], const char* msg, int maxPixelLen)
    {
        wiiFont::getInstance().getMsgMaxLen(lenInfo, msg, maxPixelLen);
    }

    void wiiFont_setFontBufByMsg(uint16_t* rguiFramebuf, const char* msg, size_t pitch, int x, int y, uint16_t color)
    {
        wiiFont::getInstance().setFontBufByMsg(rguiFramebuf, msg, pitch, x, y, color);
    }

    void wiiFont_setTextMsg(const char* msg, int x, int y, int width, int height, bool double_width, bool double_strike)
    {
        wiiFont::getInstance().setTextMsg(msg, x, y, width, height, double_width, double_strike);
    }
}
