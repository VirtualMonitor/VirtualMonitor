#ifndef __XPDM_DRV_INTF_H__
#define __XPDM_DRV_INTF_H__

#include <windows.h>

#include "DrvIntf.h"
#include <stdio.h>

#define QUIT_EVENT_NAME "Global\\VirtualMonitorQEvent"
#define DRIVER_NAME "VirtualMonitor Graphics Adapter"
#define DISPLAY_VIDEO_KEY "SYSTEM\\CurrentControlSet\\Control\\Video"

#define VM_CTL_CODE_START                 0xABCD90A0
#define VM_CTL_SET_MONITOR                (VM_CTL_CODE_START+1)
#define VM_CTL_SET_EVENT                  (VM_CTL_CODE_START+2)
#define VM_CTL_UNSET_EVENT                (VM_CTL_CODE_START+3)
#define VM_CTL_GET_DIRTY_EARA             (VM_CTL_CODE_START+4)
#define VM_CTL_UPDATE_DIRTY_EARA          (VM_CTL_CODE_START+5)
#define VM_CTL_GET_POINTER_SHAPE_EVENT    (VM_CTL_CODE_START+6)
#define VM_CTL_GET_POINTER_MOVE_EVENT     (VM_CTL_CODE_START+7)
#define VM_CTL_MAP_VIDEO_MEMORY	          (VM_CTL_CODE_START+8)
#define VM_CTL_UNMAP_VIDEO_MEMORY         (VM_CTL_CODE_START+9)

static LPSTR dispCode[7] = {
   "Change Successful",
   "Must Restart",
   "Bad Flags",
   "Bad Parameters",
   "Failed",
   "Bad Mode",
   "Not Updated"
};

#define NUM_OF_EVENT 5
struct EventArray {
    EventArray() {
        // Update Dirty area
        event[0] = CreateEvent(NULL, FALSE, FALSE, NULL);
        Assert(event[0]);

        // Restart
        event[1] = CreateEvent(NULL, FALSE, FALSE, NULL);
        Assert(event[1]);

        // Quit
        event[2] = CreateEvent(NULL, FALSE, FALSE, QUIT_EVENT_NAME);
        Assert(event[2]);

        // Pointer Shape
        event[3] = CreateEvent(NULL, FALSE, FALSE, NULL);
        Assert(event[3]);

        // Pointer MOVE
        event[4] = CreateEvent(NULL, FALSE, FALSE, NULL);
        Assert(event[4]);
    }
    ~EventArray() {
        for (int i = 0; i < NUM_OF_EVENT; i++) {
	        CloseHandle(event[i]);
        }
    }
    HANDLE event[NUM_OF_EVENT];
};

#define DEVICE_NAME_SIZE 32
class XpdmDrvIntf: public DrvIntf {
public:
    XpdmDrvIntf();
    virtual ~XpdmDrvIntf();

    virtual int Init(DisplayParam &param);
    virtual int SetDisplayMode(uint32_t xRes, uint32_t yRes, uint32_t bpp);
    virtual int Enable();
    virtual int Disable();
    virtual int GetEvent(Event &evt);
    virtual int CopyDirtyPixels(uint8_t *dst, uint32_t left, uint32_t top, uint32_t right, uint32_t bottom);
	virtual char *GetVideoMemory();

private:
	HKEY hKey;
    HDC hDC;
    uint8_t *pPixels;
    uint32_t pixelsLen;
    RECT dirtyRect;
    struct EventArray events;
    CHAR deviceName[DEVICE_NAME_SIZE];
	OSVERSIONINFO osvi;
	PVOID pVideoMemory;

    LPSTR GetDispCode(INT code);
    BOOL OpenDeviceRegistryKey();
    BOOL FindDeviceName();
    BOOL XpdmDrvCtrlReStart();
    int ForceUpDate(uint32_t left, uint32_t top, uint32_t right, uint32_t bottom);
	int XpdmDrvIntf::AeroCtrl(BOOL enable);
};

#endif /* __XPDM_DRV_INTF_H__ */

