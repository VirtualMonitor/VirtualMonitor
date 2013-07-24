#include <VBox/types.h>
#include <VBox/err.h>
#include <VBox/log.h>
#include <VBox/param.h>
#include <VBox/vmm/pdm.h>
#include <VBox/version.h>
#include <VBox/vmm/pdm.h>
#include <iprt/stream.h>
#include <iprt/rand.h>
#include <stdio.h>
#ifdef RT_OS_WINDOWS
# include <Windows.h>
#endif

#include "ConsoleImpl.h"
#include "Framebuffer.h"

extern Console           *gConsole;
extern Framebuffer       *gFramebuffer;

typedef struct _EVENT_ARRAY {
    HANDLE hPaintEvent;
    HANDLE hRestartEvent;
    HANDLE hExitEvent;
#if 0
    HANDLE hPointerShapeEvent;
    HANDLE hPointerMoveEvent;
#endif
} EVENT_ARRAY, *PEVENT_ARRAY;

#define DEVICE_NAME_SIZE 32
typedef struct XpdmMonitor {
    PPDMIDISPLAYCONNECTOR pConnector; 
    bool volatile fRender; 
    uint32_t cMilliesInterval;
    HANDLE hWorkThread;
    DWORD dwThreadId;
    CHAR s_DeviceName[DEVICE_NAME_SIZE];
    EVENT_ARRAY events;
    HDC hDC;
    RECT dirtyRect;
    PUCHAR frameBuffer;
} XpdmMonitor;

XpdmMonitor gXpdmMonitor;

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

static LPSTR dispCode[7] = {
   "Change Successful",
   "Must Restart",
   "Bad Flags",
   "Bad Parameters",
   "Failed",
   "Bad Mode",
   "Not Updated"
};

static LPSTR GetDispCode(INT code)
{
	static char tmp[MAX_PATH];

	switch (code) {
   		case DISP_CHANGE_SUCCESSFUL:
			return dispCode[0];
		case DISP_CHANGE_RESTART:
			return dispCode[1];
		case DISP_CHANGE_BADFLAGS:
			return dispCode[2];
		case DISP_CHANGE_BADPARAM:
			return dispCode[3];
		case DISP_CHANGE_FAILED:
			return dispCode[4];
		case DISP_CHANGE_BADMODE:
			return dispCode[5];
		case DISP_CHANGE_NOTUPDATED:
			return dispCode[6];
		default:
			_snprintf(&tmp[0], sizeof(tmp), "Unknown code: %08x\n", code);
			return (LPTSTR)&tmp[0];
   
	}
}

static BOOL DoSetDisplayMode(
			HKEY hKey,
			DWORD xRes,
			DWORD yRes,
			DWORD bpp)
{
	LONG rc;
	BOOL ret = TRUE;
	CHAR tmp[MAX_PATH+1];
	DWORD len;

	rc = RegSetValueEx(hKey, 
			"CustomXRes",
			0,
			REG_BINARY,
			(const PBYTE)&xRes,
			sizeof(xRes));	
	if (rc != ERROR_SUCCESS) {
		printf("Set xRes Failed:%x\n", GetLastError());
		ret = FALSE;
	}

	rc = RegSetValueEx(hKey,
			"CustomYRes",
			0,
			REG_BINARY,
			(const PBYTE)&yRes,
			sizeof(yRes));
	if (rc != ERROR_SUCCESS) {
		printf("Set yRes Failed:%x\n", GetLastError());
		ret = FALSE;
	}
	rc = RegSetValueEx(hKey,
			"CustomBPP",
			0,
			REG_BINARY,
			(const PBYTE)&bpp,
			sizeof(bpp));
	if (rc != ERROR_SUCCESS) {
		printf("Set bpp Failed:%x\n", GetLastError());
		ret = FALSE;
	}
	len = GetTempPath(MAX_PATH+1, &tmp[0]);	
	printf("TmpPath: %s\n", tmp);
	if (len) {
		rc = RegSetValueEx(hKey, "path", 0, REG_MULTI_SZ, (const PBYTE)&tmp[0], len);
		if (rc != ERROR_SUCCESS) {
			printf("%s: %d: LastError: %d\n", __FUNCTION__, __LINE__, GetLastError());
		}
	} else {
		return -1;
	}
	return ret;
}

BOOL SetDisplayMode(
		DWORD xRes,
		DWORD yRes,
		DWORD bpp)
{
	HKEY hRegRoot;
	HKEY hSubKey;
	LONG rc;
	LONG query;
	DWORD dwIdIndex;
	CHAR idName[64];
	DWORD idSize = sizeof(idName);
	CHAR video[64];
	DWORD dwType = 0;
	CHAR lpDesc[256]; 
	DWORD dwSize= sizeof(lpDesc);
	INT found = 0;
	BOOL ret = FALSE;

	rc = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
			DISPLAY_VIDEO_KEY,
			0,
			KEY_ALL_ACCESS,
			&hRegRoot);
	
	if (rc != ERROR_SUCCESS) {
		printf("Open Reg Root Key Failed:%x\n", GetLastError());
		return FALSE;
	}

	dwIdIndex = 0;
	do {
		query = RegEnumKeyEx(hRegRoot,
				dwIdIndex,
				&idName[0],
				&idSize,
				0,
				NULL,
				NULL,
				NULL);
		idSize = sizeof(idName);
		dwIdIndex++;

		sprintf(video, "%s\\0000", idName);
		rc = RegOpenKeyEx(hRegRoot,
				video,
				0,
				KEY_ALL_ACCESS,
				&hSubKey);
		if (rc != ERROR_SUCCESS) {
			printf("Open Reg Sub Key Failed:%x\n", GetLastError());
			goto OpenSubKeyFailed;
		}
		rc = RegQueryValueEx(hSubKey,
				"Device Description",
				0,
				&dwType,
				(PBYTE)&lpDesc,
				&dwSize);
		// Don't need to do this Check.
#if 0
		if (rc != ERROR_SUCCESS) {
			printf("Query Reg Sub Key Desc Failed:%x\n", GetLastError());
			goto QuerySubKeyFailed;
		}
#endif
		dwSize= sizeof(lpDesc);
		if (dwType == REG_SZ) {
			if (memcmp(lpDesc, DRIVER_NAME, sizeof(DRIVER_NAME)) == 0) {
				ret = DoSetDisplayMode(hSubKey, xRes, yRes, bpp);
			}
		}
		RegCloseKey(hSubKey);
	} while (query != ERROR_NO_MORE_ITEMS);
	RegCloseKey(hRegRoot);

	return ret;

#if 0
QuerySubKeyFailed:
	RegCloseKey(hSubKey);
#endif
OpenSubKeyFailed:
	RegCloseKey(hRegRoot);
	return FALSE;
}

/* Initialization */
static BOOL FindDeviceName()
{
	INT devNum = 0;
	DISPLAY_DEVICE displayDevice;
	BOOL result;
	// CHAR deviceNum[MAX_PATH];
        // LPSTR deviceSub;
	BOOL bFound = FALSE;

        FillMemory(&displayDevice, sizeof(DISPLAY_DEVICE), 0);
        displayDevice.cb = sizeof(DISPLAY_DEVICE);

        // First enumerate for Primary display device:
        while ((result = EnumDisplayDevices(NULL,
				devNum,
				&displayDevice,
				0))) {
		printf("Device: %s\n", &displayDevice.DeviceString[0]);
		if (strcmp(&displayDevice.DeviceString[0],
			DRIVER_NAME) == 0) {
			bFound = TRUE;

			memcpy(&gXpdmMonitor.s_DeviceName[0],
				(LPSTR)&displayDevice.DeviceName[0],
				sizeof(displayDevice.DeviceName));


			break;	
		}
		devNum++;
        }
	if (bFound == FALSE) {
		return FALSE;
	}

	return TRUE;
}

BOOL DoDisableVirtualMonitor()
{
	DEVMODE defaultMode;
	// HDC hdc;
	INT code;

	if (!FindDeviceName()) {
		return FALSE;
	}

	ZeroMemory(&defaultMode, sizeof(DEVMODE));
	defaultMode.dmSize = sizeof(DEVMODE);
       	defaultMode.dmDriverExtra = 0;

	defaultMode.dmFields = DM_BITSPERPEL |
				DM_PELSWIDTH | 
				DM_PELSHEIGHT |
				DM_POSITION |
				DM_DISPLAYFREQUENCY |
				DM_DISPLAYFLAGS;

	code = ChangeDisplaySettingsEx(gXpdmMonitor.s_DeviceName, 
					&defaultMode,
					NULL,
					CDS_NORESET | CDS_UPDATEREGISTRY,
					NULL); 


	printf("Update Registry on device mode: %s\n", GetDispCode(code));
	code = ChangeDisplaySettingsEx(NULL,
					NULL,
					NULL,
					0,
					NULL);
	printf("Raw dynamic mode change on device mode: %s\n", GetDispCode(code));

	return TRUE;

}


BOOL DisableVirtualMonitor()
{
	HANDLE e;	
	e = OpenEvent(EVENT_MODIFY_STATE, 1, QUIT_EVENT_NAME);
	if (e == NULL) {
		printf("Quit Event Failed: %d\n", GetLastError());
		return FALSE;
	}
	SetEvent(e);
	CloseHandle(e);
	return TRUE;
}

BOOL EnableVirtualMonitor(DWORD xRes, DWORD yRes, DWORD bpp)
{
	DEVMODE defaultMode;
	HDC hdc;
	int nWidth;
	INT code;
	DWORD dwFlags = 0;
	// CHAR deviceNum[MAX_PATH];
        // LPSTR deviceSub;

    RTPrintf("%s %d, %d, %d, %d\n", __FUNCTION__, __LINE__, xRes, yRes, bpp);
	hdc = GetDC(0);
	// nWidth = GetDeviceCaps(hdc,HORZRES);
	nWidth = GetSystemMetrics(SM_CXSCREEN);
	ReleaseDC(0,hdc);
	
 	ZeroMemory(&defaultMode, sizeof(DEVMODE));
	defaultMode.dmSize = sizeof(DEVMODE);
       	defaultMode.dmDriverExtra = 0;

	/* Without this, Windows will not ask the miniport for its
	 * mode table but uses an internal cache instead.
	 */
	EnumDisplaySettings(gXpdmMonitor.s_DeviceName, 0xffffff, &defaultMode);

	ZeroMemory(&defaultMode, sizeof(DEVMODE));
	defaultMode.dmSize = sizeof(DEVMODE);
       	defaultMode.dmDriverExtra = 0;

	if (!EnumDisplaySettings(gXpdmMonitor.s_DeviceName,
		     ENUM_REGISTRY_SETTINGS, &defaultMode)) {
		printf("Device: %s\n", gXpdmMonitor.s_DeviceName);
		return FALSE; // Store default failed
	}

#if 0
	if (defaultMode.dmPelsWidth == 0 || defaultMode.dmPelsHeight == 0) {
		if (!EnumDisplaySettings(gXpdmMonitor.s_DeviceName,
		     ENUM_CURRENT_SETTINGS, &defaultMode)) {
			ZeroMemory(&defaultMode, sizeof(DEVMODE));
			defaultMode.dmSize = sizeof(DEVMODE);
			defaultMode.dmDriverExtra = 0;
		}
	}
#endif

	defaultMode.dmPelsWidth = xRes;
	defaultMode.dmPelsHeight = yRes;
	defaultMode.dmBitsPerPel = bpp;

	defaultMode.dmFields = DM_BITSPERPEL |
				DM_PELSWIDTH | 
				DM_PELSHEIGHT |
				DM_POSITION ;


	defaultMode.dmPosition.x += nWidth;

       	// StringCbCopy((LPSTR)&defaultMode.dmDeviceName[0], sizeof(defaultMode.dmDeviceName), "mirror");

	code = ChangeDisplaySettingsEx(gXpdmMonitor.s_DeviceName, 
					&defaultMode,
					NULL,
					CDS_NORESET | CDS_UPDATEREGISTRY,
					NULL); 


	printf("Update Registry on device mode: %s\n", GetDispCode(code));
	code = ChangeDisplaySettingsEx(NULL,
					NULL,
					NULL,
					0,
					NULL);
	printf("Raw dynamic mode change on device mode: %s\n", GetDispCode(code));

	return TRUE;
}


static BOOL EventInit(PEVENT_ARRAY e)
{
	e->hExitEvent = CreateEvent(NULL, FALSE, FALSE, QUIT_EVENT_NAME);
	if (!e->hExitEvent) {
		return FALSE;
	}

	e->hPaintEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (!e->hPaintEvent) {
		CloseHandle(e->hExitEvent);
		return FALSE;
	}

	e->hRestartEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (!e->hRestartEvent) {
		CloseHandle(e->hExitEvent);
		CloseHandle(e->hPaintEvent);
		return FALSE;
	}

#if 0
	e->hPointerShapeEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (!e->hPointerShapeEvent) {
		CloseHandle(e->hExitEvent);
		CloseHandle(e->hPointerShapeEvent);
		return FALSE;
	}

	e->hPointerMoveEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (!e->hPointerMoveEvent) {
		CloseHandle(e->hExitEvent);
		CloseHandle(e->hPointerShapeEvent);
		CloseHandle(e->hPointerMoveEvent);
		return FALSE;
	}
#endif
	return TRUE;
}

static VOID EventUnInit(PEVENT_ARRAY e)
{
	CloseHandle(e->hExitEvent);
#if 0
	CloseHandle(e->hPointerShapeEvent);
	CloseHandle(e->hPointerMoveEvent);
#endif
}

static VOID XpdmQuit()
{
	DeleteDC(gXpdmMonitor.hDC);
	EventUnInit(&gXpdmMonitor.events);
	DoDisableVirtualMonitor();
	memset(&gXpdmMonitor, 0, sizeof(gXpdmMonitor));
}

static BOOL XpdmDrvIntfInit()
{
    if (!FindDeviceName()) {
        printf("Can't find dev\n");
        return FALSE;
    }
    if (!EventInit(&gXpdmMonitor.events)) {
        return FALSE;
    }
    if (!SetDisplayMode(gXpdmMonitor.pConnector->cx,
                              gXpdmMonitor.pConnector->cy,
                              gXpdmMonitor.pConnector->cBits)) {
	RTPrintf("%s: %d\n", __FUNCTION__, __LINE__);
        EventUnInit(&gXpdmMonitor.events);
        return FALSE;
    }

    if (!EnableVirtualMonitor(gXpdmMonitor.pConnector->cx,
                              gXpdmMonitor.pConnector->cy,
                              gXpdmMonitor.pConnector->cBits)) {
	RTPrintf("%s: %d\n", __FUNCTION__, __LINE__);
        EventUnInit(&gXpdmMonitor.events);
        return FALSE;
    }
    return TRUE;
}

static DECLCALLBACK(int) xpdmUpdateDisplay(PPDMIDISPLAYPORT pInterface)
{
	RTPrintf("%s: %d\n", __FUNCTION__, __LINE__);
	return 0;
}
static DECLCALLBACK(int) xpdmUpdateDisplayAll(PPDMIDISPLAYPORT pInterface)
{
	RTPrintf("%s: %d\n", __FUNCTION__, __LINE__);
	return 0;
}
static DECLCALLBACK(int) xpdmQueryColorDepth(PPDMIDISPLAYPORT pInterface, uint32_t *pcBits)
{
	RTPrintf("%s: %d\n", __FUNCTION__, __LINE__);
	return 0;
}
static DECLCALLBACK(int) xpdmSetRefreshRate(PPDMIDISPLAYPORT pInterface, uint32_t cMilliesInterval)
{
	RTPrintf("%s: %d\n", __FUNCTION__, __LINE__);
	return 0;
}
static DECLCALLBACK(int) xpdmDisplayBlt(PPDMIDISPLAYPORT pInterface, const void *pvData, uint32_t x, uint32_t y, uint32_t cx, uint32_t cy)
{
	RTPrintf("%s: %d\n", __FUNCTION__, __LINE__);
	return 0;
}
static DECLCALLBACK(void) xpdmUpdateDisplayRect (PPDMIDISPLAYPORT pInterface, int32_t x, int32_t y, uint32_t w, uint32_t h)
{
	RTPrintf("%s: %d\n", __FUNCTION__, __LINE__);
}
static DECLCALLBACK(int) xpdmCopyRect (PPDMIDISPLAYPORT pInterface,
                                          uint32_t w,
                                          uint32_t h,
                                          const uint8_t *pu8Src,
                                          int32_t xSrc,
                                          int32_t ySrc,
                                          uint32_t u32SrcWidth,
                                          uint32_t u32SrcHeight,
                                          uint32_t u32SrcLineSize,
                                          uint32_t u32SrcBitsPerPixel,
                                          uint8_t *pu8Dst,
                                          int32_t xDst,
                                          int32_t yDst,
                                          uint32_t u32DstWidth,
                                          uint32_t u32DstHeight,
                                          uint32_t u32DstLineSize,
                                          uint32_t u32DstBitsPerPixel)
{
	RTPrintf("%s: %d\n", __FUNCTION__, __LINE__);
	return 0;
}

BOOL XpdmDrvCtrlStart()
{
    INT rc;
    if (gXpdmMonitor.hDC) {
	DeleteDC(gXpdmMonitor.hDC);
    }
    gXpdmMonitor.hDC = CreateDC(NULL,
                                gXpdmMonitor.s_DeviceName,
                                NULL,
                                NULL);

    if (gXpdmMonitor.hDC == NULL) {
	RTPrintf("%s: %d\n", __FUNCTION__, __LINE__);
        EventUnInit(&gXpdmMonitor.events);
	DoDisableVirtualMonitor();
        return FALSE;
    }

    rc = ExtEscape(gXpdmMonitor.hDC,
                   VM_CTL_SET_EVENT,
                   sizeof(EVENT_ARRAY), 
                   (LPSTR)&gXpdmMonitor.events, 
                   0, 
                   NULL);
    if (rc < 0) {
	RTPrintf("%s: %d\n", __FUNCTION__, __LINE__);
        XpdmQuit();
        return FALSE;
    }
    return TRUE;
}


static DWORD XpdmWorkingThread(LPVOID lpParameter)
{
    DWORD dwStatus;
    INT ret;
    INT bufferSize = gXpdmMonitor.pConnector->cbScanline*gXpdmMonitor.pConnector->cy;

    // memset(gXpdmMonitor.pConnector->pu8Data, 0xFF, gXpdmMonitor.pConnector->cbScanline*gXpdmMonitor.pConnector->cy);
    XpdmDrvCtrlStart();
    for (;;) {
        dwStatus = WaitForMultipleObjects(3, (const HANDLE*)&gXpdmMonitor.events, FALSE, INFINITE);	
        if (dwStatus == WAIT_OBJECT_0) {
            ResetEvent(gXpdmMonitor.events.hPaintEvent);
            if (!gXpdmMonitor.fRender) {
                continue;
            }
            ret = ExtEscape(gXpdmMonitor.hDC,
                            VM_CTL_GET_DIRTY_EARA,
                            0, 
                            NULL, 
                            sizeof(gXpdmMonitor.dirtyRect),
                            (LPSTR)&gXpdmMonitor.dirtyRect);

            if (ret < sizeof(gXpdmMonitor.dirtyRect)) {
                continue;
            }
#if 0
            printf("APP: left: %d, right: %d, top: %d, bottom: %d\n",
			gXpdmMonitor.dirtyRect.left, gXpdmMonitor.dirtyRect.right,
                        gXpdmMonitor.dirtyRect.top, gXpdmMonitor.dirtyRect.bottom);
#endif

            ret = ExtEscape(gXpdmMonitor.hDC,
                            VM_CTL_UPDATE_DIRTY_EARA,
                            sizeof(gXpdmMonitor.dirtyRect),
                            (LPSTR)&gXpdmMonitor.dirtyRect,
                            bufferSize,
                            (LPSTR)gXpdmMonitor.frameBuffer);


            if (ret <= 0) {
                continue;
            }
             
            INT offset = (gXpdmMonitor.dirtyRect.left * gXpdmMonitor.pConnector->cBits) >> 3; 
            INT copyLineSize = ((gXpdmMonitor.dirtyRect.right - gXpdmMonitor.dirtyRect.left) * gXpdmMonitor.pConnector->cBits)  >> 3;

            PUCHAR src = gXpdmMonitor.frameBuffer;
            // PCHAR dst = (PCHAR)pvOut + pRect->top*lineSize + offset;
            INT copied = 0;
            PUCHAR dst = (PUCHAR)gXpdmMonitor.pConnector->pu8Data + gXpdmMonitor.dirtyRect.top*gXpdmMonitor.pConnector->cbScanline + offset;
            for (INT y = gXpdmMonitor.dirtyRect.top; y < gXpdmMonitor.dirtyRect.bottom; y++) {
                memcpy(dst, src, copyLineSize); 
                dst += gXpdmMonitor.pConnector->cbScanline;
                src += copyLineSize;
                copied += copyLineSize;
            } 

#if 1
            gXpdmMonitor.pConnector->pfnUpdateRect(gXpdmMonitor.pConnector,
                                                   gXpdmMonitor.dirtyRect.left,
                                                   gXpdmMonitor.dirtyRect.top,
                                                   gXpdmMonitor.dirtyRect.right - gXpdmMonitor.dirtyRect.left,
                                                   gXpdmMonitor.dirtyRect.bottom - gXpdmMonitor.dirtyRect.top);
#else

            gXpdmMonitor.pConnector->pfnUpdateRect(gXpdmMonitor.pConnector,
                                                   0,
                                                   0,
                                                   800,
                                                   600);
#endif
#if 0

            printf("APP: x: %d, y: %d, w: %d, h: %d, %d, %d\n",
			gXpdmMonitor.dirtyRect.left,
                        gXpdmMonitor.dirtyRect.top, 
                        gXpdmMonitor.dirtyRect.right - gXpdmMonitor.dirtyRect.left,
                        gXpdmMonitor.dirtyRect.bottom - gXpdmMonitor.dirtyRect.top, 
                        ret, copied);
#endif
        } else if (dwStatus == WAIT_OBJECT_0 + 1) {
		XpdmDrvCtrlStart();
        } else if (dwStatus == WAIT_OBJECT_0 + 2) {
		printf("Exit\n");
        	XpdmQuit();
        	return 0;
        } else {
        	// DbgPrintf("Status: %x, LastError: %x\n", dwStatus, GetLastError());
        }
    }
    return 0;
}

static DECLCALLBACK(void) xpdmSetRenderVRAM(PPDMIDISPLAYPORT pInterface, bool fRender)
{
    ASMAtomicWriteBool(&gXpdmMonitor.fRender, fRender);
}

BOOL CtrlHandler( DWORD fdwCtrlType ) 
{ 
  switch( fdwCtrlType ) 
  { 
    // Handle the CTRL-C signal. 
    case CTRL_C_EVENT: 
      DisableVirtualMonitor();
      printf( "Ctrl-C event\n\n" );
      return( TRUE );
  }
  return TRUE;
}

DECLCALLBACK(int) XpdmDispProbe(PPDMIDISPLAYPORT pIPort, PPDMIDISPLAYCONNECTOR pConnector)
{
    pIPort->pfnUpdateDisplay       = xpdmUpdateDisplay;
    pIPort->pfnUpdateDisplayAll    = xpdmUpdateDisplayAll;
    pIPort->pfnQueryColorDepth     = xpdmQueryColorDepth;
    pIPort->pfnSetRefreshRate      = xpdmSetRefreshRate;
    pIPort->pfnDisplayBlt          = xpdmDisplayBlt;
    pIPort->pfnUpdateDisplayRect   = xpdmUpdateDisplayRect;
    pIPort->pfnCopyRect            = xpdmCopyRect;
    pIPort->pfnSetRenderVRAM       = xpdmSetRenderVRAM;

    RTPrintf("%s %d\n", __FUNCTION__, __LINE__);
    gXpdmMonitor.pConnector = pConnector;
    if (!XpdmDrvIntfInit()) {
        return -1;
    }
    INT bufferSize = gXpdmMonitor.pConnector->cbScanline*gXpdmMonitor.pConnector->cy;
    gXpdmMonitor.frameBuffer = new UCHAR[bufferSize];
    if (!gXpdmMonitor.frameBuffer) {
        return -1;
    }

    RTPrintf("%s %d\n", __FUNCTION__, __LINE__);
    SetConsoleCtrlHandler((PHANDLER_ROUTINE)CtrlHandler, TRUE);

    gXpdmMonitor.hWorkThread = CreateThread(
                                            NULL,  // default security attributes
                                            0,     // use default stack size
                                            (LPTHREAD_START_ROUTINE)XpdmWorkingThread, // thread function name
                                            NULL,              // argument to thread function
                                            0,              // use default creation flags
                                            &gXpdmMonitor.dwThreadId);

    if (gXpdmMonitor.hWorkThread == NULL) {
        XpdmQuit();
        return -1;
    }

    return 0;
}
