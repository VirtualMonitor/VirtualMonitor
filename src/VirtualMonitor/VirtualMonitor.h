#ifndef __VIRTUAL_MONITOR_H__
#define __VIRTUAL_MONITOR_H__

#include <VBox/types.h>

#include "DrvIntf.h"
#include "Display.h"

typedef DECLCALLBACK(DrvIntf *)  FNDRVPROBE(DisplayParam &param);
/** Pointer to a FNPDMDRVCONSTRUCT() function. */
typedef FNDRVPROBE *PFNDRVPROBE;

struct VirtualMonitorDrvObj {
    PFNDRVPROBE pfnDispDrvProbe;
    char *DrvDesc;
};

typedef DECLCALLBACK(VDisplay *)  FNDISPLAYPROBE(DisplayParam &param, char *videoMemory);
/** Pointer to a FNPDMDRVCONSTRUCT() function. */
typedef FNDISPLAYPROBE *PFNDISPLAYPROBE;

struct DisplayIntfObj {
    PFNDISPLAYPROBE pfnDispIntfProbe;
    char *IntfDesc;
};



#endif /* __VIRTUAL_MONITOR_H__ */
