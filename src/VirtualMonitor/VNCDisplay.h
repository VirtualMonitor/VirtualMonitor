#ifndef __VNC_DISPLAY_H__
#define __VNC_DISPLAY_H__

#include <iprt/err.h>
#include <iprt/net.h>
#include <iprt/socket.h>

#include <rfb/rfb.h>

#include "Display.h"

#define VNC_SIZEOFRGBA          4
#ifdef LIBVNCSERVER_IPv6
/*
* Some old version of libvncserver not support ipV6
* For windows we compile our own, it is support
*/
#if defined (RT_OS_WINDOWS)
#define ENABLE_IPv6
#endif
#endif

#define VRDE_INPUT_POINT_BUTTON1    0x01
#define VRDE_INPUT_POINT_BUTTON2    0x02
#define VRDE_INPUT_POINT_BUTTON3    0x04
#define VRDE_INPUT_POINT_WHEEL_UP   0x08
#define VRDE_INPUT_POINT_WHEEL_DOWN 0x10

typedef struct _VRDEINPUTPOINT
{
    int x;
    int y;
    unsigned uButtons;
} VRDEINPUTPOINT;

class VNCDisplay : public Display {
public:
    VNCDisplay() {};
    virtual ~VNCDisplay();
    virtual int Init(DisplayParam &param, char *pVideoMemory);
    virtual int Start();
    virtual int Stop();
    virtual int Update(uint32_t left, uint32_t top, uint32_t right, uint32_t bottom);

private:
    static DECLCALLBACK(enum rfbNewClientAction) rfbNewClientEvent(rfbClientPtr cl);
    static DECLCALLBACK(void) vncMouseEvent(int buttonMask, int x, int y, rfbClientPtr cl);
    static void vncKeyboardEvent(rfbBool down, rfbKeySym keySym, rfbClientPtr cl);
    static void clientGoneHook(rfbClientPtr cl);
private:
    uint32_t uClients;
    uint16_t ipv4Port;
    uint16_t ipv6Port;
    char ipv4Addr[ADDRESSSIZE];
    char ipv6Addr[ADDRESSSIZE];
    RTTHREAD thread;

    rfbScreenInfoPtr vncServer;
};
#endif /* __VNC_DISPLAY_H__ */

