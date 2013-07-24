#include "VNCDisplay.h"

int VNCDisplay::Init(DisplayParam &param)
{
    int rc;

    xRes = param.x;
    yRes = param.y;
    bpp= param.bpp;

    if (pPixels) {
        delete pPixels;
    }
    pixelsLen = xRes * yRes * ((bpp+7)/8);
    pPixels = new uint8_t[pixelsLen];
    Assert(pPixels);

    vncServer = rfbGetScreen(0, NULL, xRes, yRes, 8, 3, VNC_SIZEOFRGBA);
    Assert(vncServer);

    vncServer->frameBuffer = (char *)pPixels;
    vncServer->serverFormat.redShift = 16;
    vncServer->serverFormat.greenShift = 8;
    vncServer->serverFormat.blueShift = 0;
    vncServer->screenData = (void *)this;
    vncServer->desktopName = "VirtualMonitor";
    vncServer->httpDir = "./webclients";
    vncServer->httpEnableProxyConnect = TRUE;
    char szIPv4ListenAll[] = "0.0.0.0";
    // get listen address
    if (param.net.ipv4Addr[0]) {
        strncpy(ipv4Addr, param.net.ipv4Addr, sizeof(ipv4Addr));
    } else {
        strncpy(ipv4Addr, szIPv4ListenAll, sizeof(ipv4Addr));
    }

    if (ipv4Port == 0)
        vncServer->autoPort = 1;
	else
    	ipv4Port = param.net.ipv4Port;
#ifndef ENABLE_IPv6 
    if (!rfbStringToAddr(ipv4Addr, &vncServer->listenInterface)) {
        printf("could not parse VNC server listen address '%s'\n", ipv4Addr);
    }

    vncServer->port = ipv4Port;

    rfbInitServer(vncServer);

    vncServer->newClientHook = &VNCDisplay::rfbNewClientEvent;
    vncServer->kbdAddEvent = &VNCDisplay::vncKeyboardEvent;
    vncServer->ptrAddEvent = &VNCDisplay::vncMouseEvent;
#else
    char szIPv6ListenAll[] = "::";
    uint32_t cbOut = 0;
    RTNETADDRTYPE enmAddrType;
    size_t resSize = 42;
    char pszGetAddrInfo4[42]; // used to store the result of RTSocketQueryAddressStr()
    char pszGetAddrInfo6[42]; // used to store the result of RTSocketQueryAddressStr()
    char *pszServerAddress4 = NULL;
    char *pszServerAddress6 = NULL;

    if (param.net.ipv6Addr[0]) {
        strncpy(ipv6Addr, param.net.ipv6Addr, sizeof(ipv6Addr));
    } else {
        strncpy(ipv6Addr, szIPv6ListenAll, sizeof(ipv6Addr));
    }
    ipv6Port = param.net.ipv6Port;

    if (strlen(ipv6Addr) > 0) {
        resSize = 42;
        enmAddrType = RTNETADDRTYPE_IPV6;
        rc = RTSocketQueryAddressStr(ipv6Addr, pszGetAddrInfo6, &resSize, &enmAddrType);
        if (RT_SUCCESS(rc)) {
            pszServerAddress6 = pszGetAddrInfo6;
            printf("IPv6 Address: %s\n", pszServerAddress6);
        }
    }
    if (strlen(ipv4Addr) > 0) {
        resSize = 16;
        enmAddrType = RTNETADDRTYPE_IPV4;
        rc = RTSocketQueryAddressStr(ipv4Addr, pszGetAddrInfo4, &resSize, &enmAddrType);
        
        if (RT_SUCCESS(rc)) {
            pszServerAddress4 = pszGetAddrInfo4;
            printf("IPv4 Address: %s\n", pszServerAddress4);
        }

    }
    if (ipv4Port == 0 || ipv6Port == 0)
        vncServer->autoPort = 1;
    else
    {
        vncServer->port = ipv4Port;
        vncServer->ipv6port = ipv6Port;
    }

    if (!rfbStringToAddr(pszServerAddress4,&vncServer->listenInterface))
        printf("VNC: could not parse VNC server listen address IPv4 '%s'\n", pszServerAddress4);

    vncServer->listen6Interface = pszServerAddress6;

    rfbInitServer(vncServer);

    vncServer->newClientHook = VNCDisplay::rfbNewClientEvent;
    vncServer->kbdAddEvent = VNCDisplay::vncKeyboardEvent;
    vncServer->ptrAddEvent = VNCDisplay::vncMouseEvent;

    // notify about the actually used port
    int port = 0;
    port = vncServer->ipv6port;

    if (vncServer->listen6Sock < 0)
    {
        printf("VNC: not able to bind to IPv6 socket with address '%s'\n",pszServerAddress6);
        port = 0;

    }


#endif

    return 0;
}

void VNCDisplay::vncMouseEvent(int buttonMask, int x, int y, rfbClientPtr cl)
{
    VNCDisplay *instance = static_cast<VNCDisplay*>(cl->screen->screenData);

    VRDEINPUTPOINT point;
    unsigned button = 0;
    if (buttonMask & 1) button |= VRDE_INPUT_POINT_BUTTON1;
    if (buttonMask & 2) button |= VRDE_INPUT_POINT_BUTTON3;
    if (buttonMask & 4) button |= VRDE_INPUT_POINT_BUTTON2;
    if (buttonMask & 8) button |= VRDE_INPUT_POINT_WHEEL_UP;
    if (buttonMask & 16) button |= VRDE_INPUT_POINT_WHEEL_DOWN;
    point.uButtons = button;
    point.x = x;
    point.y = y;
    // instance->mCallbacks->VRDECallbackInput(instance->mCallback, VRDE_INPUT_POINT, &point, sizeof(point));
    rfbDefaultPtrAddEvent(buttonMask, x, y, cl);
}

enum rfbNewClientAction VNCDisplay::rfbNewClientEvent(rfbClientPtr cl)
{
    VNCDisplay *instance = static_cast<VNCDisplay*>(cl->screen->screenData);

    ///@todo: we need auth user here

    // instance->mCallbacks->VRDECallbackClientConnect(instance->mCallback, (int)cl->sock);
    instance->uClients++;

    cl->clientGoneHook = &VNCDisplay::clientGoneHook;

    return RFB_CLIENT_ACCEPT;
}

void VNCDisplay::clientGoneHook(rfbClientPtr cl)
{
    VNCDisplay *instance = static_cast<VNCDisplay*>(cl->screen->screenData);

    instance->uClients--;
    // instance->mCallbacks->VRDECallbackClientDisconnect(instance->mCallback, (int)cl->sock, 0);
}


void VNCDisplay::vncKeyboardEvent(rfbBool down, rfbKeySym keycode, rfbClientPtr cl)
{
    VNCDisplay *instance = static_cast<VNCDisplay*>(cl->screen->screenData);
}

Display *VNCDisplayProbe(DisplayParam &param)
{
    VNCDisplay *vnc = new VNCDisplay();
    Assert(vnc);
    if (vnc->Init(param)) {
        printf("VNC Display Init Failed\n");
        delete vnc;
        return NULL;
    }
    return vnc;
}

static DECLCALLBACK(int) VRDEWokingThread(RTTHREAD Thread, void *pvUser)
{
    rfbScreenInfoPtr vncServer = rfbScreenInfoPtr(pvUser);
    rfbRunEventLoop(vncServer, -1, FALSE);
    return 0;
}


int VNCDisplay::Start()
{
    int rc;
    rc = RTThreadCreate(&thread, VRDEWokingThread, vncServer, 0, RTTHREADTYPE_VRDP_IO, 0, "VRDE");
    if (RT_FAILURE(rc))
    {
        printf("Error: Thread creation failed with %d\n", rc);
        return -1;
    }

    return 0;
}

int VNCDisplay::Stop()
{
    return 0;
}


int VNCDisplay::Update(uint32_t left, uint32_t top, uint32_t right, uint32_t bottom)
{
    rfbMarkRectAsModified(vncServer, left, top, right, bottom);
    return 0;
}

VNCDisplay::~VNCDisplay()
{
    if (pPixels) {
        delete pPixels;
    }
}
