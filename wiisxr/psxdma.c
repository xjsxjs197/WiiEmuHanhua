/***************************************************************************
 *   Copyright (C) 2007 Ryan Schultz, PCSX-df Team, PCSX team              *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.           *
 ***************************************************************************/

/*
* Handles PSX DMA functions.
*/
#include "Gamecube/DEBUG.h"
#include "psxdma.h"

// Dma0/1 in mdec.c
// Dma3   in cdrom.c

void psxDma4(u32 madr, u32 bcr, u32 chcr) { // SPU

	u16 *ptr;
	u32 size;

	switch (chcr) {
		case 0x01000201: //cpu to spu transfer
#ifdef DEBUG_DMA
      sprintf(txtbuffer,"*** DMA4 SPU - mem2spu *** %08x addr = %08x size = %08x\n", chcr, madr, bcr);
      DEBUG_print(txtbuffer, 5);
#endif
			ptr = (u16 *)PSXM(madr);
			if (ptr == NULL) {
#ifdef DEBUG_DMA
        DEBUG_print("*** DMA4 SPU - mem2spu *** NULL Pointer!!!\n", 5);
#endif
				break;
			}
			SPU_writeDMAMem(ptr, (bcr >> 16) * (bcr & 0xffff) * 2);
			break;

		case 0x01000200: //spu to cpu transfer
#ifdef DEBUG_DMA
      sprintf(txtbuffer,"*** DMA4 SPU - spu2mem *** %08x addr = %08x size = %08x\n", chcr, madr, bcr);
      DEBUG_print(txtbuffer, 6);
#endif
			ptr = (u16 *)PSXM(madr);
			if (ptr == NULL) {
#ifdef DEBUG_DMA
        DEBUG_print("*** DMA4 SPU - spu2mem *** NULL Pointer!!!\n", 6);
#endif
				break;
			}
			size = (bcr >> 16) * (bcr & 0xffff) * 2;
    		SPU_readDMAMem(ptr, size);
			psxCpu->Clear(madr, size);
			break;

#ifdef DEBUG_DMA
		default:
  		sprintf(txtbuffer,"*** DMA4 SPU - unknown *** %08x addr = %08x size = %08x\n", chcr, madr, bcr);
      DEBUG_print(txtbuffer, 7);
			break;
#endif
	}

	// upd xjsxjs197 start
	HW_DMA4_CHCR &= SWAPu32(~0x01000000);
	/*tmpVal = (u32)(~0x01000000);
	STORE_SWAP32p(tmpAddr, tmpVal);
	HW_DMA4_CHCR &= tmpAddr[0];*/
	// upd xjsxjs197 end
	DMA_INTERRUPT(4);

}

void psxDma2(u32 madr, u32 bcr, u32 chcr) { // GPU
#ifdef PROFILE
  start_section(GFX_SECTION);
#endif
	u32 *ptr;
	u32 size;

	switch(chcr) {
		case 0x01000200: // vram2mem
#ifdef DEBUG_DMA
      sprintf(txtbuffer,"*** DMA2 GPU - vram2mem *** %08x addr = %08x size = %08x\n", chcr, madr, bcr);
      DEBUG_print(txtbuffer, 8);
#endif
			ptr = (u32 *)PSXM(madr);
			if (ptr == NULL) {
#ifdef DEBUG_DMA
				DEBUG_print("*** DMA2 GPU - vram2mem *** NULL Pointer!!!\n", 8);
#endif
				break;
			}
			size = (bcr >> 16) * (bcr & 0xffff);
			GPU_readDataMem((unsigned long*)ptr, size);
			psxCpu->Clear(madr, size);
			break;

		case 0x01000201: // mem2vram
#ifdef DEBUG_DMA
      sprintf(txtbuffer,"*** DMA 2 - GPU mem2vram *** %08x addr = %08x size = %08x\n", chcr, madr, bcr);
      DEBUG_print(txtbuffer, 9);
#endif
			ptr = (u32 *)PSXM(madr);
			if (ptr == NULL) {
#ifdef DEBUG_DMA
        DEBUG_print("*** DMA2 GPU - mem2vram *** NULL Pointer!!!\n", 9);
#endif
				break;
			}
			size = (bcr >> 16) * (bcr & 0xffff);
			GPU_writeDataMem((unsigned long*)ptr, size);
			// upd xjsxjs197 start
			//GPUDMA_INT((size / 4) / BIAS);
			GPUDMA_INT((size >> 3));
			// upd xjsxjs197 end
#ifdef PROFILE
	end_section(GFX_SECTION);
#endif
			return;
//			break;

		case 0x01000401: // dma chain
#ifdef DEBUG_DMA
      sprintf(txtbuffer,"*** DMA 2 - GPU dma chain *** %08x addr = %08x size = %08x\n", chcr, madr, bcr);
      DEBUG_print(txtbuffer, 10);
#endif
			GPU_dmaChain((u32 *)psxM, madr & 0x1fffff);
			break;

#ifdef DEBUG_DMA
		default:
		  sprintf(txtbuffer,"*** DMA 2 - GPU unknown *** %08x addr = %08x size = %08x\n", chcr, madr, bcr);
      DEBUG_print(txtbuffer, 11);
			break;
#endif
	}

	// upd xjsxjs197 start
	HW_DMA2_CHCR &= SWAPu32(~0x01000000);
	/*tmpVal = (u32)(~0x01000000);
	STORE_SWAP32p(tmpAddr, tmpVal);
	HW_DMA2_CHCR &= tmpAddr[0];*/
	// upd xjsxjs197 end
	DMA_INTERRUPT(2);
#ifdef PROFILE
	end_section(GFX_SECTION);
#endif
}

void gpuInterrupt() {
	// upd xjsxjs197 start
	HW_DMA2_CHCR &= SWAPu32(~0x01000000);
	/*tmpVal = (u32)(~0x01000000);
	STORE_SWAP32p(tmpAddr, tmpVal);
	HW_DMA2_CHCR &= tmpAddr[0];*/
	// upd xjsxjs197 end
	DMA_INTERRUPT(2);
}

void psxDma6(u32 madr, u32 bcr, u32 chcr) {
	u32 *mem = (u32 *)PSXM(madr);

#ifdef DEBUG_DMA
	sprintf(txtbuffer,"*** DMA6 OT *** %08x addr = %08x size = %08x\n", chcr, madr, bcr);
  DEBUG_print(txtbuffer, 12);
#endif

	if (chcr == 0x11000002) {
		if (mem == NULL) {
#ifdef DEBUG_DMA
      DEBUG_print("*** DMA6 OT *** NULL Pointer!!!\n", 12);
#endif
			// upd xjsxjs197 start
            HW_DMA6_CHCR &= SWAPu32(~0x01000000);
            /*tmpVal = (u32)(~0x01000000);
            STORE_SWAP32p(tmpAddr, tmpVal);
            HW_DMA6_CHCR &= tmpAddr[0];*/
            // upd xjsxjs197 end
			DMA_INTERRUPT(6);
			return;
		}

		while (bcr--) {
            // upd xjsxjs197 start
			//*mem-- = SWAPu32((madr - 4) & 0xffffff);
			STORE_SWAP32p(tmpAddr, (madr - 4) & 0xffffff);
			*mem-- = tmpAddr[0];
			//mem--;
			//STORE_SWAP32p(mem, (u32)((madr - 4) & 0xffffff));
            // upd xjsxjs197 end
			madr -= 4;
		}
		// upd xjsxjs197 start
		//mem++; *mem = SWAPu32(0xffffff);
		mem++;
		tmpVal = (u32)(0xffffff);
		STORE_SWAP32p(mem, tmpVal);
		// upd xjsxjs197 end
	}
#ifdef DEBUG_DMA
	else {
		// Unknown option
		sprintf(txtbuffer,"*** DMA6 OT - unknown *** %08x addr = %08x size = %08x\n", chcr, madr, bcr);
    DEBUG_print(txtbuffer, 13);
	}
#endif

    // upd xjsxjs197 start
	HW_DMA6_CHCR &= SWAPu32(~0x01000000);
	/*tmpVal = (u32)(~0x01000000);
	STORE_SWAP32p(tmpAddr, tmpVal);
	HW_DMA6_CHCR &= tmpAddr[0];*/
	// upd xjsxjs197 end
	DMA_INTERRUPT(6);
}

