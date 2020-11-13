/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/gb/renderers/software.h>

#include <mgba/core/cache-set.h>
#include <mgba/internal/gb/io.h>
#include <mgba/internal/gb/renderers/cache-set.h>
#include <mgba-util/math.h>
#include <mgba-util/memory.h>

static void GBVideoSoftwareRendererInit(struct GBVideoRenderer* renderer, enum GBModel model, bool borders);
static void GBVideoSoftwareRendererDeinit(struct GBVideoRenderer* renderer);
static uint8_t GBVideoSoftwareRendererWriteVideoRegister(struct GBVideoRenderer* renderer, uint16_t address, uint8_t value);
static void GBVideoSoftwareRendererWriteSGBPacket(struct GBVideoRenderer* renderer, uint8_t* data);
static void GBVideoSoftwareRendererWritePalette(struct GBVideoRenderer* renderer, int index, uint16_t value);
static void GBVideoSoftwareRendererWriteVRAM(struct GBVideoRenderer* renderer, uint16_t address);
static void GBVideoSoftwareRendererWriteOAM(struct GBVideoRenderer* renderer, uint16_t oam);
static void GBVideoSoftwareRendererDrawRange(struct GBVideoRenderer* renderer, int startX, int endX, int y, struct GBObj* obj, size_t oamMax);
static void GBVideoSoftwareRendererFinishScanline(struct GBVideoRenderer* renderer, int y);
static void GBVideoSoftwareRendererFinishFrame(struct GBVideoRenderer* renderer);
static void GBVideoSoftwareRendererEnableSGBBorder(struct GBVideoRenderer* renderer, bool enable);
static void GBVideoSoftwareRendererGetPixels(struct GBVideoRenderer* renderer, size_t* stride, const void** pixels);
static void GBVideoSoftwareRendererPutPixels(struct GBVideoRenderer* renderer, size_t stride, const void* pixels);

static void GBVideoSoftwareRendererDrawBackground(struct GBVideoSoftwareRenderer* renderer, uint8_t* maps, int startX, int endX, int sx, int sy);
static void GBVideoSoftwareRendererDrawObj(struct GBVideoSoftwareRenderer* renderer, struct GBObj* obj, int startX, int endX, int y);

static void _clearScreen(struct GBVideoSoftwareRenderer* renderer) {
	size_t sgbOffset = 0;
	if (renderer->model & GB_MODEL_SGB) {
		return;
	}
	int y;
	for (y = 0; y < GB_VIDEO_VERTICAL_PIXELS; ++y) {
		color_t* row = &renderer->outputBuffer[renderer->outputBufferStride * y + sgbOffset];
		int x;
		for (x = 0; x < GB_VIDEO_HORIZONTAL_PIXELS; x += 4) {
			row[x + 0] = renderer->palette[0];
			row[x + 1] = renderer->palette[0];
			row[x + 2] = renderer->palette[0];
			row[x + 3] = renderer->palette[0];
		}
	}
}

static void _regenerateSGBBorder(struct GBVideoSoftwareRenderer* renderer) {
	int i;
	for (i = 0; i < 0x40; ++i) {
		uint16_t color;
		LOAD_16LE(color, 0x800 + i * 2, renderer->d.sgbMapRam);
		renderer->d.writePalette(&renderer->d, i + 0x40, color);
	}
	int x, y;
	for (y = 0; y < 224; ++y) {
		for (x = 0; x < 256; x += 8) {
			if (x >= 48 && x < 208 && y >= 40 && y < 184) {
				continue;
			}
			uint16_t mapData;
			LOAD_16LE(mapData, (x >> 2) + (y & ~7) * 8, renderer->d.sgbMapRam);
			if (UNLIKELY(SGBBgAttributesGetTile(mapData) >= 0x100)) {
				continue;
			}

			int localY = y & 0x7;
			if (SGBBgAttributesIsYFlip(mapData)) {
				localY = 7 - localY;
			}
			uint8_t tileData[4];
			tileData[0] = renderer->d.sgbCharRam[(SGBBgAttributesGetTile(mapData) * 16 + localY) * 2 + 0x00];
			tileData[1] = renderer->d.sgbCharRam[(SGBBgAttributesGetTile(mapData) * 16 + localY) * 2 + 0x01];
			tileData[2] = renderer->d.sgbCharRam[(SGBBgAttributesGetTile(mapData) * 16 + localY) * 2 + 0x10];
			tileData[3] = renderer->d.sgbCharRam[(SGBBgAttributesGetTile(mapData) * 16 + localY) * 2 + 0x11];

			size_t base = y * renderer->outputBufferStride + x;
			int paletteBase = SGBBgAttributesGetPalette(mapData) * 0x10;
			int colorSelector;

			int flip = 0;
			if (SGBBgAttributesIsXFlip(mapData)) {
				flip = 7;
			}
			for (i = 7; i >= 0; --i) {
				colorSelector = (tileData[0] >> i & 0x1) << 0 | (tileData[1] >> i & 0x1) << 1 | (tileData[2] >> i & 0x1) << 2 | (tileData[3] >> i & 0x1) << 3;
				renderer->outputBuffer[(base + 7 - i) ^ flip] = renderer->palette[paletteBase | colorSelector];
			}
		}
	}
}

static inline void _setAttribute(uint8_t* sgbAttributes, unsigned x, unsigned y, int palette) {
	int p = sgbAttributes[(x >> 2) + 5 * y];
	p &= ~(3 << (2 * (3 - (x & 3))));
	p |= palette << (2 * (3 - (x & 3)));
	sgbAttributes[(x >> 2) + 5 * y] = p;
}

static void _parseAttrBlock(struct GBVideoSoftwareRenderer* renderer, int start) {
	uint8_t block[6];
	memcpy(block, &renderer->sgbPacket[start], 6);
	unsigned x0 = block[2];
	unsigned x1 = block[4];
	unsigned y0 = block[3];
	unsigned y1 = block[5];
	unsigned x, y;
	int pIn = block[1] & 3;
	int pPerim = (block[1] >> 2) & 3;
	int pOut = (block[1] >> 4) & 3;

	for (y = 0; y < GB_VIDEO_VERTICAL_PIXELS / 8; ++y) {
		for (x = 0; x < GB_VIDEO_HORIZONTAL_PIXELS / 8; ++x) {
			if (y > y0 && y < y1 && x > x0 && x < x1) {
				if (block[0] & 1) {
					_setAttribute(renderer->d.sgbAttributes, x, y, pIn);
				}
			} else if (y < y0 || y > y1 || x < x0 || x > x1) {
				if (block[0] & 4) {
					_setAttribute(renderer->d.sgbAttributes, x, y, pOut);
				}
			} else {
				if (block[0] & 2) {
					_setAttribute(renderer->d.sgbAttributes, x, y, pPerim);
				} else if (block[0] & 1) {
					_setAttribute(renderer->d.sgbAttributes, x, y, pIn);
				} else if (block[0] & 4) {
					_setAttribute(renderer->d.sgbAttributes, x, y, pOut);
				}
			}
		}
	}
}

static void _parseAttrLine(struct GBVideoSoftwareRenderer* renderer, int start) {
	uint8_t byte = renderer->sgbPacket[start];
	unsigned line = byte & 0x1F;
	int pal = (byte >> 5) & 3;

	if (byte & 0x80) {
		if (line > GB_VIDEO_VERTICAL_PIXELS / 8) {
			return;
		}
		int x;
		for (x = 0; x < GB_VIDEO_HORIZONTAL_PIXELS / 8; ++x) {
			_setAttribute(renderer->d.sgbAttributes, x, line, pal);
		}
	} else {
		if (line > GB_VIDEO_HORIZONTAL_PIXELS / 8) {
			return;
		}
		int y;
		for (y = 0; y < GB_VIDEO_VERTICAL_PIXELS / 8; ++y) {
			_setAttribute(renderer->d.sgbAttributes, line, y, pal);
		}
	}
}

static bool _inWindow(struct GBVideoSoftwareRenderer* renderer) {
	return GBRegisterLCDCIsWindow(renderer->lcdc) && GB_VIDEO_HORIZONTAL_PIXELS + 7 > renderer->wx;
}

void GBVideoSoftwareRendererCreate(struct GBVideoSoftwareRenderer* renderer) {
	renderer->d.init = GBVideoSoftwareRendererInit;
	renderer->d.deinit = GBVideoSoftwareRendererDeinit;
	renderer->d.writeVideoRegister = GBVideoSoftwareRendererWriteVideoRegister;
	renderer->d.writeSGBPacket = GBVideoSoftwareRendererWriteSGBPacket;
	renderer->d.writePalette = GBVideoSoftwareRendererWritePalette;
	renderer->d.writeVRAM = GBVideoSoftwareRendererWriteVRAM;
	renderer->d.writeOAM = GBVideoSoftwareRendererWriteOAM;
	renderer->d.drawRange = GBVideoSoftwareRendererDrawRange;
	renderer->d.finishScanline = GBVideoSoftwareRendererFinishScanline;
	renderer->d.finishFrame = GBVideoSoftwareRendererFinishFrame;
	renderer->d.enableSGBBorder = GBVideoSoftwareRendererEnableSGBBorder;
	renderer->d.getPixels = GBVideoSoftwareRendererGetPixels;
	renderer->d.putPixels = GBVideoSoftwareRendererPutPixels;

	renderer->d.disableBG = false;
	renderer->d.disableOBJ = false;
	renderer->d.disableWIN = false;

	renderer->temporaryBuffer = 0;
}

static void GBVideoSoftwareRendererInit(struct GBVideoRenderer* renderer, enum GBModel model, bool sgbBorders) {
	struct GBVideoSoftwareRenderer* softwareRenderer = (struct GBVideoSoftwareRenderer*) renderer;
	softwareRenderer->lcdc = 0;
	softwareRenderer->scy = 0;
	softwareRenderer->scx = 0;
	softwareRenderer->wy = 0;
	softwareRenderer->currentWy = 0;
	softwareRenderer->currentWx = 0;
	softwareRenderer->lastY = GB_VIDEO_VERTICAL_PIXELS;
	softwareRenderer->lastX = 0;
	softwareRenderer->hasWindow = false;
	softwareRenderer->wx = 0;
	softwareRenderer->model = model;
	softwareRenderer->sgbTransfer = 0;
	softwareRenderer->sgbCommandHeader = 0;
	softwareRenderer->sgbBorders = sgbBorders;
	softwareRenderer->objOffsetX = 0;
	softwareRenderer->objOffsetY = 0;
	softwareRenderer->offsetScx = 0;
	softwareRenderer->offsetScy = 0;
	softwareRenderer->offsetWx = 0;
	softwareRenderer->offsetWy = 0;

	int i;
	for (i = 0; i < 64; ++i) {
		softwareRenderer->lookup[i] = i;
		softwareRenderer->lookup[i] = i;
		softwareRenderer->lookup[i] = i;
		softwareRenderer->lookup[i] = i;
	}

	memset(softwareRenderer->palette, 0, sizeof(softwareRenderer->palette));
}

static void GBVideoSoftwareRendererDeinit(struct GBVideoRenderer* renderer) {
	struct GBVideoSoftwareRenderer* softwareRenderer = (struct GBVideoSoftwareRenderer*) renderer;
	UNUSED(softwareRenderer);
}

static void GBVideoSoftwareRendererUpdateWindow(struct GBVideoSoftwareRenderer* renderer, bool before, bool after, uint8_t oldWy) {
	if (renderer->lastY >= GB_VIDEO_VERTICAL_PIXELS || !(after || before)) {
		return;
	}
	if (!renderer->hasWindow && renderer->lastX == GB_VIDEO_HORIZONTAL_PIXELS) {
		return;
	}
	if (renderer->lastY >= oldWy) {
		if (!after) {
			renderer->currentWy -= renderer->lastY;
			renderer->hasWindow = true;
		} else if (!before) {
			if (!renderer->hasWindow) {
				renderer->currentWy = renderer->lastY - renderer->wy;
				if (renderer->lastY >= renderer->wy && renderer->lastX > renderer->wx) {
					++renderer->currentWy;
				}
			} else {
				renderer->currentWy += renderer->lastY;
			}
		} else if (renderer->wy != oldWy) {
			renderer->currentWy += oldWy - renderer->wy;
			renderer->hasWindow = true;
		}
	}
}

static uint8_t GBVideoSoftwareRendererWriteVideoRegister(struct GBVideoRenderer* renderer, uint16_t address, uint8_t value) {
	struct GBVideoSoftwareRenderer* softwareRenderer = (struct GBVideoSoftwareRenderer*) renderer;
	if (renderer->cache) {
		GBVideoCacheWriteVideoRegister(renderer->cache, address, value);
	}
	bool wasWindow = _inWindow(softwareRenderer);
	uint8_t wy = softwareRenderer->wy;
	switch (address) {
	case REG_LCDC:
		softwareRenderer->lcdc = value;
		GBVideoSoftwareRendererUpdateWindow(softwareRenderer, wasWindow, _inWindow(softwareRenderer), wy);
		break;
	case REG_SCY:
		softwareRenderer->scy = value;
		break;
	case REG_SCX:
		softwareRenderer->scx = value;
		break;
	case REG_WY:
		softwareRenderer->wy = value;
		GBVideoSoftwareRendererUpdateWindow(softwareRenderer, wasWindow, _inWindow(softwareRenderer), wy);
		break;
	case REG_WX:
		softwareRenderer->wx = value;
		GBVideoSoftwareRendererUpdateWindow(softwareRenderer, wasWindow, _inWindow(softwareRenderer), wy);
		break;
	case REG_BGP:
		softwareRenderer->lookup[0] = value & 3;
		softwareRenderer->lookup[1] = (value >> 2) & 3;
		softwareRenderer->lookup[2] = (value >> 4) & 3;
		softwareRenderer->lookup[3] = (value >> 6) & 3;
		break;
	case REG_OBP0:
		softwareRenderer->lookup[0x20 + 0] = value & 3;
		softwareRenderer->lookup[0x20 + 1] = (value >> 2) & 3;
		softwareRenderer->lookup[0x20 + 2] = (value >> 4) & 3;
		softwareRenderer->lookup[0x20 + 3] = (value >> 6) & 3;
		break;
	case REG_OBP1:
		softwareRenderer->lookup[0x24 + 0] = value & 3;
		softwareRenderer->lookup[0x24 + 1] = (value >> 2) & 3;
		softwareRenderer->lookup[0x24 + 2] = (value >> 4) & 3;
		softwareRenderer->lookup[0x24 + 3] = (value >> 6) & 3;
		break;
	}
	return value;
}

static void GBVideoSoftwareRendererWriteSGBPacket(struct GBVideoRenderer* renderer, uint8_t* data) {
	struct GBVideoSoftwareRenderer* softwareRenderer = (struct GBVideoSoftwareRenderer*) renderer;
	memcpy(softwareRenderer->sgbPacket, data, sizeof(softwareRenderer->sgbPacket));
	int i;
	softwareRenderer->sgbCommandHeader = data[0];
	softwareRenderer->sgbTransfer = 0;
	int set;
	int sets;
	int attrX;
	int attrY;
	int attrDirection;
	int pBefore;
	int pAfter;
	int pDiv;
	switch (softwareRenderer->sgbCommandHeader >> 3) {
	case SGB_PAL_SET:
		softwareRenderer->sgbPacket[1] = data[9];
		if (!(data[9] & 0x80)) {
			break;
		}
		// Fall through
	case SGB_ATTR_SET:
		set = softwareRenderer->sgbPacket[1] & 0x3F;
		if (set <= 0x2C) {
			memcpy(renderer->sgbAttributes, &renderer->sgbAttributeFiles[set * 90], 90);
		}
		break;
	case SGB_ATTR_BLK:
		sets = softwareRenderer->sgbPacket[1];
		i = 2;
		for (; i < (softwareRenderer->sgbCommandHeader & 7) << 4 && sets; i += 6, --sets) {
			_parseAttrBlock(softwareRenderer, i);
		}
		break;
	case SGB_ATTR_LIN:
		sets = softwareRenderer->sgbPacket[1];
		i = 2;
		for (; i < (softwareRenderer->sgbCommandHeader & 7) << 4 && sets; ++i, --sets) {
			_parseAttrLine(softwareRenderer, i);
		}
		break;
	case SGB_ATTR_DIV:
		pAfter = softwareRenderer->sgbPacket[1] & 3;
		pBefore = (softwareRenderer->sgbPacket[1] >> 2) & 3;
		pDiv = (softwareRenderer->sgbPacket[1] >> 4) & 3;
		attrX = softwareRenderer->sgbPacket[2];
		if (softwareRenderer->sgbPacket[1] & 0x40) {
			if (attrX > GB_VIDEO_VERTICAL_PIXELS / 8) {
				attrX = GB_VIDEO_VERTICAL_PIXELS / 8;
			}
			int j;
			for (j = 0; j < attrX; ++j) {
				for (i = 0; i < GB_VIDEO_HORIZONTAL_PIXELS / 8; ++i) {
					_setAttribute(renderer->sgbAttributes, i, j, pBefore);
				}
			}
			if (attrX < GB_VIDEO_VERTICAL_PIXELS / 8) {
				for (i = 0; i < GB_VIDEO_HORIZONTAL_PIXELS / 8; ++i) {
					_setAttribute(renderer->sgbAttributes, i, attrX, pDiv);
				}

			}
			for (; j < GB_VIDEO_VERTICAL_PIXELS / 8; ++j) {
				for (i = 0; i < GB_VIDEO_HORIZONTAL_PIXELS / 8; ++i) {
					_setAttribute(renderer->sgbAttributes, i, j, pAfter);
				}
			}
		} else {
			if (attrX > GB_VIDEO_HORIZONTAL_PIXELS / 8) {
				attrX = GB_VIDEO_HORIZONTAL_PIXELS / 8;
			}
			int j;
			for (j = 0; j < attrX; ++j) {
				for (i = 0; i < GB_VIDEO_HORIZONTAL_PIXELS / 8; ++i) {
					_setAttribute(renderer->sgbAttributes, j, i, pBefore);
				}
			}
			if (attrX < GB_VIDEO_HORIZONTAL_PIXELS / 8) {
				for (i = 0; i < GB_VIDEO_VERTICAL_PIXELS / 8; ++i) {
					_setAttribute(renderer->sgbAttributes, attrX, i, pDiv);
				}

			}
			for (; j < GB_VIDEO_HORIZONTAL_PIXELS / 8; ++j) {
				for (i = 0; i < GB_VIDEO_VERTICAL_PIXELS / 8; ++i) {
					_setAttribute(renderer->sgbAttributes, j, i, pAfter);
				}
			}
		}
		break;
	case SGB_ATTR_CHR:
		attrX = softwareRenderer->sgbPacket[1];
		attrY = softwareRenderer->sgbPacket[2];
		if (attrX >= GB_VIDEO_HORIZONTAL_PIXELS / 8) {
			attrX = 0;
		}
		if (attrY >= GB_VIDEO_VERTICAL_PIXELS / 8) {
			attrY = 0;
		}
		sets = softwareRenderer->sgbPacket[3];
		sets |= softwareRenderer->sgbPacket[4] << 8;
		attrDirection = softwareRenderer->sgbPacket[5];
		i = 6;
		for (; i < (softwareRenderer->sgbCommandHeader & 7) << 4 && sets; ++i) {
			int j;
			for (j = 0; j < 4 && sets; ++j, --sets) {
				uint8_t p = softwareRenderer->sgbPacket[i] >> (6 - j * 2);
				_setAttribute(renderer->sgbAttributes, attrX, attrY, p & 3);
				if (attrDirection) {
					++attrY;
					if (attrY >= GB_VIDEO_VERTICAL_PIXELS / 8) {
						attrY = 0;
						++attrX;
					}
					if (attrX >= GB_VIDEO_HORIZONTAL_PIXELS / 8) {
						attrX = 0;
					}
				} else {
					++attrX;
					if (attrX >= GB_VIDEO_HORIZONTAL_PIXELS / 8) {
						attrX = 0;
						++attrY;
					}
					if (attrY >= GB_VIDEO_VERTICAL_PIXELS / 8) {
						attrY = 0;
					}
				}
			}
		}

		break;
	case SGB_ATRC_EN:
	case SGB_MASK_EN:
		if (softwareRenderer->sgbBorders && !renderer->sgbRenderMode) {
			_regenerateSGBBorder(softwareRenderer);
		}
	}
}

static void GBVideoSoftwareRendererWritePalette(struct GBVideoRenderer* renderer, int index, uint16_t value) {
	struct GBVideoSoftwareRenderer* softwareRenderer = (struct GBVideoSoftwareRenderer*) renderer;
	color_t color = mColorFrom555(value);
	if (softwareRenderer->model & GB_MODEL_SGB) {
		if (index < 0x10 && index && !(index & 3)) {
			color = softwareRenderer->palette[0];
		} else if (index >= 0x40 && !(index & 0xF)) {
			color = softwareRenderer->palette[0];
		}
	}
	if (renderer->cache) {
		mCacheSetWritePalette(renderer->cache, index, color);
	}
	if (softwareRenderer->model == GB_MODEL_AGB) {
		unsigned r = M_R5(value);
		unsigned g = M_G5(value);
		unsigned b = M_B5(value);
		r = r * r;
		g = g * g;
		b = b * b;
#ifdef COLOR_16_BIT
		r /= 31;
		g /= 31;
		b /= 31;
		color = mColorFrom555(r | (g << 5) | (b << 10));
#else
		r >>= 2;
		r += r >> 4;
		g >>= 2;
		g += g >> 4;
		b >>= 2;
		b += b >> 4;
		color = r | (g << 8) | (b << 16);
#endif
	}
	softwareRenderer->palette[index] = color;

	if (softwareRenderer->model & GB_MODEL_SGB && !index && GBRegisterLCDCIsEnable(softwareRenderer->lcdc)) {
		renderer->writePalette(renderer, 0x04, value);
		renderer->writePalette(renderer, 0x08, value);
		renderer->writePalette(renderer, 0x0C, value);
		renderer->writePalette(renderer, 0x40, value);
		renderer->writePalette(renderer, 0x50, value);
		renderer->writePalette(renderer, 0x60, value);
		renderer->writePalette(renderer, 0x70, value);
		if (softwareRenderer->sgbBorders && !renderer->sgbRenderMode) {
			_regenerateSGBBorder(softwareRenderer);
		}
	}
}

static void GBVideoSoftwareRendererWriteVRAM(struct GBVideoRenderer* renderer, uint16_t address) {
	if (renderer->cache) {
		mCacheSetWriteVRAM(renderer->cache, address);
	}
}

static void GBVideoSoftwareRendererWriteOAM(struct GBVideoRenderer* renderer, uint16_t oam) {
	UNUSED(renderer);
	UNUSED(oam);
	// Nothing to do
}

static void GBVideoSoftwareRendererDrawRange(struct GBVideoRenderer* renderer, int startX, int endX, int y, struct GBObj* obj, size_t oamMax) {
	struct GBVideoSoftwareRenderer* softwareRenderer = (struct GBVideoSoftwareRenderer*) renderer;
	softwareRenderer->lastY = y;
	softwareRenderer->lastX = endX;
	uint8_t* maps = &softwareRenderer->d.vram[GB_BASE_MAP];
	if (GBRegisterLCDCIsTileMap(softwareRenderer->lcdc)) {
		maps += GB_SIZE_MAP;
	}
	if (softwareRenderer->d.disableBG) {
		memset(&softwareRenderer->row[startX], 0, endX - startX);
	}
	if (GBRegisterLCDCIsBgEnable(softwareRenderer->lcdc) || softwareRenderer->model >= GB_MODEL_CGB) {
		int wy = softwareRenderer->wy + softwareRenderer->currentWy;
		int wx = softwareRenderer->wx + softwareRenderer->currentWx - 7;
		if (GBRegisterLCDCIsWindow(softwareRenderer->lcdc) && wy == y && wx <= endX) {
			softwareRenderer->hasWindow = true;
		}
		if (GBRegisterLCDCIsWindow(softwareRenderer->lcdc) && softwareRenderer->hasWindow && wx <= endX) {
			if (wx > 0 && !softwareRenderer->d.disableBG) {
				GBVideoSoftwareRendererDrawBackground(softwareRenderer, maps, startX, wx, softwareRenderer->scx - softwareRenderer->offsetScx, softwareRenderer->scy + y - softwareRenderer->offsetScy);
			}

			maps = &softwareRenderer->d.vram[GB_BASE_MAP];
			if (GBRegisterLCDCIsWindowTileMap(softwareRenderer->lcdc)) {
				maps += GB_SIZE_MAP;
			}
			if (!softwareRenderer->d.disableWIN) {
				GBVideoSoftwareRendererDrawBackground(softwareRenderer, maps, wx, endX, -wx - softwareRenderer->offsetWx, y - wy - softwareRenderer->offsetWy);
			}
		} else if (!softwareRenderer->d.disableBG) {
			GBVideoSoftwareRendererDrawBackground(softwareRenderer, maps, startX, endX, softwareRenderer->scx - softwareRenderer->offsetScx, softwareRenderer->scy + y - softwareRenderer->offsetScy);
		}
	} else if (!softwareRenderer->d.disableBG) {
		memset(&softwareRenderer->row[startX], 0, endX - startX);
	}

	if (GBRegisterLCDCIsObjEnable(softwareRenderer->lcdc) && !softwareRenderer->d.disableOBJ) {
		size_t i;
		for (i = 0; i < oamMax; ++i) {
			GBVideoSoftwareRendererDrawObj(softwareRenderer, &obj[i], startX, endX, y);
		}
	}

	size_t sgbOffset = 0;
	if (softwareRenderer->model & GB_MODEL_SGB && softwareRenderer->sgbBorders) {
		sgbOffset = softwareRenderer->outputBufferStride * 40 + 48;
	}
	color_t* row = &softwareRenderer->outputBuffer[softwareRenderer->outputBufferStride * y + sgbOffset];
	int x = startX;
	int p = 0;
	switch (softwareRenderer->d.sgbRenderMode) {
	case 0:
		if (softwareRenderer->model & GB_MODEL_SGB) {
			p = softwareRenderer->d.sgbAttributes[(startX >> 5) + 5 * (y >> 3)];
			p >>= 6 - ((x / 4) & 0x6);
			p &= 3;
			p <<= 2;
		}
		for (; x < ((startX + 7) & ~7) && x < endX; ++x) {
			row[x] = softwareRenderer->palette[p | softwareRenderer->lookup[softwareRenderer->row[x] & 0x7F]];
		}
		for (; x + 7 < (endX & ~7); x += 8) {
			if (softwareRenderer->model & GB_MODEL_SGB) {
				p = softwareRenderer->d.sgbAttributes[(x >> 5) + 5 * (y >> 3)];
				p >>= 6 - ((x / 4) & 0x6);
				p &= 3;
				p <<= 2;
			}
			row[x + 0] = softwareRenderer->palette[p | softwareRenderer->lookup[softwareRenderer->row[x] & 0x7F]];
			row[x + 1] = softwareRenderer->palette[p | softwareRenderer->lookup[softwareRenderer->row[x + 1] & 0x7F]];
			row[x + 2] = softwareRenderer->palette[p | softwareRenderer->lookup[softwareRenderer->row[x + 2] & 0x7F]];
			row[x + 3] = softwareRenderer->palette[p | softwareRenderer->lookup[softwareRenderer->row[x + 3] & 0x7F]];
			row[x + 4] = softwareRenderer->palette[p | softwareRenderer->lookup[softwareRenderer->row[x + 4] & 0x7F]];
			row[x + 5] = softwareRenderer->palette[p | softwareRenderer->lookup[softwareRenderer->row[x + 5] & 0x7F]];
			row[x + 6] = softwareRenderer->palette[p | softwareRenderer->lookup[softwareRenderer->row[x + 6] & 0x7F]];
			row[x + 7] = softwareRenderer->palette[p | softwareRenderer->lookup[softwareRenderer->row[x + 7] & 0x7F]];
		}
		if (softwareRenderer->model & GB_MODEL_SGB) {
			p = softwareRenderer->d.sgbAttributes[(x >> 5) + 5 * (y >> 3)];
			p >>= 6 - ((x / 4) & 0x6);
			p &= 3;
			p <<= 2;
		}
		for (; x < endX; ++x) {
			row[x] = softwareRenderer->palette[p | softwareRenderer->lookup[softwareRenderer->row[x] & 0x7F]];
		}
		break;
	case 1:
		break;
	case 2:
		for (; x < ((startX + 7) & ~7) && x < endX; ++x) {
			row[x] = 0;
		}
		for (; x + 7 < (endX & ~7); x += 8) {
			row[x] = 0;
			row[x + 1] = 0;
			row[x + 2] = 0;
			row[x + 3] = 0;
			row[x + 4] = 0;
			row[x + 5] = 0;
			row[x + 6] = 0;
			row[x + 7] = 0;
		}
		for (; x < endX; ++x) {
			row[x] = 0;
		}
		break;
	case 3:
		for (; x < ((startX + 7) & ~7) && x < endX; ++x) {
			row[x] = softwareRenderer->palette[0];
		}
		for (; x + 7 < (endX & ~7); x += 8) {
			row[x] = softwareRenderer->palette[0];
			row[x + 1] = softwareRenderer->palette[0];
			row[x + 2] = softwareRenderer->palette[0];
			row[x + 3] = softwareRenderer->palette[0];
			row[x + 4] = softwareRenderer->palette[0];
			row[x + 5] = softwareRenderer->palette[0];
			row[x + 6] = softwareRenderer->palette[0];
			row[x + 7] = softwareRenderer->palette[0];
		}
		for (; x < endX; ++x) {
			row[x] = softwareRenderer->palette[0];
		}
		break;
	}
}

static void GBVideoSoftwareRendererFinishScanline(struct GBVideoRenderer* renderer, int y) {
	struct GBVideoSoftwareRenderer* softwareRenderer = (struct GBVideoSoftwareRenderer*) renderer;

	softwareRenderer->lastX = 0;
	softwareRenderer->currentWx = 0;

	if (softwareRenderer->sgbTransfer == 1) {
		size_t offset = 2 * ((y & 7) + (y >> 3) * GB_VIDEO_HORIZONTAL_PIXELS);
		if (offset >= 0x1000) {
			return;
		}
		uint8_t* buffer = NULL;
		switch (softwareRenderer->sgbCommandHeader >> 3) {
		case SGB_PAL_TRN:
			buffer = renderer->sgbPalRam;
			break;
		case SGB_CHR_TRN:
			buffer = &renderer->sgbCharRam[SGB_SIZE_CHAR_RAM / 2 * (softwareRenderer->sgbPacket[1] & 1)];
			break;
		case SGB_PCT_TRN:
			buffer = renderer->sgbMapRam;
			break;
		case SGB_ATTR_TRN:
			buffer = renderer->sgbAttributeFiles;
			break;
		default:
			break;
		}
		if (buffer) {
			int i;
			for (i = 0; i < GB_VIDEO_HORIZONTAL_PIXELS; i += 8) {
				if (UNLIKELY(offset + (i << 1) + 1 >= 0x1000)) {
					break;
				}
				uint8_t hi = 0;
				uint8_t lo = 0;
				hi |= (softwareRenderer->row[i + 0] & 0x2) << 6;
				lo |= (softwareRenderer->row[i + 0] & 0x1) << 7;
				hi |= (softwareRenderer->row[i + 1] & 0x2) << 5;
				lo |= (softwareRenderer->row[i + 1] & 0x1) << 6;
				hi |= (softwareRenderer->row[i + 2] & 0x2) << 4;
				lo |= (softwareRenderer->row[i + 2] & 0x1) << 5;
				hi |= (softwareRenderer->row[i + 3] & 0x2) << 3;
				lo |= (softwareRenderer->row[i + 3] & 0x1) << 4;
				hi |= (softwareRenderer->row[i + 4] & 0x2) << 2;
				lo |= (softwareRenderer->row[i + 4] & 0x1) << 3;
				hi |= (softwareRenderer->row[i + 5] & 0x2) << 1;
				lo |= (softwareRenderer->row[i + 5] & 0x1) << 2;
				hi |= (softwareRenderer->row[i + 6] & 0x2) << 0;
				lo |= (softwareRenderer->row[i + 6] & 0x1) << 1;
				hi |= (softwareRenderer->row[i + 7] & 0x2) >> 1;
				lo |= (softwareRenderer->row[i + 7] & 0x1) >> 0;
				buffer[offset + (i << 1) + 0] = lo;
				buffer[offset + (i << 1) + 1] = hi;
			}
		}
	}
}

static void GBVideoSoftwareRendererFinishFrame(struct GBVideoRenderer* renderer) {
	struct GBVideoSoftwareRenderer* softwareRenderer = (struct GBVideoSoftwareRenderer*) renderer;

	if (softwareRenderer->temporaryBuffer) {
		mappedMemoryFree(softwareRenderer->temporaryBuffer, GB_VIDEO_HORIZONTAL_PIXELS * GB_VIDEO_VERTICAL_PIXELS * 4);
		softwareRenderer->temporaryBuffer = 0;
	}
	if (!GBRegisterLCDCIsEnable(softwareRenderer->lcdc)) {
		_clearScreen(softwareRenderer);
	}
	if (softwareRenderer->model & GB_MODEL_SGB) {
		switch (softwareRenderer->sgbCommandHeader >> 3) {
		case SGB_PAL_SET:
		case SGB_ATTR_SET:
			if (softwareRenderer->sgbPacket[1] & 0x40) {
				renderer->sgbRenderMode = 0;
			}
			break;
		case SGB_PAL_TRN:
		case SGB_CHR_TRN:
		case SGB_PCT_TRN:
		case SGB_ATRC_EN:
		case SGB_MASK_EN:
			if (softwareRenderer->sgbBorders && !renderer->sgbRenderMode) {
				// Make sure every buffer sees this if we're multibuffering
				_regenerateSGBBorder(softwareRenderer);
			}
			// Fall through
		case SGB_ATTR_TRN:
			++softwareRenderer->sgbTransfer;
			if (softwareRenderer->sgbTransfer == 5) {
				softwareRenderer->sgbCommandHeader = 0;
			}
			break;
		default:
			break;
		}
	}
	softwareRenderer->lastY = GB_VIDEO_VERTICAL_PIXELS;
	softwareRenderer->lastX = 0;
	softwareRenderer->currentWy = 0;
	softwareRenderer->currentWx = 0;
	softwareRenderer->hasWindow = false;
}

static void GBVideoSoftwareRendererEnableSGBBorder(struct GBVideoRenderer* renderer, bool enable) {
	struct GBVideoSoftwareRenderer* softwareRenderer = (struct GBVideoSoftwareRenderer*) renderer;
	if (softwareRenderer->model & GB_MODEL_SGB) {
		if (enable == softwareRenderer->sgbBorders) {
			return;
		}
		softwareRenderer->sgbBorders = enable;
		if (softwareRenderer->sgbBorders && !renderer->sgbRenderMode) {
			_regenerateSGBBorder(softwareRenderer);
		}
	}
}

static void GBVideoSoftwareRendererDrawBackground(struct GBVideoSoftwareRenderer* renderer, uint8_t* maps, int startX, int endX, int sx, int sy) {
	uint8_t* data = renderer->d.vram;
	uint8_t* attr = &maps[GB_SIZE_VRAM_BANK0];
	if (!GBRegisterLCDCIsTileData(renderer->lcdc)) {
		data += 0x1000;
	}
	int topY = ((sy >> 3) & 0x1F) * 0x20;
	int bottomY = sy & 7;
	if (startX < 0) {
		startX = 0;
	}
	int x;
	if ((startX + sx) & 7) {
		int startX2 = startX + 8 - ((startX + sx) & 7);
		for (x = startX; x < startX2; ++x) {
			uint8_t* localData = data;
			int localY = bottomY;
			int topX = ((x + sx) >> 3) & 0x1F;
			int bottomX = 7 - ((x + sx) & 7);
			int bgTile;
			if (GBRegisterLCDCIsTileData(renderer->lcdc)) {
				bgTile = maps[topX + topY];
			} else {
				bgTile = ((int8_t*) maps)[topX + topY];
			}
			int p = 0;
			if (renderer->model >= GB_MODEL_CGB) {
				GBObjAttributes attrs = attr[topX + topY];
				p = GBObjAttributesGetCGBPalette(attrs) * 4;
				if (GBObjAttributesIsPriority(attrs) && GBRegisterLCDCIsBgEnable(renderer->lcdc)) {
					p |= 0x80;
				}
				if (GBObjAttributesIsBank(attrs)) {
					localData += GB_SIZE_VRAM_BANK0;
				}
				if (GBObjAttributesIsYFlip(attrs)) {
					localY = 7 - bottomY;
				}
				if (GBObjAttributesIsXFlip(attrs)) {
					bottomX = 7 - bottomX;
				}
			}
			uint8_t tileDataLower = localData[(bgTile * 8 + localY) * 2];
			uint8_t tileDataUpper = localData[(bgTile * 8 + localY) * 2 + 1];
			tileDataUpper >>= bottomX;
			tileDataLower >>= bottomX;
			renderer->row[x] = p | ((tileDataUpper & 1) << 1) | (tileDataLower & 1);
		}
		startX = startX2;
	}
	for (x = startX; x < endX; x += 8) {
		uint8_t* localData = data;
		int localY = bottomY;
		int topX = ((x + sx) >> 3) & 0x1F;
		int bgTile;
		if (GBRegisterLCDCIsTileData(renderer->lcdc)) {
			bgTile = maps[topX + topY];
		} else {
			bgTile = ((int8_t*) maps)[topX + topY];
		}
		int p = 0;
		if (renderer->model >= GB_MODEL_CGB) {
			GBObjAttributes attrs = attr[topX + topY];
			p = GBObjAttributesGetCGBPalette(attrs) * 4;
			if (GBObjAttributesIsPriority(attrs) && GBRegisterLCDCIsBgEnable(renderer->lcdc)) {
				p |= 0x80;
			}
			if (GBObjAttributesIsBank(attrs)) {
				localData += GB_SIZE_VRAM_BANK0;
			}
			if (GBObjAttributesIsYFlip(attrs)) {
				localY = 7 - bottomY;
			}
			if (GBObjAttributesIsXFlip(attrs)) {
				uint8_t tileDataLower = localData[(bgTile * 8 + localY) * 2];
				uint8_t tileDataUpper = localData[(bgTile * 8 + localY) * 2 + 1];
				renderer->row[x + 0] = p | ((tileDataUpper & 1) << 1) | (tileDataLower & 1);
				renderer->row[x + 1] = p | (tileDataUpper & 2) | ((tileDataLower & 2) >> 1);
				renderer->row[x + 2] = p | ((tileDataUpper & 4) >> 1) | ((tileDataLower & 4) >> 2);
				renderer->row[x + 3] = p | ((tileDataUpper & 8) >> 2) | ((tileDataLower & 8) >> 3);
				renderer->row[x + 4] = p | ((tileDataUpper & 16) >> 3) | ((tileDataLower & 16) >> 4);
				renderer->row[x + 5] = p | ((tileDataUpper & 32) >> 4) | ((tileDataLower & 32) >> 5);
				renderer->row[x + 6] = p | ((tileDataUpper & 64) >> 5) | ((tileDataLower & 64) >> 6);
				renderer->row[x + 7] = p | ((tileDataUpper & 128) >> 6) | ((tileDataLower & 128) >> 7);
				continue;
			}
		}
		uint8_t tileDataLower = localData[(bgTile * 8 + localY) * 2];
		uint8_t tileDataUpper = localData[(bgTile * 8 + localY) * 2 + 1];
		renderer->row[x + 7] = p | ((tileDataUpper & 1) << 1) | (tileDataLower & 1);
		renderer->row[x + 6] = p | (tileDataUpper & 2) | ((tileDataLower & 2) >> 1);
		renderer->row[x + 5] = p | ((tileDataUpper & 4) >> 1) | ((tileDataLower & 4) >> 2);
		renderer->row[x + 4] = p | ((tileDataUpper & 8) >> 2) | ((tileDataLower & 8) >> 3);
		renderer->row[x + 3] = p | ((tileDataUpper & 16) >> 3) | ((tileDataLower & 16) >> 4);
		renderer->row[x + 2] = p | ((tileDataUpper & 32) >> 4) | ((tileDataLower & 32) >> 5);
		renderer->row[x + 1] = p | ((tileDataUpper & 64) >> 5) | ((tileDataLower & 64) >> 6);
		renderer->row[x + 0] = p | ((tileDataUpper & 128) >> 6) | ((tileDataLower & 128) >> 7);
	}
}

static void GBVideoSoftwareRendererDrawObj(struct GBVideoSoftwareRenderer* renderer, struct GBObj* obj, int startX, int endX, int y) {
	int objX = obj->x + renderer->objOffsetX;
	int ix = objX - 8;
	if (endX < ix || startX >= ix + 8) {
		return;
	}
	if (objX < endX) {
		endX = objX;
	}
	if (objX - 8 > startX) {
		startX = objX - 8;
	}
	if (startX < 0) {
		startX = 0;
	}
	uint8_t* data = renderer->d.vram;
	int tileOffset = 0;
	int bottomY;
	int objY = obj->y + renderer->objOffsetY;
	if (GBObjAttributesIsYFlip(obj->attr)) {
		bottomY = 7 - ((y - objY - 16) & 7);
		if (GBRegisterLCDCIsObjSize(renderer->lcdc) && y - objY < -8) {
			++tileOffset;
		}
	} else {
		bottomY = (y - objY - 16) & 7;
		if (GBRegisterLCDCIsObjSize(renderer->lcdc) && y - objY >= -8) {
			++tileOffset;
		}
	}
	if (GBRegisterLCDCIsObjSize(renderer->lcdc) && obj->tile & 1) {
		--tileOffset;
	}
	uint8_t mask = GBObjAttributesIsPriority(obj->attr) ? 0x63 : 0x60;
	uint8_t mask2 = GBObjAttributesIsPriority(obj->attr) ? 0 : 0x83;
	int p;
	if (renderer->model >= GB_MODEL_CGB) {
		p = (GBObjAttributesGetCGBPalette(obj->attr) + 8) * 4;
		if (GBObjAttributesIsBank(obj->attr)) {
			data += GB_SIZE_VRAM_BANK0;
		}
		if (!GBRegisterLCDCIsBgEnable(renderer->lcdc)) {
			mask = 0x60;
			mask2 = 0x83;
		}
	} else {
		p = (GBObjAttributesGetPalette(obj->attr) + 8) * 4;
	}
	int bottomX;
	int x = startX;
	if ((x - objX) & 7) {
		for (; x < endX; ++x) {
			if (GBObjAttributesIsXFlip(obj->attr)) {
				bottomX = (x - objX) & 7;
			} else {
				bottomX = 7 - ((x - objX) & 7);
			}
			int objTile = obj->tile + tileOffset;
			uint8_t tileDataLower = data[(objTile * 8 + bottomY) * 2];
			uint8_t tileDataUpper = data[(objTile * 8 + bottomY) * 2 + 1];
			tileDataUpper >>= bottomX;
			tileDataLower >>= bottomX;
			color_t current = renderer->row[x];
			if (((tileDataUpper | tileDataLower) & 1) && !(current & mask) && (current & mask2) <= 0x80) {
				renderer->row[x] = p | ((tileDataUpper & 1) << 1) | (tileDataLower & 1);
			}
		}
	} else if (GBObjAttributesIsXFlip(obj->attr)) {
		int objTile = obj->tile + tileOffset;
		uint8_t tileDataLower = data[(objTile * 8 + bottomY) * 2];
		uint8_t tileDataUpper = data[(objTile * 8 + bottomY) * 2 + 1];
		color_t current;
		current = renderer->row[x];
		if (((tileDataUpper | tileDataLower) & 1) && !(current & mask) && (current & mask2) <= 0x80) {
			renderer->row[x] = p | ((tileDataUpper & 1) << 1) | (tileDataLower & 1);
		}
		current = renderer->row[x + 1];
		if (((tileDataUpper | tileDataLower) & 2) && !(current & mask) && (current & mask2) <= 0x80) {
			renderer->row[x + 1] = p | (tileDataUpper & 2) | ((tileDataLower & 2) >> 1);
		}
		current = renderer->row[x + 2];
		if (((tileDataUpper | tileDataLower) & 4) && !(current & mask) && (current & mask2) <= 0x80) {
			renderer->row[x + 2] = p | ((tileDataUpper & 4) >> 1) | ((tileDataLower & 4) >> 2);
		}
		current = renderer->row[x + 3];
		if (((tileDataUpper | tileDataLower) & 8) && !(current & mask) && (current & mask2) <= 0x80) {
			renderer->row[x + 3] = p | ((tileDataUpper & 8) >> 2) | ((tileDataLower & 8) >> 3);
		}
		current = renderer->row[x + 4];
		if (((tileDataUpper | tileDataLower) & 16) && !(current & mask) && (current & mask2) <= 0x80) {
			renderer->row[x + 4] = p | ((tileDataUpper & 16) >> 3) | ((tileDataLower & 16) >> 4);
		}
		current = renderer->row[x + 5];
		if (((tileDataUpper | tileDataLower) & 32) && !(current & mask) && (current & mask2) <= 0x80) {
			renderer->row[x + 5] = p | ((tileDataUpper & 32) >> 4) | ((tileDataLower & 32) >> 5);
		}
		current = renderer->row[x + 6];
		if (((tileDataUpper | tileDataLower) & 64) && !(current & mask) && (current & mask2) <= 0x80) {
			renderer->row[x + 6] = p | ((tileDataUpper & 64) >> 5) | ((tileDataLower & 64) >> 6);
		}
		current = renderer->row[x + 7];
		if (((tileDataUpper | tileDataLower) & 128) && !(current & mask) && (current & mask2) <= 0x80) {
			renderer->row[x + 7] = p | ((tileDataUpper & 128) >> 6) | ((tileDataLower & 128) >> 7);
		}
	} else {
		int objTile = obj->tile + tileOffset;
		uint8_t tileDataLower = data[(objTile * 8 + bottomY) * 2];
		uint8_t tileDataUpper = data[(objTile * 8 + bottomY) * 2 + 1];
		color_t current;
		current = renderer->row[x + 7];
		if (((tileDataUpper | tileDataLower) & 1) && !(current & mask) && (current & mask2) <= 0x80) {
			renderer->row[x + 7] = p | ((tileDataUpper & 1) << 1) | (tileDataLower & 1);
		}
		current = renderer->row[x + 6];
		if (((tileDataUpper | tileDataLower) & 2) && !(current & mask) && (current & mask2) <= 0x80) {
			renderer->row[x + 6] = p | (tileDataUpper & 2) | ((tileDataLower & 2) >> 1);
		}
		current = renderer->row[x + 5];
		if (((tileDataUpper | tileDataLower) & 4) && !(current & mask) && (current & mask2) <= 0x80) {
			renderer->row[x + 5] = p | ((tileDataUpper & 4) >> 1) | ((tileDataLower & 4) >> 2);
		}
		current = renderer->row[x + 4];
		if (((tileDataUpper | tileDataLower) & 8) && !(current & mask) && (current & mask2) <= 0x80) {
			renderer->row[x + 4] = p | ((tileDataUpper & 8) >> 2) | ((tileDataLower & 8) >> 3);
		}
		current = renderer->row[x + 3];
		if (((tileDataUpper | tileDataLower) & 16) && !(current & mask) && (current & mask2) <= 0x80) {
			renderer->row[x + 3] = p | ((tileDataUpper & 16) >> 3) | ((tileDataLower & 16) >> 4);
		}
		current = renderer->row[x + 2];
		if (((tileDataUpper | tileDataLower) & 32) && !(current & mask) && (current & mask2) <= 0x80) {
			renderer->row[x + 2] = p | ((tileDataUpper & 32) >> 4) | ((tileDataLower & 32) >> 5);
		}
		current = renderer->row[x + 1];
		if (((tileDataUpper | tileDataLower) & 64) && !(current & mask) && (current & mask2) <= 0x80) {
			renderer->row[x + 1] = p | ((tileDataUpper & 64) >> 5) | ((tileDataLower & 64) >> 6);
		}
		current = renderer->row[x];
		if (((tileDataUpper | tileDataLower) & 128) && !(current & mask) && (current & mask2) <= 0x80) {
			renderer->row[x] = p | ((tileDataUpper & 128) >> 6) | ((tileDataLower & 128) >> 7);
		}
	}
}

static void GBVideoSoftwareRendererGetPixels(struct GBVideoRenderer* renderer, size_t* stride, const void** pixels) {
	struct GBVideoSoftwareRenderer* softwareRenderer = (struct GBVideoSoftwareRenderer*) renderer;
	*stride = softwareRenderer->outputBufferStride;
	*pixels = softwareRenderer->outputBuffer;
}

static void GBVideoSoftwareRendererPutPixels(struct GBVideoRenderer* renderer, size_t stride, const void* pixels) {
	struct GBVideoSoftwareRenderer* softwareRenderer = (struct GBVideoSoftwareRenderer*) renderer;
	// TODO: Share with GBAVideoSoftwareRendererGetPixels

	const color_t* colorPixels = pixels;
	unsigned i;
	for (i = 0; i < GB_VIDEO_VERTICAL_PIXELS; ++i) {
		memmove(&softwareRenderer->outputBuffer[softwareRenderer->outputBufferStride * i], &colorPixels[stride * i], GB_VIDEO_HORIZONTAL_PIXELS * BYTES_PER_PIXEL);
	}
}
