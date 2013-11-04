#ifndef __DISPLAY_H__
#define __DISPLAY_H__
#define  RT_STRICT 
#include <VBox/types.h>
#include <iprt/assert.h>

#define ADDRESSSIZE         60
typedef struct _DisplayParam {
    uint32_t x;
    uint32_t y;
    uint32_t bpp;
    union {
        struct {
            char ipv4Addr[ADDRESSSIZE];
            uint16_t ipv4Port;
            char ipv6Addr[ADDRESSSIZE];
            uint16_t ipv6Port;
        } net;
        struct {
            uint32_t dummy;
        } usb;
    };
	bool enableDummyDriver;
	// for Testing
	char *inputFile;
} DisplayParam;

class Display {
public:
    Display () { xRes = yRes = bpp = pixelsLen = 0; pPixels = NULL;};
    virtual ~Display() { };
    virtual int Init(DisplayParam &param) = 0;
    virtual int Start() = 0;
    virtual int Stop() = 0;
    virtual int Update(uint32_t left, uint32_t top, uint32_t right, uint32_t bottom) = 0;

public:
    uint8_t *pPixels;
    uint32_t pixelsLen;

protected:
    uint32_t xRes;
    uint32_t yRes;
    uint32_t bpp;
};

#endif /* __DISPLAY_H__ */
