#include <map>

#include <gccore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <ogcsys.h>
#include <unistd.h>
#include <fat.h>
#include <malloc.h>

#include "mGba_CnFont_I4_dat.h"
#include "CnFont.h"

#define CH_FONT_HEIGHT 32
#define CH_FONT_WIDTH 32

// 字符图片的大小
#define CHAR_IMG_SIZE CH_FONT_HEIGHT * CH_FONT_WIDTH / 2

// 保存字符图片映射信息
static std::map<uint16_t, int> charWidthMap;
static std::map<uint16_t, u8*> charImgBufMap;

/**
* 初始化
*/
void InitCnFont() {
	int searchLen = (int)(mGba_CnFont_I4_dat_size / (CHAR_IMG_SIZE + 4));
	int bufIndex = 0;
	int skipSetp = (CHAR_IMG_SIZE + 4) / 2;

	uint16_t *zhFontBufTemp = (uint16_t *)mGba_CnFont_I4_dat;
    while (bufIndex < searchLen)
    {
        charWidthMap.insert(std::pair<uint16_t, int>(*zhFontBufTemp, *((u8*)(zhFontBufTemp + 1) + 1)));
        u8 * tmpPngBuf = (u8*) memalign(32, CHAR_IMG_SIZE);
        memcpy(tmpPngBuf, (u8*)(zhFontBufTemp + 2), CHAR_IMG_SIZE);
        charImgBufMap.insert(std::pair<uint16_t, u8*>(*zhFontBufTemp, tmpPngBuf));

        zhFontBufTemp += skipSetp;
        bufIndex++;
    }
}

/**
* 释放资源
*/
void DestroyCnFont() {
    charWidthMap.clear();
    if (charImgBufMap.size() > 0)
    {
         std::map<uint16_t, u8*>::iterator it = charImgBufMap.begin();
         while (it != charImgBufMap.end())
         {
             u8* tmpBuf = it->second;
             free(tmpBuf);
             ++it;
         }

         charImgBufMap.clear();
    }
}

/**
* 开始画字符
*/
void DrawCnChar(int x, int y, uint32_t color, uint16_t glyph) {
	color = (color >> 24) | (color << 8);
	GXTexObj fontTexObj;

	GX_InvalidateTexAll();

	GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_NOOP);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);

    std::map<uint16_t, u8*>::iterator iter;
    u8* charPngBug;
    iter = charImgBufMap.find(glyph);
    if (iter != charImgBufMap.end())
    {
        charPngBug = iter->second;
    }
    else
    {
        charPngBug = (u8*)(charImgBufMap[0]);
    }


	GX_InitTexObj(&fontTexObj, charPngBug, CH_FONT_WIDTH, CH_FONT_HEIGHT, GX_TF_I4, GX_CLAMP, GX_CLAMP, GX_FALSE);
    GX_LoadTexObj(&fontTexObj, GX_TEXMAP0);

    GX_Begin(GX_QUADS, GX_VTXFMT0, 4);

    GX_Position2s16(x, y);
    GX_Color1u32(color);
    GX_TexCoord2f32(0.0f, 0.0f);

    GX_Position2s16(CH_FONT_WIDTH + x, y);
    GX_Color1u32(color);
    GX_TexCoord2f32(1.0f, 0.0f);

    GX_Position2s16(CH_FONT_WIDTH + x, CH_FONT_HEIGHT + y);
    GX_Color1u32(color);
    GX_TexCoord2f32(1.0f, 1.0f);

    GX_Position2s16(x, CH_FONT_HEIGHT + y);
    GX_Color1u32(color);
    GX_TexCoord2f32(0.0f, 1.0f);

    GX_End();
}

/**
* 取得字符高度
*/
int GetCharHeight(uint16_t glyph) {
    return CH_FONT_HEIGHT;
}

/**
* 取得字符宽度
*/
int GetCharWidth(uint16_t glyph) {
    std::map<uint16_t, int>::iterator iter;
    int charPngWidth;
    iter = charWidthMap.find(glyph);
    if (iter != charWidthMap.end())
    {
        charPngWidth = iter->second;
    }
    else
    {
        charPngWidth = charWidthMap[0];
    }

    return charWidthMap[glyph];
}
