#include <VBox/err.h>
#include <VBox/log.h>
#include <iprt/alloc.h>
#include <iprt/alloca.h>
#include <iprt/ctype.h>
#include <iprt/file.h>
#include <iprt/path.h>
#include <iprt/initterm.h>
#include <iprt/semaphore.h>
#include <iprt/stream.h>
#include <iprt/string.h>
#include <iprt/thread.h>
#include <iprt/net.h>

#include "DrvIntf.h"
#include "VirtualMonitor.h"
#include "Display.h"
#include <stdio.h>

#if defined (RT_OS_WINDOWS)
#include <windows.h>
#include <Shlobj.h>
char *evn = "Hello";
extern "C" char **_environ = &evn;
extern DrvIntf *XpdmDrvProbe(DisplayParam &param);
#endif
extern DrvIntf *DummyDrvProbe(DisplayParam &param);

struct VirtualMonitorDrvObj DrvObj[] = {
#if defined (RT_OS_WINDOWS)
    { XpdmDrvProbe, "XPDM Display Driver"},
#endif
    { DummyDrvProbe, "Dummy Display Driver"},
};


extern Display *VNCDisplayProbe(DisplayParam &param, char *videoMemory);
struct DisplayIntfObj displayIntf[] = {
    { VNCDisplayProbe, "VNC Display"},
};

#define FULL_REFRESH_CYCLE 10


static DrvIntf *drvIntfObj = NULL;
#define MAX_DISPLAY_INTF 3
static Display *displayIntfObj[MAX_DISPLAY_INTF];
static int displayIntfCnt = 0;

#if defined (RT_OS_WINDOWS)
static BOOL CtrlHandler(DWORD fdwCtrlType) 
{ 
  switch (fdwCtrlType) 
  { 
    // Handle the CTRL-C signal. 
    case CTRL_C_EVENT: 
      drvIntfObj->Disable();
      // RTPrintf( "Ctrl-C event\n\n" );
      return( TRUE );
  }
  return TRUE;
}
#endif

static bool IsSupport()
{
	bool ret = true;
#if defined (RT_OS_WINDOWS)
	OSVERSIONINFOEX osvi;
	bool isVista = false;
	bool isWin7 = false;
	ZeroMemory(&osvi, sizeof(OSVERSIONINFOEX));
	osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
	GetVersionEx((LPOSVERSIONINFO)&osvi);

    if (osvi.dwMajorVersion == 5) {
			ret = true;
    } else if (osvi.dwMajorVersion == 6) {
		if (osvi.dwMinorVersion == 0) {
			isVista = true;
			ret = true;
		}
		if (osvi.dwMinorVersion == 1) {
			isWin7 = true;
			ret = true;
		}
	}
	if (isVista || isWin7) {
		if (!IsUserAnAdmin()) {
			printf("Access Denied. Administrator permissions are needed to use the selected options.");
			printf("Use an administrator command prompt to complete these tasks.");
			return false;
		}
	}
#endif
	return ret;
}

int VirtualMonitorMain(DisplayParam cmd)
{
	int i = 0;
	int total = RT_ELEMENTS(DrvObj);
	char *videoMemory;
	if (cmd.enableDummyDriver) {
		i = RT_ELEMENTS(DrvObj) - 1;
	} else {
		// dummy Driver disabled, skip
		total -= 1;
		if (!IsSupport()) {
			return -1;
		}
	}
	do {
        drvIntfObj = DrvObj[i].pfnDispDrvProbe(cmd);
        RTPrintf("Probe %s %s\n", DrvObj[i].DrvDesc, drvIntfObj ? "Successful" : "Failed");
        if (drvIntfObj) {
            break;
        }
		i++;
    } while (i < total);
	if (!drvIntfObj) {
		return -1;
	}

#if defined (RT_OS_WINDOWS)
    SetConsoleCtrlHandler((PHANDLER_ROUTINE)CtrlHandler, TRUE);
#endif
    if (drvIntfObj->SetDisplayMode(cmd.x, cmd.y, cmd.bpp)) {
		RTPrintf("can't support Display mode:[%dx%dx%d]\n", cmd.x, cmd.y, cmd.bpp);
		goto unsupport;
	}
    drvIntfObj->Enable();
	videoMemory = drvIntfObj->GetVideoMemory();

    Display *disp;
    for (int i = 0; i < RT_ELEMENTS(displayIntf); i++) {
        disp = displayIntf[i].pfnDispIntfProbe(cmd, videoMemory);
        RTPrintf("Create %s %s %p\n", displayIntf[i].IntfDesc, disp ? "Successful" : "Failed", videoMemory);
        if (disp) {
            displayIntfObj[displayIntfCnt] = disp;
            displayIntfCnt++;
            disp->Start();
			if (!videoMemory) {
				drvIntfObj->CopyDirtyPixels(disp->pPixels,
											0,
											0,
											cmd.x,
											cmd.y);
			}
            disp->Update(0, 0, cmd.x, cmd.y);
        }
    }
    RTPrintf("%d Display Intf Object\n", displayIntfCnt);
	DWORD counter = 0;
    Event evt;
    while (drvIntfObj->GetEvent(evt) == 0) {
        if (evt.code == EVENT_DIRTY_AREA || evt.code == EVENT_TIMEOUT ) {
		
		if ( (evt.code == EVENT_TIMEOUT) ||
			((++counter) % FULL_REFRESH_CYCLE == 0))
		{
			evt.dirtyArea.left = 0;
			evt.dirtyArea.top = 0;
			evt.dirtyArea.right = cmd.x;
			evt.dirtyArea.bottom = cmd.y;
			counter = 0;
		}
#if 0
            RTPrintf("Dirty: lef: %d, top: %d, right: %d bottom: %d\n", 
                    evt.dirtyArea.left,
                    evt.dirtyArea.top,
                    evt.dirtyArea.right,
                    evt.dirtyArea.bottom);
#endif
            for (int i = 0; i < displayIntfCnt; i++) {
                disp = displayIntfObj[i];

				if (!videoMemory) {
					drvIntfObj->CopyDirtyPixels(disp->pPixels,
												evt.dirtyArea.left,
												evt.dirtyArea.top,
												evt.dirtyArea.right,
												evt.dirtyArea.bottom);
				}
                disp->Update(evt.dirtyArea.left,
                             evt.dirtyArea.top,
                             evt.dirtyArea.right,
                             evt.dirtyArea.bottom);
            }
        } 
		else if (evt.code == EVENT_QUIT) {
            RTPrintf("Quit\n");
            break;
        }
    }

    drvIntfObj->Disable();
unsupport:

    for (int i = 0; i < displayIntfCnt; i++) {
        disp = displayIntfObj[i];
        disp->Stop();
        delete disp;
    }

    delete drvIntfObj;
    return 0;
}

