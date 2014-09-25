#include "XpdmDrvIntf.h"
#include "Common.h"

LPSTR XpdmDrvIntf::GetDispCode(INT code)
{
    static char tmp[MAX_PATH];
    static LPSTR dispCode[7] = {
       "Change Successful",
       "Must Restart",
       "Bad Flags",
       "Bad Parameters",
       "Failed",
       "Bad Mode",
       "Not Updated"
    };

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



BOOL XpdmDrvIntf::OpenDeviceRegistryKey()
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
        return NULL;
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
                            KEY_QUERY_VALUE,
                            &hSubKey);
        if (rc != ERROR_SUCCESS) {
            // printf("Open Reg Sub Key Failed: %s %x\n", video, GetLastError());
			if (dwIdIndex < 1024) {
				continue;
			} else {
            	goto OpenSubKeyFailed;
			}
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
        		RegCloseKey(hSubKey);
				// ReOpen for ALL_ACCESS
        		rc = RegOpenKeyEx(hRegRoot,
                            video,
                            0,
                            KEY_ALL_ACCESS,
                            &hSubKey);
        		if (rc != ERROR_SUCCESS) {
					goto OpenSubKeyFailed;
				}
                this->hKey = hSubKey;
                RegCloseKey(hRegRoot);
                return TRUE;
            }
        }
        RegCloseKey(hSubKey);
    } while (query != ERROR_NO_MORE_ITEMS);
    RegCloseKey(hRegRoot);

OpenSubKeyFailed:
    RegCloseKey(hRegRoot);
    return FALSE;
}

int XpdmDrvIntf::SetDisplayMode(uint32_t xRes, uint32_t yRes, uint32_t bpp)
{
    LONG rc;
    int ret = 0;

    if (!xRes || !yRes || !bpp) {
        return -1;
    }
    this->xRes = xRes; 
    this->yRes = yRes; 
    this->bpp = bpp; 

    pixelsLen = xRes * yRes * ((bpp+7)/8);
    rc = RegSetValueEx(hKey, 
            "CustomMode0Width",
            0,
            REG_BINARY,
            (const PBYTE)&xRes,
            sizeof(xRes));  
    if (rc != ERROR_SUCCESS) {
        printf("Set xRes Failed:%x\n", GetLastError());
        ret = -1;
    }

    rc = RegSetValueEx(hKey,
            "CustomMode0Height",
            0,
            REG_BINARY,
            (const PBYTE)&yRes,
            sizeof(yRes));
    if (rc != ERROR_SUCCESS) {
        printf("Set yRes Failed:%x\n", GetLastError());
        ret = -1;
    }
    rc = RegSetValueEx(hKey,
            "CustomMode0BPP",
            0,
            REG_BINARY,
            (const PBYTE)&bpp,
            sizeof(bpp));
    if (rc != ERROR_SUCCESS) {
        printf("Set bpp Failed:%x, %x, %x\n", GetLastError(), rc, ERROR_SUCCESS);
        ret = -1;
    }
#if 0
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
#endif
    return ret;
}

BOOL XpdmDrvIntf::FindDeviceName()
{
    INT devNum = 0;
    DISPLAY_DEVICE displayDevice;
    BOOL result;
    BOOL bFound = FALSE;

    FillMemory(&displayDevice, sizeof(DISPLAY_DEVICE), 0);
    displayDevice.cb = sizeof(DISPLAY_DEVICE);

    // First enumerate for Primary display device:
    while ((result = EnumDisplayDevices(NULL, devNum, &displayDevice, 0))) {
#if 0
		printf("%s, name: %s\n\tid: %s\n\t key: %s\n",
				&displayDevice.DeviceString[0],
				&displayDevice.DeviceName[0],
				&displayDevice.DeviceID[0],
				&displayDevice.DeviceKey[0]);
#endif
        if (strcmp(&displayDevice.DeviceString[0], DRIVER_NAME) == 0) {
            bFound = TRUE;
            memcpy(&deviceName[0], (LPSTR)&displayDevice.DeviceName[0], sizeof(displayDevice.DeviceName));
            printf("%s\n", deviceName);
            break;  
        }
        devNum++;
    }
    return bFound;
}

int XpdmDrvIntf::Enable()
{
    DEVMODE defaultMode;
    HDC hdc;
    int nWidth;
    INT code;
    DWORD dwFlags = 0;

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
    EnumDisplaySettings(deviceName, 0xffffff, &defaultMode);

    ZeroMemory(&defaultMode, sizeof(DEVMODE));
    defaultMode.dmSize = sizeof(DEVMODE);
        defaultMode.dmDriverExtra = 0;

    if (!EnumDisplaySettings(deviceName,
             ENUM_REGISTRY_SETTINGS, &defaultMode)) {
        printf("Device: %s\n", deviceName);
        return -1; // Store default failed
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

    code = ChangeDisplaySettingsEx(deviceName, 
                    &defaultMode,
                    NULL,
                    CDS_NORESET | CDS_UPDATEREGISTRY,
                    NULL); 


    // printf("Update Registry on device mode: %s\n", GetDispCode(code));
    code = ChangeDisplaySettingsEx(NULL,
                    NULL,
                    NULL,
                    0,
                    NULL);
    // printf("Raw dynamic mode change on device mode: %s\n", GetDispCode(code));

    if (!XpdmDrvCtrlReStart()) {
        Disable();
        return -1;
    }
    return 0;
}

int XpdmDrvIntf::Disable()
{
    DEVMODE defaultMode;
    INT code;

	if (pVideoMemory) {
		ExtEscape(hDC,
				   VM_CTL_UNMAP_VIDEO_MEMORY,
                   sizeof(pVideoMemory), 
                   (LPSTR)&pVideoMemory, 
                   0, 
                   NULL);
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

    code = ChangeDisplaySettingsEx(deviceName, 
                    &defaultMode,
                    NULL,
                    CDS_NORESET | CDS_UPDATEREGISTRY,
                    NULL); 


    // printf("Update Registry on device mode: %s\n", GetDispCode(code));
    code = ChangeDisplaySettingsEx(NULL,
                    NULL,
                    NULL,
                    0,
                    NULL);
    // printf("Raw dynamic mode change on device mode: %s\n", GetDispCode(code));

#if 1
    SetEvent(events.event[2]);
#else
    HANDLE e;   
    e = OpenEvent(EVENT_MODIFY_STATE, 1, QUIT_EVENT_NAME);
    if (e == NULL) {
        printf("Quit Event Failed: %d\n", GetLastError());
        return FALSE;
    }
    SetEvent(e);
    CloseHandle(e);
#endif

	AeroCtrl(TRUE);
    return 0;
}


BOOL XpdmDrvIntf::XpdmDrvCtrlReStart()
{
    INT rc;
    if (hDC) {
        DeleteDC(hDC);
    }

    hDC = CreateDC(NULL, deviceName, NULL, NULL);
    if (hDC == NULL) {
        return FALSE;
    }
    rc = ExtEscape(hDC,
                   VM_CTL_SET_EVENT,
                   sizeof(events.event), 
                   (LPSTR)&events.event, 
                   0, 
                   NULL);
    if (rc < 0) {
        return FALSE;
    }
    return TRUE;
}

int XpdmDrvIntf::GetEvent(Event &evt)
{
    DWORD dwStatus;
    INT ret;

    for (;;) {
        dwStatus = WaitForMultipleObjects(sizeof(events.event)/sizeof(events.event[0]),
                                          (const HANDLE*)&events.event,
                                          FALSE,
                                          500); 
        if (dwStatus == WAIT_OBJECT_0) {
            ret = ExtEscape(hDC, VM_CTL_GET_DIRTY_EARA, 0, NULL, 
                        sizeof(dirtyRect),
                        (LPSTR)&dirtyRect);

            if (ret < sizeof(dirtyRect)) {
                continue;
            }
			if (!pVideoMemory) {
				ret = ExtEscape(hDC,
							VM_CTL_UPDATE_DIRTY_EARA,
							sizeof(dirtyRect),
							(LPSTR)&dirtyRect,
							pixelsLen,
							(LPSTR)pPixels);
				if (ret <= 0) {
					continue;
				}
			}
            evt.code = EVENT_DIRTY_AREA;
            evt.dirtyArea.left = dirtyRect.left;
            evt.dirtyArea.top = dirtyRect.top;
            evt.dirtyArea.right = dirtyRect.right;
            evt.dirtyArea.bottom = dirtyRect.bottom;
            return 0;
        } else if (dwStatus == WAIT_OBJECT_0 + 1) {
            // Restart
            ResetEvent(events.event[0]);
            XpdmDrvCtrlReStart();
            continue;
        } else if (dwStatus == WAIT_OBJECT_0 + 2) {
            // Quit
            evt.code = EVENT_QUIT;
            return 0;
        } 
		else if (dwStatus == WAIT_TIMEOUT) {
		    evt.code = EVENT_TIMEOUT;
			return 0;
		}
		else {
			return -1;
		}
    }
    return 0;
}

int XpdmDrvIntf::ForceUpDate(uint32_t left, uint32_t top, uint32_t right, uint32_t bottom)
{
    int ret;
    RECT d;
    d.left = left;
    d.top = top;
    d.right = right;
    d.bottom = bottom;

    ret = ExtEscape(hDC,
                    VM_CTL_UPDATE_DIRTY_EARA,
                    sizeof(d),
                    (LPSTR)&d,
                    pixelsLen,
                    (LPSTR)pPixels);

    return ret;
}

int XpdmDrvIntf::CopyDirtyPixels(uint8_t *dst, uint32_t left, uint32_t top, uint32_t right, uint32_t bottom)
{
    if (dirtyRect.left != left ||
        dirtyRect.top != top ||
        dirtyRect.right != right ||
        dirtyRect.bottom != bottom) {
        ForceUpDate(left, top, right, bottom);
    }
    INT Bpp = ((bpp+7)/8);
    INT offset = (left * Bpp); 
    INT copyLineSize = (right - left) * Bpp;
    INT LineSize = xRes*Bpp;

    PUCHAR src = pPixels;
    INT copied = 0;
    PUCHAR dest = (PUCHAR)dst + top*LineSize+offset;
    for (uint32_t y = top; y < bottom; y++) {
        memcpy(dest, src, copyLineSize); 
        dest += LineSize;
        src += copyLineSize;
        copied += copyLineSize;
    } 

    return 0;
}

XpdmDrvIntf::XpdmDrvIntf()
{
    hDC = NULL;
    pPixels = NULL;
    pixelsLen = 0;
}

XpdmDrvIntf::~XpdmDrvIntf()
{
    RegCloseKey(hKey);
    if (pPixels) 
        delete pPixels;
}

int XpdmDrvIntf::AeroCtrl(BOOL enable)
{
	int ret = -1;
	BOOL isEnabled = TRUE;
	HMODULE dwmapi;

	typedef HRESULT (CALLBACK *PfnDwmEnableComposition)(BOOL   fEnable);
	PfnDwmEnableComposition pfnDwmEnableComposition;
	typedef HRESULT (CALLBACK *PfnDwmIsCompositionEnabled)(BOOL *pfEnabled);
	PfnDwmIsCompositionEnabled pfnDwmIsCompositionEnabled = NULL;

	if (osvi.dwMajorVersion >= 6) {
		dwmapi = LoadLibrary("dwmapi.dll");
		if (dwmapi != NULL) {
			pfnDwmIsCompositionEnabled = (PfnDwmIsCompositionEnabled)GetProcAddress(dwmapi, "DwmIsCompositionEnabled");
			pfnDwmEnableComposition = (PfnDwmEnableComposition)GetProcAddress(dwmapi, "DwmEnableComposition");
			if (pfnDwmIsCompositionEnabled) {
				pfnDwmIsCompositionEnabled(&isEnabled);
			}
			ret = 0;
			// printf("Aero: %d, new: %d\n", isEnabled, enable);
			if (isEnabled != enable) {
				if (pfnDwmEnableComposition) {
					pfnDwmEnableComposition(enable);
				}
			}
			FreeLibrary(dwmapi);
		}
	} else {
		ret = 0;
	}
	return ret;
}

char *XpdmDrvIntf::GetVideoMemory()
{
    LONG rc;
    rc = ExtEscape(hDC,
                   VM_CTL_MAP_VIDEO_MEMORY,
                   0, 
                   NULL, 
                   sizeof(pVideoMemory), 
                   (LPSTR)&pVideoMemory);
    if (rc < 0) {
        printf("%s: %d\n", __FUNCTION__, __LINE__);
        return NULL;
    }
	if (!pVideoMemory) {
    	pPixels = new uint8_t[pixelsLen];
    	Assert(pPixels);
    }

	return (char*)pVideoMemory;
}


int XpdmDrvIntf::Init(DisplayParam &param)
{
	HDEVINFO h = NULL;
	SP_DEVINFO_DATA dev_info_data;
	ULONG status = 0, problem = 0;

    if (!FindDeviceName()) {
		h = GetDevInfoFromDeviceId(&dev_info_data, "VirtualMonitorVideo\0\0\0");
		if (!h) {
			printf("Driver Not installed\n");
		} else {
			GetDevStatus(h, &dev_info_data, &status, &problem);
			printf("Driver report status: %08x, problem: %08x\n", status, problem);
			DestroyDevInfo(h);
		}
        return -1;
    }
    if (!OpenDeviceRegistryKey()) {
        printf("Can't find %s in Registery\n", DRIVER_NAME);
        return -1;
    }

 	ZeroMemory(&osvi, sizeof(osvi));
	osvi.dwOSVersionInfoSize = sizeof(osvi);
	GetVersionEx(&osvi);

	AeroCtrl(FALSE);
    return 0;
}

DrvIntf *XpdmDrvProbe(DisplayParam &param)
{
    XpdmDrvIntf *xpdm = new XpdmDrvIntf;
    Assert(xpdm);
    if (xpdm->Init(param)) {
        delete xpdm;
        return NULL;
    }
    return xpdm;
}
