#ifndef __DRV_INTF_H__
#define __DRV_INTF_H__
#define  RT_STRICT 
#include <VBox/types.h>
#include <iprt/assert.h>
#include "Display.h"

enum EVENT_CODE {
    EVENT_DIRTY_AREA,
    EVENT_TIMEOUT,
	EVENT_QUIT,
};

typedef struct _Event {
    EVENT_CODE code;
    union {
        struct {
            uint32_t left;
            uint32_t top;
            uint32_t right;
            uint32_t bottom;
        } dirtyArea;
        struct {
            uint32_t x;
            uint32_t y;
        } PointerMove;
        struct {
            uint32_t fFlags;
            uint32_t cHotX;
            uint32_t cHotY;
            uint32_t cWidth;
            uint32_t cHeight;
            uint32_t cbLength;
            uint8_t *pPixels;
        } PointerShape;
    };
} Event;

class DrvIntf {
public:
    DrvIntf() {};
    virtual ~DrvIntf() { };
    virtual int Init(DisplayParam &param) = 0;
    virtual int SetDisplayMode(uint32_t xRes, uint32_t yRes, uint32_t bpp) = 0;
    virtual int Enable() = 0;
    virtual int Disable() = 0;
    virtual int GetEvent(Event &evt) = 0;
    virtual int CopyDirtyPixels(uint8_t *dst, uint32_t left, uint32_t top, uint32_t right, uint32_t bottom) = 0;
	virtual char *GetVideoMemory() = 0;

protected:
    uint32_t xRes;
    uint32_t yRes;
    uint32_t bpp;
};
#endif /* __DRV_INTF_H__ */
