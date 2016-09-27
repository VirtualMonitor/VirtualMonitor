#include "XDrvIntf.h"

XDrvIntf::XDrvIntf()
{
    pPixels = NULL; pixelsLen = 0;
}

int XDrvIntf::Enable()
{
    running = true;
    int ret = 0;
    return ret;
}

int XDrvIntf::Disable()
{
    running = false;
    return 0;
}


int XDrvIntf::CopyDirtyPixels(uint8_t *dst, uint32_t left, uint32_t top, uint32_t right, uint32_t bottom)
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

int XDrvIntf::GetEvent(Event &evt)
{
    evtOut=&evt;
    while (running) {
        int rc= xpixelBuffer->getEvent(evt);
        xpixelBuffer->refresh(evt.dirtyArea.right, evt.dirtyArea.bottom);
        return rc;
    }

    // not running
    evt.code = EVENT_QUIT;
    return 0;

}


int XDrvIntf::SetDisplayMode(uint32_t xRes, uint32_t yRes, uint32_t bpp)
{
    if (!xRes || !yRes || !bpp) {
           return -1;
   }
   if (pPixels) {
       delete pPixels;
   }

   this->xRes=xRes;
   this->yRes=yRes;
   this->bpp =bpp;

   xpixelBuffer = new XPixelBuffer();
   xpixelBuffer->initialise(xRes,yRes,bpp);

   pPixels = (uint8_t *)xpixelBuffer->getFrameBuffer();

   return 0;

}

char *XDrvIntf::GetVideoMemory()
{
    return xpixelBuffer->getFrameBuffer();
}

int XDrvIntf::Init(DisplayParam &param)
{
    if (param.inputFile)
        inputFile = fopen(param.inputFile, "r");
    // Always return success
    return 0;
}

XDrvIntf::~XDrvIntf()
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

    if (xpixelBuffer)
        delete xpixelBuffer;

}

DrvIntf *XDrvProbe(DisplayParam &param)
{
    XDrvIntf *drv= new XDrvIntf;
    Assert(drv);
    if (drv->Init(param)) {
        delete drv;
        return NULL;
    }
    return drv;
}

