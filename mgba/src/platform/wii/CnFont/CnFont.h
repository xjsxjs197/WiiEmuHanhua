/****************************************************************************
 * Snes9x Nintendo Wii/Gamecube Port
 *
 * Tantric 2009-2010
 *
 * filelist.h
 *
 * Contains a list of all of the files stored in the images/, fonts/, and
 * sounds/ folders
 ***************************************************************************/

#ifndef _CNFONT_H_
#define _CNFONT_H_

extern "C" void InitCnFont();
extern "C" void DestroyCnFont();
extern "C" void DrawCnChar(int x, int y, uint32_t color, uint16_t glyph);
extern "C" int GetCharHeight(uint16_t glyph);
extern "C" int GetCharWidth(uint16_t glyph);

#endif
