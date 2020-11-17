#include "franspu.h"
#include "../plugins.h"

extern void *cacheable_kernel_memcpy(void *to, const void *from, size_t len);

// READ DMA (one value)
unsigned short  FRAN_SPU_readDMA(void)
{
    // upd xjsxjs197 start
 	//unsigned short s=LE2HOST16(spuMem[spuAddr>>1]);
	unsigned short s = LOAD_SWAP16p(spuMem + (spuAddr >> 1));
	// upd xjsxjs197 end
 	spuAddr+=2;
 	// upd xjsxjs197 start
 	//if(spuAddr>=0x80000) spuAddr=0;
 	spuAddr &= 0x7fffe;
 	// upd xjsxjs197 end
 	return s;
}

// READ DMA (many values)
void  FRAN_SPU_readDMAMem(unsigned short * pusPSXMem,int iSize)
{
	// upd xjsxjs197 start
	/*if (spuAddr+(iSize<<1)>=0x80000)
 	{
 		memcpy(pusPSXMem,&spuMem[spuAddr>>1],0x7ffff-spuAddr+1);
		memcpy(pusPSXMem+(0x7ffff-spuAddr+1),spuMem,(iSize<<1)-(0x7ffff-spuAddr+1));
		spuAddr=(iSize<<1)-(0x7ffff-spuAddr+1);
	} else {
		memcpy(pusPSXMem,&spuMem[spuAddr>>1],iSize<<1);
		spuAddr+=(iSize<<1);
	}*/

	iSize <<= 1;
	if (spuAddr + iSize >= 0x80000)
 	{
 		int tmpAddr = 0x80000 - spuAddr;
 		cacheable_kernel_memcpy(pusPSXMem, spuMem + (spuAddr >> 1), tmpAddr);
 		spuAddr = iSize - tmpAddr;
		cacheable_kernel_memcpy(pusPSXMem + tmpAddr, spuMem, spuAddr);
		#ifdef DISP_DEBUG
		char debug[256];
		sprintf(debug, "FRAN_SPU_readDMAMem==Over====%d", spuAddr);
		GPU_displayText(debug);
		#endif
	} else {
		cacheable_kernel_memcpy(pusPSXMem, spuMem + (spuAddr >> 1), iSize);
		spuAddr += iSize;
	}
	// upd xjsxjs197 end
}

// WRITE DMA (one value)
void  FRAN_SPU_writeDMA(unsigned short val)
{
    // upd xjsxjs197 start
 	//spuMem[spuAddr>>1] = HOST2LE16(val);
 	STORE_SWAP16p(spuMem + (spuAddr >> 1), val);
 	// upd xjsxjs197 end
 	spuAddr+=2;
 	// upd xjsxjs197 start
 	//if(spuAddr>=0x80000) spuAddr=0;
 	spuAddr &= 0x7fffe;
 	// upd xjsxjs197 end
}

//GPUdisplayText  GPU_displayText2;
// WRITE DMA (many values)
void  FRAN_SPU_writeDMAMem(unsigned short * pusPSXMem,int iSize)
{
	// upd xjsxjs197 start
	/*if (spuAddr+(iSize<<1)>0x7ffff)
	{
 		memcpy(&spuMem[spuAddr>>1],pusPSXMem,0x7ffff-spuAddr+1);
		memcpy(spuMem,pusPSXMem+(0x7ffff-spuAddr+1),(iSize<<1)-(0x7ffff-spuAddr+1));
		spuAddr=(iSize<<1)-(0x7ffff-spuAddr+1);
  	} else {
  		memcpy(&spuMem[spuAddr>>1],pusPSXMem,iSize<<1);
  		spuAddr+=(iSize<<1);
  	}*/

	iSize <<= 1;
	if (spuAddr + iSize > 0x7ffff)
	{
		int tmpAddr = 0x80000 - spuAddr;
 		cacheable_kernel_memcpy(spuMem + (spuAddr >> 1), pusPSXMem, tmpAddr);
		spuAddr = iSize - tmpAddr;
		if (spuAddr > 0)
        {
            cacheable_kernel_memcpy(spuMem + 0x1010, pusPSXMem + tmpAddr, spuAddr);
            #ifdef DISP_DEBUG
            char debug[256];
            sprintf(debug, "SPU_writeDMAMem==Over====%d", spuAddr);
            GPU_displayText(debug);
            #endif
            spuAddr += 0x1010;
        }
  	} else {
  		cacheable_kernel_memcpy(spuMem + (spuAddr >> 1), pusPSXMem, iSize);
  		spuAddr += iSize;
  	}
	// upd xjsxjs197 end
}
