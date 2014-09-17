#ifndef __DUMMY_DRV_INTF_H__
#define __DUMMY_DRV_INTF_H__
#include <stdio.h>
#include <string.h>
#include <iprt/rand.h>
#include <iprt/thread.h>

#include "DrvIntf.h"
#include "Display.h"

class DummyDrvIntf: public DrvIntf {
public:
    DummyDrvIntf() { pPixels = NULL; pixelsLen = 0;};
    virtual ~DummyDrvIntf(); 

    virtual int Init(DisplayParam &param);
    virtual int SetDisplayMode(uint32_t xRes, uint32_t yRes, uint32_t bpp);
    virtual int Enable();
    virtual int Disable();
    virtual int GetEvent(Event &evt);
    virtual int CopyDirtyPixels(uint8_t *dst, uint32_t left, uint32_t top, uint32_t right, uint32_t bottom);
	virtual char *GetVideoMemory();

private:
    uint8_t *pPixels;
    uint32_t pixelsLen;
	bool running;
	FILE *inputFile;
};

int DummyDrvIntf::Enable()
{
	running = true;
	return 0;
}

int DummyDrvIntf::Disable()
{
	running = false;
	return 0;
}


int DummyDrvIntf::CopyDirtyPixels(uint8_t *dst, uint32_t left, uint32_t top, uint32_t right, uint32_t bottom)
{

    uint32_t Bpp = ((bpp+7)/8);
    uint32_t offset = (left * Bpp); 
    uint32_t copyLineSize = (right - left) * Bpp;
    uint32_t LineSize = xRes*Bpp;

    uint32_t copied = 0;
    uint8_t *dest = dst + top*LineSize+offset;
    uint8_t *src = pPixels + top*LineSize+offset;
    for (uint32_t y = top; y < bottom; y++) {
        memcpy(dest, src, copyLineSize); 
        dest += LineSize;
        src += LineSize;
        copied += copyLineSize;
    } 

	return 0;
}

int DummyDrvIntf::GetEvent(Event &evt)
{
    uint32_t x, y, w, h, color;
    uint32_t dx, dy, i, j;
	uint32_t *dst;
    uint32_t LineSize;

	while (running) {
        RTThreadSleep(1000*2);

		if (this->inputFile) {
			fread(pPixels, 1, xRes*yRes*(bpp>>3), inputFile);
			x = 0;
			y = 0;
			w = xRes;
			h = yRes;
		} else {
			x = RTRandU32() % xRes;
			y = RTRandU32() % yRes;
			dx = xRes - x;
			dy = yRes - y;
			if (dx == 0 || dy == 0) {
				continue;
			}
			w = RTRandU32() % dx;
			h = RTRandU32() % dy;
			w += x;
			h += y;
			// only support 32bit true color
			LineSize = xRes*((bpp+7)/8);
			color = RTRandU32() & 0xFFFFFF;
			for (i = y; i < h; i++) {
				dst = (uint32_t*)&(pPixels[LineSize*i]);
				dst = (uint32_t*)&(pPixels[LineSize*i]);
				for (j = x; j < w; j++) {
					dst[j] = color;
				}
			}
		}
        evt.code = EVENT_DIRTY_AREA;
        evt.dirtyArea.left = x;
        evt.dirtyArea.top = y;
        evt.dirtyArea.right = w;
        evt.dirtyArea.bottom = h;
		return 0;
	}

	// not running
    evt.code = EVENT_QUIT;
	return 0;
}

int DummyDrvIntf::SetDisplayMode(uint32_t xRes, uint32_t yRes, uint32_t bpp)
{
    int ret = 0;

    if (!xRes || !yRes || !bpp) {
        return -1;
    }
    if (pPixels) {
        delete pPixels;
    }
	
	// Now only support 32bit true color
	if (bpp != 32) return -1;
    pixelsLen = xRes * yRes * ((bpp+7)/8);
    pPixels = new uint8_t[pixelsLen];
    Assert(pPixels);

    this->xRes = xRes; 
    this->yRes = yRes; 
    this->bpp = bpp; 

	return 0;
}

char *DummyDrvIntf::GetVideoMemory()
{
	return NULL;
}

int DummyDrvIntf::Init(DisplayParam &param)
{
	if (param.inputFile)
		inputFile = fopen(param.inputFile, "r");
	// Always return success
	return 0;
}

DummyDrvIntf::~DummyDrvIntf()
{
	if (pPixels) {
		delete pPixels;
	}
	if (inputFile) {
		fclose(inputFile);
	}
	inputFile = NULL;
	pPixels = NULL;
	pixelsLen = 0;
};

DrvIntf *DummyDrvProbe(DisplayParam &param)
{
    DummyDrvIntf *dummy= new DummyDrvIntf;
    Assert(dummy);
    if (dummy->Init(param)) {
        delete dummy;
        return NULL;
    }
    return dummy;
}

#endif /* __DUMMY_DRV_INTF_H__ */
