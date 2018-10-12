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
#include <exception>
#include <ogc/gx.h>

#include "wiiFont.h"
#include "wiiFontC.h"
#include "gettext.h"
#include "../gfx/drivers_font_renderer/bitmap.h"
extern "C" {
#include "../memory/wii/mem2_manager.h"
}

#define CHAR_IMG_SIZE 338
static std::map<wchar_t, int> charCodeMap;
static std::map<uint16_t, uint16_t> colorMap;
static uint8_t *ZhBufFont_dat;
static char* zhMapBuf;

/**
* 初始化
*/
wiiFont::wiiFont()
{
    wiiFontInit();
}

/**
* 释放资源
*/
wiiFont::~wiiFont()
{
	wiiFontClose();
}

/**
* 初始化
*/
void wiiFont::wiiFontInit()
{
    int ZhBufFont_size = 2384766; // RGB5A3
	int searchLen = (int)(ZhBufFont_size / (4 + CHAR_IMG_SIZE));
	int bufIndex = 0;
	int skipSetp = (CHAR_IMG_SIZE + 4) / 2;

    // 读取中文字库信息到Mem2
    FILE *charPngFile;
    charPngFile = fopen("sd:/apps/retroarchCnFont/ZhBufFont13X13NoBlock_RGB5A3.dat", "rb");
	ZhBufFont_dat = (uint8_t *)_mem2_memalign(32, ZhBufFont_size);

	fseek(charPngFile, 0, SEEK_SET);
	fread(ZhBufFont_dat, 1, ZhBufFont_size, charPngFile);
    fclose(charPngFile);
    charPngFile = NULL;

    // 将字库信息放入Map方便查找
	uint16_t *zhFontBufTemp = (uint16_t *)ZhBufFont_dat;
    while (bufIndex < searchLen)
    {
        charCodeMap.insert(std::pair<wchar_t, int>(*zhFontBufTemp, bufIndex * skipSetp));
        zhFontBufTemp += skipSetp;
        bufIndex++;
    }

    // 设置白色字和绿色字的像素映射，方便显示绿字
    colorMap.insert(std::pair<uint16_t, uint16_t>(0, 0));
    colorMap.insert(std::pair<uint16_t, uint16_t>(32777, 32768));
    colorMap.insert(std::pair<uint16_t, uint16_t>(53151, 33216));
    colorMap.insert(std::pair<uint16_t, uint16_t>(56768, 32992));
    colorMap.insert(std::pair<uint16_t, uint16_t>(47868, 33152));
    colorMap.insert(std::pair<uint16_t, uint16_t>(52512, 32896));
    colorMap.insert(std::pair<uint16_t, uint16_t>(32782, 32768));
    colorMap.insert(std::pair<uint16_t, uint16_t>(57343, 33280));
    colorMap.insert(std::pair<uint16_t, uint16_t>(53148, 33216));
    colorMap.insert(std::pair<uint16_t, uint16_t>(52855, 33056));
    colorMap.insert(std::pair<uint16_t, uint16_t>(65262, 33152));
    colorMap.insert(std::pair<uint16_t, uint16_t>(62057, 33056));
    colorMap.insert(std::pair<uint16_t, uint16_t>(33239, 32992));
    colorMap.insert(std::pair<uint16_t, uint16_t>(33075, 32896));
    colorMap.insert(std::pair<uint16_t, uint16_t>(62190, 33152));
    colorMap.insert(std::pair<uint16_t, uint16_t>(42615, 33056));
    colorMap.insert(std::pair<uint16_t, uint16_t>(65535, 33280));
    colorMap.insert(std::pair<uint16_t, uint16_t>(57235, 33216));
    colorMap.insert(std::pair<uint16_t, uint16_t>(41993, 32768));
    colorMap.insert(std::pair<uint16_t, uint16_t>(53143, 33216));
    colorMap.insert(std::pair<uint16_t, uint16_t>(47104, 32768));
    colorMap.insert(std::pair<uint16_t, uint16_t>(47871, 33152));
    colorMap.insert(std::pair<uint16_t, uint16_t>(42620, 33056));
    colorMap.insert(std::pair<uint16_t, uint16_t>(47863, 33152));
    colorMap.insert(std::pair<uint16_t, uint16_t>(57070, 33152));
    colorMap.insert(std::pair<uint16_t, uint16_t>(62463, 33280));
    colorMap.insert(std::pair<uint16_t, uint16_t>(65427, 33216));
    colorMap.insert(std::pair<uint16_t, uint16_t>(41984, 32768));
    colorMap.insert(std::pair<uint16_t, uint16_t>(57340, 33280));
    colorMap.insert(std::pair<uint16_t, uint16_t>(52526, 32896));
    colorMap.insert(std::pair<uint16_t, uint16_t>(57335, 33280));
    colorMap.insert(std::pair<uint16_t, uint16_t>(47118, 32768));
    colorMap.insert(std::pair<uint16_t, uint16_t>(65527, 33280));
    colorMap.insert(std::pair<uint16_t, uint16_t>(62355, 33216));
    colorMap.insert(std::pair<uint16_t, uint16_t>(47113, 32768));
    colorMap.insert(std::pair<uint16_t, uint16_t>(62455, 33280));
    colorMap.insert(std::pair<uint16_t, uint16_t>(65431, 33216));
    colorMap.insert(std::pair<uint16_t, uint16_t>(57079, 33152));
    colorMap.insert(std::pair<uint16_t, uint16_t>(57247, 33216));
    colorMap.insert(std::pair<uint16_t, uint16_t>(62460, 33280));
    colorMap.insert(std::pair<uint16_t, uint16_t>(62199, 33152));
    colorMap.insert(std::pair<uint16_t, uint16_t>(65271, 33152));
    colorMap.insert(std::pair<uint16_t, uint16_t>(65532, 33280));
    colorMap.insert(std::pair<uint16_t, uint16_t>(52841, 33056));
    colorMap.insert(std::pair<uint16_t, uint16_t>(65267, 33152));
    colorMap.insert(std::pair<uint16_t, uint16_t>(56782, 32992));
    colorMap.insert(std::pair<uint16_t, uint16_t>(52979, 33152));
    colorMap.insert(std::pair<uint16_t, uint16_t>(57084, 33152));
    colorMap.insert(std::pair<uint16_t, uint16_t>(62364, 33216));
    colorMap.insert(std::pair<uint16_t, uint16_t>(62367, 33216));
    colorMap.insert(std::pair<uint16_t, uint16_t>(47575, 32992));
    colorMap.insert(std::pair<uint16_t, uint16_t>(47411, 32896));
    colorMap.insert(std::pair<uint16_t, uint16_t>(56777, 32992));
    colorMap.insert(std::pair<uint16_t, uint16_t>(65436, 33216));
    colorMap.insert(std::pair<uint16_t, uint16_t>(47740, 33056));
    colorMap.insert(std::pair<uint16_t, uint16_t>(52521, 32896));
    colorMap.insert(std::pair<uint16_t, uint16_t>(52983, 33152));
    colorMap.insert(std::pair<uint16_t, uint16_t>(56937, 33056));
    colorMap.insert(std::pair<uint16_t, uint16_t>(57087, 33152));
    colorMap.insert(std::pair<uint16_t, uint16_t>(33230, 32992));
    colorMap.insert(std::pair<uint16_t, uint16_t>(62067, 33056));
    colorMap.insert(std::pair<uint16_t, uint16_t>(52860, 33056));
    colorMap.insert(std::pair<uint16_t, uint16_t>(33235, 32992));
    colorMap.insert(std::pair<uint16_t, uint16_t>(56947, 33056));
    colorMap.insert(std::pair<uint16_t, uint16_t>(42455, 32992));
    colorMap.insert(std::pair<uint16_t, uint16_t>(62359, 33216));
    colorMap.insert(std::pair<uint16_t, uint16_t>(57239, 33216));
    colorMap.insert(std::pair<uint16_t, uint16_t>(62062, 33056));
    colorMap.insert(std::pair<uint16_t, uint16_t>(56942, 33056));
    colorMap.insert(std::pair<uint16_t, uint16_t>(47392, 32896));
    colorMap.insert(std::pair<uint16_t, uint16_t>(57244, 33216));
    colorMap.insert(std::pair<uint16_t, uint16_t>(52691, 32992));
    colorMap.insert(std::pair<uint16_t, uint16_t>(33070, 32896));
    colorMap.insert(std::pair<uint16_t, uint16_t>(52851, 33056));
    colorMap.insert(std::pair<uint16_t, uint16_t>(33065, 32896));
    colorMap.insert(std::pair<uint16_t, uint16_t>(42272, 32896));
    colorMap.insert(std::pair<uint16_t, uint16_t>(52991, 33152));
    colorMap.insert(std::pair<uint16_t, uint16_t>(57075, 33152));
    colorMap.insert(std::pair<uint16_t, uint16_t>(42291, 32896));
    colorMap.insert(std::pair<uint16_t, uint16_t>(62195, 33152));
    colorMap.insert(std::pair<uint16_t, uint16_t>(41998, 32768));
    colorMap.insert(std::pair<uint16_t, uint16_t>(52988, 33152));
    colorMap.insert(std::pair<uint16_t, uint16_t>(42281, 32896));
    colorMap.insert(std::pair<uint16_t, uint16_t>(42446, 32992));
    colorMap.insert(std::pair<uint16_t, uint16_t>(47731, 33056));
    colorMap.insert(std::pair<uint16_t, uint16_t>(52681, 32992));
    colorMap.insert(std::pair<uint16_t, uint16_t>(47566, 32992));
    colorMap.insert(std::pair<uint16_t, uint16_t>(42451, 32992));
    colorMap.insert(std::pair<uint16_t, uint16_t>(52846, 33056));
    colorMap.insert(std::pair<uint16_t, uint16_t>(47561, 32992));
    colorMap.insert(std::pair<uint16_t, uint16_t>(42286, 32896));
    colorMap.insert(std::pair<uint16_t, uint16_t>(42611, 33056));
    colorMap.insert(std::pair<uint16_t, uint16_t>(47735, 33056));
    colorMap.insert(std::pair<uint16_t, uint16_t>(52672, 32992));
    colorMap.insert(std::pair<uint16_t, uint16_t>(47401, 32896));
    colorMap.insert(std::pair<uint16_t, uint16_t>(47552, 32992));
    colorMap.insert(std::pair<uint16_t, uint16_t>(52686, 32992));
    colorMap.insert(std::pair<uint16_t, uint16_t>(47571, 32992));
    colorMap.insert(std::pair<uint16_t, uint16_t>(47406, 32896));
    colorMap.insert(std::pair<uint16_t, uint16_t>(53139, 33216));
    colorMap.insert(std::pair<uint16_t, uint16_t>(47859, 33152));
    colorMap.insert(std::pair<uint16_t, uint16_t>(56951, 33056));
    colorMap.insert(std::pair<uint16_t, uint16_t>(52695, 32992));
    colorMap.insert(std::pair<uint16_t, uint16_t>(56956, 33056));
    colorMap.insert(std::pair<uint16_t, uint16_t>(52531, 32896));
    colorMap.insert(std::pair<uint16_t, uint16_t>(56787, 32992));
    colorMap.insert(std::pair<uint16_t, uint16_t>(52974, 33152));
    colorMap.insert(std::pair<uint16_t, uint16_t>(62071, 33056));
    colorMap.insert(std::pair<uint16_t, uint16_t>(62204, 33152));
    colorMap.insert(std::pair<uint16_t, uint16_t>(56791, 32992));

    // 读取街机游戏文件名映射文件
    FILE *titleMapFile;
    titleMapFile = fopen("sd:/apps/retroarchCnFont/zh.lang", "rb");

    fseek(titleMapFile, 0, SEEK_END);
    long zhMapFileLen = ftell(titleMapFile);
    zhMapBuf = (char*)_mem2_memalign(32, (int)zhMapFileLen);

    fseek(titleMapFile, 0, SEEK_SET);
	fread(zhMapBuf, 1, zhMapFileLen, titleMapFile);
    fclose(titleMapFile);
    titleMapFile = NULL;

    // 游戏文件名映射放入内存
    LoadLanguage(zhMapBuf);
}

/**
* 释放资源
*/
void wiiFont::wiiFontClose()
{
    if (ZhBufFont_dat)
    {
        _mem2_free(ZhBufFont_dat);
        ZhBufFont_dat = NULL;
    }

    if (zhMapBuf)
    {
        _mem2_free(zhMapBuf);
        zhMapBuf = NULL;
    }
}

/**
* 取得当前字符在字库中的位置
* @param unicode Unicode编码格式的当前字符
* @return 当前字符在字库中的位置
*/
uint16_t* wiiFont::getPngPosByCharCode(wchar_t unicode)
{
    return ((uint16_t *)ZhBufFont_dat + charCodeMap[unicode] + 1);
}

/**
* 单字节字符串转换成多字节字符串
* @param strChar 单字节字符串
* @return 多字节字符串
*/
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

/**
* 单字节字符串转换成多字节字符串
* @param strChar 单字节字符串
* @return 多字节字符串
*/
wchar_t* wiiFont::charToWideChar(const char* strChar)
{
	return charToWideChar((char*)strChar);
}

/**
* 取得当前字符串显示时的最大像素长度
* @param msg 单字节字符串
* @param maxPixelLen 最大可以显示的像素长度
* @return 当前字符串显示时的最大像素长度
*/
int wiiFont::getMaxLen(const char* msg, int maxPixelLen)
{
    int maxLen = 0;
    int retWid = 0;
    uint16_t * charDataPosPtr;
    uint8_t * charInfoPtr;
    wchar_t* unicodeMsg = charToWideChar(gettext(msg));
    wchar_t* freePtr = unicodeMsg;
    while (*unicodeMsg)
    {
        charDataPosPtr = getPngPosByCharCode(*unicodeMsg);
        charInfoPtr = (uint8_t *)charDataPosPtr;
        charDataPosPtr++;

        retWid += *(charInfoPtr + 1); // x + charWidth
        if (*unicodeMsg >= 256)
        {
            retWid++;
        }

        if (retWid > maxPixelLen)
        {
            break;
        }

        maxLen++;
        unicodeMsg++;
    }

    free(freePtr);
    return maxLen;
}

/**
* 设置当前字符串显示时的最大长度信息
* @param lenInfo 最大长度信息（0：可以显示的最大文字个数，1：可以显示的最大像素长度，2：是否超过可以显示的最大长度）
* @param msg 单字节字符串
* @param maxPixelLen 最大可以显示的像素长度
*/
void wiiFont::getMsgMaxLen(int lenInfo[3], const char* msg, int maxPixelLen)
{
    int retWid = 0;
    uint16_t * charDataPosPtr;
    uint8_t * charInfoPtr;
    lenInfo[0] = 0;
    lenInfo[1] = 0;
    lenInfo[2] = 0;
    wchar_t* unicodeMsg = charToWideChar(gettext(msg));
    wchar_t* freePtr = unicodeMsg;
    while (*unicodeMsg)
    {
        charDataPosPtr = getPngPosByCharCode(*unicodeMsg);
        charInfoPtr = (uint8_t *)charDataPosPtr;
        charDataPosPtr++;

        retWid += *(charInfoPtr + 1); // x + charWidth
        if (*unicodeMsg >= 256)
        {
            retWid++;
        }

        if (retWid > maxPixelLen)
        {
            lenInfo[2] = 1;
            break;
        }

        lenInfo[0] += 1;
        lenInfo[1] = retWid;
        lenInfo[2] = 0;
        unicodeMsg++;
    }

    free(freePtr);
}

/**
* 将当前显示的字符串信息设置到显示Buf中
* @param rguiFramebuf 显示Buf
* @param unicodeMsg unicode格式的字符串
* @param pitch 屏幕宽度
* @param x 位置x
* @param y 位置y
* @param color 是否是选中的颜色
*/
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
        //x -= *charInfoPtr; // x - paddingLeft
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

        x += *(charInfoPtr + 1); // x + charWidth
        if (*tmpMsgPtr >= 256)
        {
            x++;
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

/**
* 将当前显示的字符串信息设置到显示Buf中
* @param msg 单字节的字符串
* @param x 位置x
* @param y 位置y
* @param width 屏幕宽
* @param height 屏幕高
* @param double_width 是否双倍宽
* @param double_strike 是否是粗体
*/
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
        //x -= *charInfoPtr; // x - paddingLeft
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

                charDataPosPtr++;
            }
        }

        charWidth = *(charInfoPtr + 1);
        if (*tmpMsgPtr >= 256)
        {
            charWidth++;
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

/**
* 将当前显示的字符串信息设置到显示Buf中
* @param rguiFramebuf 显示Buf
* @param msg 单字节字符串
* @param pitch 屏幕宽度
* @param x 位置x
* @param y 位置y
* @param color 是否是选中的颜色
*/
void wiiFont::setFontBufByMsg(uint16_t* rguiFramebuf, const char* msg, size_t pitch, int x, int y, uint16_t color)
{
    wchar_t* unicodeMsg = charToWideChar(msg);
    setFontBufByMsgW(rguiFramebuf, unicodeMsg, pitch, x, y, color);

    free(unicodeMsg);
}

/**
* 根据当前字符串，取得相应的中文字符串
* @param title 当前字符串
* @return 相应的中文字符串
*/
char* wiiFont::getChTitle(char* title)
{
    return (char*)gettext(title);
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

    char* wiiFont_getChTitle(char* title)
    {
        wiiFont::getInstance().getChTitle(title);
    }
}
