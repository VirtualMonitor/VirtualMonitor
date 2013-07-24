/* $Id: DisplayImpl.h $ */
/** @file
 * VBox frontends: Basic Frontend (BFE):
 * Declaration of Display class
 */

/*
 * Copyright (C) 2006-2007 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ____H_DISPLAYIMPL
#define ____H_DISPLAYIMPL

#include <iprt/semaphore.h>
#include <VBox/vmm/pdm.h>

#include "Framebuffer.h"

class Console;
struct VBVACMDHDR;


typedef DECLCALLBACK(int) FNDISPDRVCALLBACK(PPDMIDISPLAYPORT pUpPort, PPDMIDISPLAYCONNECTOR pConnector);
typedef FNDISPDRVCALLBACK* PFNDISPDRVCALLBACK;

/** Pointer shape change event data structure */
struct PointerShape
{
    PointerShape(BOOL aVisible, BOOL aAlpha, ULONG aXHot, ULONG aYHot,
                            ULONG aWidth, ULONG aHeight,
                            const uint8_t *aShape)
        : visible (aVisible), alpha (aAlpha), xHot (aXHot), yHot (aYHot)
        , width (aWidth), height (aHeight), shape (NULL)
    {
        // make a copy of the shape
        if (aShape) {
            uint32_t shapeSize = ((((aWidth + 7) / 8) * aHeight + 3) & ~3) + aWidth * 4 * aHeight;
            shape = new uint8_t [shapeSize];
            if (shape)
                memcpy ((void *) shape, (void *) aShape, shapeSize);
        }
    }

    ~PointerShape()
    {
        if (shape) delete[] shape;
    }

    const BOOL visible;
    const BOOL alpha;
    const ULONG xHot;
    const ULONG yHot;
    const ULONG width;
    const ULONG height;
    const uint8_t *shape;
};

class Display
{

public:

    Display();
    ~Display();

    // public methods only for internal purposes
    int handleDisplayResize (int w, int h);
    void handleDisplayUpdate (int x, int y, int cx, int cy);
    int handlePointerShape(bool fVisible, bool fAlpha, uint32_t xHot, uint32_t yHot, uint32_t width, uint32_t height, const void *pShape);

    int VideoAccelEnable (bool fEnable, struct VBVAMEMORY *pVbvaMemory);
    void VideoAccelFlush (void);
    bool VideoAccelAllowed (void);

    void SetVideoModeHint(ULONG aDisplay, BOOL aEnabled, BOOL aChangeOrigin, LONG aOriginX, LONG aOriginY, ULONG aWidth, ULONG aHeight, ULONG aBitsPerPixel);

    static const PDMDRVREG  DrvReg;

    uint32_t getWidth();
    uint32_t getHeight();
    uint32_t getBitsPerPixel();

    STDMETHODIMP ConnectToDrv(PFNDISPDRVCALLBACK pfnDispDrvCallBack);
    STDMETHODIMP SetFramebuffer(unsigned iScreenID, Framebuffer *Framebuffer);
    STDMETHODIMP InvalidateAndUpdate();
    STDMETHODIMP ResizeCompleted();
    STDMETHODIMP GetScreenResolution(ULONG aScreenId, ULONG *aWidth, ULONG *aHeight, ULONG *aBitsPerPixel);
    STDMETHOD(GetFramebuffer)(ULONG aScreenId, Framebuffer **aFramebuffer, LONG *aXOrigin, LONG *aYOrigin);

    void getFramebufferDimensions(int32_t *px1, int32_t *py1, int32_t *px2,
                                  int32_t *py2);

    void resetFramebuffer();

    void setRunning(void) { mfMachineRunning = true; };

private:

    void updateDisplayData();

    static DECLCALLBACK(void*) drvQueryInterface(PPDMIBASE pInterface, const char *pszIID);
    static DECLCALLBACK(int)   drvConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfg, uint32_t fFlags);
    static DECLCALLBACK(int)   displayResizeCallback(PPDMIDISPLAYCONNECTOR pInterface, uint32_t bpp, void *pvVRAM, uint32_t cbLine, uint32_t cx, uint32_t cy);
    static DECLCALLBACK(void)  displayUpdateCallback(PPDMIDISPLAYCONNECTOR pInterface,
                                                     uint32_t x, uint32_t y, uint32_t cx, uint32_t cy);
    static DECLCALLBACK(void)  displayRefreshCallback(PPDMIDISPLAYCONNECTOR pInterface);
    static DECLCALLBACK(void)  displayResetCallback(PPDMIDISPLAYCONNECTOR pInterface);
    static DECLCALLBACK(void)  displayLFBModeChangeCallback(PPDMIDISPLAYCONNECTOR pInterface, bool fEnabled);
    static DECLCALLBACK(void)  displayProcessAdapterDataCallback(PPDMIDISPLAYCONNECTOR pInterface, void *pvVRAM, uint32_t u32VRAMSize);
    static DECLCALLBACK(void)  displayProcessDisplayDataCallback(PPDMIDISPLAYCONNECTOR pInterface, void *pvVRAM, unsigned uScreenId);

    static DECLCALLBACK(int) displayMousePointerShape(PPDMIDISPLAYCONNECTOR pInterface, bool fVisible, bool fAlpha,
                                                        uint32_t xHot, uint32_t yHot,
                                                        uint32_t cx, uint32_t cy,
                                                        const void *pvShape);

    static DECLCALLBACK(void) doInvalidateAndUpdate(struct DRVMAINDISPLAY  *mpDrv);

    /** Pointer to the associated display driver. */
    struct DRVMAINDISPLAY  *mpDrv;

    PointerShape *mPointerShape;
    ULONG mPointerX;
    ULONG mPointerY;

    Console *mParent;
    Framebuffer *mFramebuffer;
    bool mFramebufferOpened;

    ULONG mSupportedAccelOps;

    struct VBVAMEMORY *mpVbvaMemory;
    bool        mfVideoAccelEnabled;

    struct VBVAMEMORY *mpPendingVbvaMemory;
    bool        mfPendingVideoAccelEnable;
    bool        mfMachineRunning;

    uint8_t    *mpu8VbvaPartial;
    uint32_t   mcbVbvaPartial;

    bool vbvaFetchCmd (struct VBVACMDHDR **ppHdr, uint32_t *pcbCmd);
    void vbvaReleaseCmd (struct VBVACMDHDR *pHdr, int32_t cbCmd);

    void handleResizeCompletedEMT (void);
    volatile uint32_t mu32ResizeStatus;

    enum {
        ResizeStatus_Void,
        ResizeStatus_InProgress,
        ResizeStatus_UpdateDisplayData
    };
};

extern Display *gDisplay;

#endif // !____H_DISPLAYIMPL
