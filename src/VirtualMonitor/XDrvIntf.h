#ifndef XDRVINTF_H
#define XDRVINTF_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iprt/rand.h>
#include <iprt/thread.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <memory>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include "DrvIntf.h"
#include "Display.h"
#include "XPixelBuffer.h"

#define DISPLAYKEY "display"


class XDrvIntf: public DrvIntf {
public:

    XDrvIntf();
    virtual ~XDrvIntf();

    virtual int Init(DisplayParam &param);
    virtual int SetDisplayMode(uint32_t xRes, uint32_t yRes, uint32_t bpp);
    virtual int Enable();
    virtual int Disable();
    virtual int GetEvent(Event &evt);
    virtual int CopyDirtyPixels(uint8_t *dst, uint32_t left, uint32_t top, uint32_t right, uint32_t bottom);
    virtual char *GetVideoMemory();

private:

    Event *evtOut;

    uint8_t *pPixels;
    uint32_t pixelsLen;

    uint8_t xRes;
    uint8_t yRes;
    uint8_t bpp;

    int screensize;

    bool running;

    FILE *inputFile;
\
    int init(void);

    XPixelBuffer* xpixelBuffer;

};


#endif // XDRVINTF_H
