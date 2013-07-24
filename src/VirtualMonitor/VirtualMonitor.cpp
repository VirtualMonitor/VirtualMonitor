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

#if defined (RT_OS_WINDOWS)
#include <windows.h>
char *evn = "Hello";
extern "C" char **_environ = &evn;
extern DrvIntf *XpdmDrvProbe();
#endif
extern DrvIntf *DummyDrvProbe();

struct VirtualMonitorDrvObj DrvObj[] = {
#if defined (RT_OS_WINDOWS)
    { XpdmDrvProbe, "XPDM Display Driver"},
#endif
    { DummyDrvProbe, "Dummy Display Driver"},
};


extern Display *VNCDisplayProbe(DisplayParam &param);
struct DisplayIntfObj displayIntf[] = {
    { VNCDisplayProbe, "VNC Display"},
};

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
      RTPrintf( "Ctrl-C event\n\n" );
      return( TRUE );
  }
  return TRUE;
}
#endif

int VirtualMonitorMain(DisplayParam cmd)
{
    for (int i = 0; i < RT_ELEMENTS(DrvObj); i++) {
        drvIntfObj = DrvObj[i].pfnDispDrvProbe();
        RTPrintf("Probe %s %s\n", DrvObj[i].DrvDesc, drvIntfObj ? "Successful" : "Failed");
        if (drvIntfObj) {
            break;
        }
    }

#if defined (RT_OS_WINDOWS)
    SetConsoleCtrlHandler((PHANDLER_ROUTINE)CtrlHandler, TRUE);
#endif
    if (drvIntfObj->SetDisplayMode(cmd.x, cmd.y, cmd.bpp)) {
		RTPrintf("can't support Display mode:[%dx%dx%d]\n", cmd.x, cmd.y, cmd.bpp);
		goto unsupport;
	}
    drvIntfObj->Enable();

    Display *disp;
    for (int i = 0; i < RT_ELEMENTS(displayIntf); i++) {
        disp = displayIntf[i].pfnDispIntfProbe(cmd);
        RTPrintf("Create %s %s\n", displayIntf[i].IntfDesc, disp ? "Successful" : "Failed");
        if (disp) {
            displayIntfObj[displayIntfCnt] = disp;
            displayIntfCnt++;
            disp->Start();
            drvIntfObj->CopyDirtyPixels(disp->pPixels,
                                        0,
                                        0,
                                        cmd.x,
                                        cmd.y);
            disp->Update(0, 0, cmd.x, cmd.y);
        }
    }
    RTPrintf("%d Display Intf Object\n", displayIntfCnt);

    Event evt;
    while (drvIntfObj->GetEvent(evt) == 0) {
        if (evt.code == EVENT_DITRY_AREA) {
#if 0
            RTPrintf("Dirty: lef: %d, top: %d, right: %d bottom: %d\n", 
                    evt.dirtyArea.left,
                    evt.dirtyArea.top,
                    evt.dirtyArea.right,
                    evt.dirtyArea.bottom);
#endif
            for (int i = 0; i < displayIntfCnt; i++) {
                disp = displayIntfObj[i];
                drvIntfObj->CopyDirtyPixels(disp->pPixels,
                                            evt.dirtyArea.left,
                                            evt.dirtyArea.top,
                                            evt.dirtyArea.right,
                                            evt.dirtyArea.bottom);
                disp->Update(evt.dirtyArea.left,
                             evt.dirtyArea.top,
                             evt.dirtyArea.right,
                             evt.dirtyArea.bottom);

            }
        } else if (evt.code == EVENT_QUIT) {
            RTPrintf("Quit\n");
            break;
        }
    }

    drvIntfObj->Disable();
unsupport:
    delete drvIntfObj;

    for (int i = 0; i < displayIntfCnt; i++) {
        disp = displayIntfObj[i];
        disp->Stop();
        delete disp;
    }

    return 0;
}

