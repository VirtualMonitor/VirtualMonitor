/* $Id: VBoxBFE.cpp $ */
/** @file
 * Basic Frontend (BFE): VBoxBFE main routines.
 *
 * VBoxBFE is a limited frontend that sits directly on the Virtual Machine
 * Manager (VMM) and does _not_ use COM to communicate.
 * VBoxBFE is based on SDL. Much of the code has been copied over from the
 * other frontends in VBox/Main/ and src/Frontends/VBoxSDL/.
 */

/*
 * Copyright (C) 2006-2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_GUI

#ifndef VBOXBFE_WITHOUT_COM
# include <VBox/com/Guid.h>
# include <VBox/com/string.h>
using namespace com;
#endif

#include <VBox/types.h>
#include <VBox/err.h>
#include <VBox/log.h>
#include <VBox/param.h>
#include <VBox/vmm/pdm.h>
#include <VBox/version.h>
#ifdef VBOX_WITH_HGCM
# include <VBox/shflsvc.h>
#endif
#include <iprt/alloc.h>
#include <iprt/alloca.h>
#include <iprt/assert.h>
#include <iprt/ctype.h>
#include <iprt/file.h>
#include <iprt/path.h>
#include <iprt/initterm.h>
#include <iprt/semaphore.h>
#include <iprt/stream.h>
#include <iprt/string.h>
#include <iprt/thread.h>
#include <iprt/uuid.h>

#include "VBoxBFE.h"

#include <stdio.h>
#include <stdlib.h> /* putenv */
#include <errno.h>

#if defined(RT_OS_LINUX)
#include <fcntl.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <linux/if_tun.h>
#endif

#include "ConsoleImpl.h"
#include "DisplayImpl.h"
#include "MouseImpl.h"
#include "KeyboardImpl.h"
#include "VMMDev.h"
#include "StatusImpl.h"
#include "Framebuffer.h"
#include "MachineDebuggerImpl.h"

#if defined(USE_SDL)
#include "SDLConsole.h"
#include "SDLFramebuffer.h"
#endif
#include <VBox/vmm/pdm.h>


#if defined (RT_OS_WINDOWS)
char *evn = "Hello";
extern "C" char **_environ = &evn;
extern DECLCALLBACK(int) XpdmDispProbe(PPDMIDISPLAYPORT pIPort, PPDMIDISPLAYCONNECTOR pConnector);
#endif
extern DECLCALLBACK(int) DummyDispProbe(PPDMIDISPLAYPORT pIPort, PPDMIDISPLAYCONNECTOR pConnector);

struct VirtualMonitorDrvObj {
    PFNDISPDRVCALLBACK pfnDispDrvProbe;
    char *DrvDesc;
};

struct VirtualMonitorDrvObj DrvObj[] = {
#if defined (RT_OS_WINDOWS)
    { XpdmDispProbe, "XPDM Display Driver"},
#endif
    { DummyDispProbe, "Dummy Daemon Display Driver"},
};

/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/

#define VBOXSDL_ADVANCED_OPTIONS
#define MAC_STRING_LEN 12


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static DECLCALLBACK(int) vboxbfeConfigConstructor(PVM pVM, void *pvUser);
static DECLCALLBACK(int) VMPowerUpThread(RTTHREAD Thread, void *pvUser);


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/

// Mouse             *gMouse           = NULL;
Display         *gDisplay         = NULL;
Framebuffer       *gFramebuffer     = NULL;
Console           *gConsole         = NULL;

VMSTATE machineState = VMSTATE_CREATING;

static PPDMLED     mapFDLeds[2]   = {0};

/** flag whether keyboard/mouse events are grabbed */
#if   defined (DEBUG_dmik)
// my mini kbd doesn't have RCTRL...
int                gHostKey    = KMOD_RSHIFT;
int                gHostKeySym = SDLK_RSHIFT;
#else
int                gHostKey    = KMOD_RCTRL;
int                gHostKeySym = SDLK_RCTRL;
#endif
bool gfAllowFullscreenToggle = true;

static bool        g_fIOAPIC = false;
static bool        g_fACPI   = true;
static bool        g_fAudio  = false;
static char       *g_pszHdaFile   = NULL;
static bool        g_fHdaSpf = false;
static char       *g_pszHdbFile   = NULL;
static bool        g_fHdbSpf = false;
static char       *g_pszCdromFile = NULL;
static char       *g_pszFdaFile   = NULL;
       const char *g_pszStateFile = NULL;
static const char *g_pszBootDevice = "IDE";
static uint32_t    g_u32MemorySizeMB = 128;
static uint32_t    g_u32VRamSize = 4 * _1M;
#ifdef VBOXSDL_ADVANCED_OPTIONS
static bool        g_fRawR0 = true;
static bool        g_fRawR3 = true;
static bool        g_fPATM  = true;
static bool        g_fCSAM  = true;
#endif
static bool        g_fRestoreState = false;
static const char *g_pszShareDir[MaxSharedFolders];
static const char *g_pszShareName[MaxSharedFolders];
static bool        g_fShareReadOnly[MaxSharedFolders];
static unsigned    g_uNumShares;
static bool        g_fPreAllocRam = false;
static int         g_iBootMenu = 2;
static bool        g_fReleaseLog = false; /**< Set if we should open the release. */
       const char *g_pszProgressString;
       unsigned    g_uProgressPercent = ~0U;

/** @todo currently this is only set but never read. */
static char szError[512];

/**
 * Print a syntax error.
 *
 * @returns return value for main().
 * @param   pszMsg  The message format string.
 * @param   ...     Format arguments.
 */
static int SyntaxError(const char *pszMsg, ...)
{
    va_list va;
    RTPrintf("error: ");
    va_start(va, pszMsg);
    RTPrintfV(pszMsg, va);
    va_end(va);
    return 1;
}

/**
 * Print a fatal error.
 *
 * @returns return value for main().
 * @param   pszMsg  The message format string.
 * @param   ...     Format arguments.
 */
static int FatalError(const char *pszMsg, ...)
{
    va_list va;
    RTPrintf("fatal error: ");
    va_start(va, pszMsg);
    RTPrintfV(pszMsg, va);
    va_end(va);
    return 1;
}
/**
 * Print program usage.
 */
static void show_usage()
{
    RTPrintf("Usage:\n"
             "  -fullscreen        Start VM in fullscreen mode\n"
             "  -vrdp [port]       Listen for VRDP connections on port (default if not specified)\n"
#ifdef VBOX_SECURELABEL
             "  -securelabel       Display a secure VM label at the top of the screen\n"
             "  -seclabelfnt       TrueType (.ttf) font file for secure session label\n"
             "  -seclabelsiz       Font point size for secure session label (default 12)\n"
#endif
             "\n");
}


extern "C" DECLEXPORT(int) TrustedMain (int argc, char **argv, char **envp)
{
    bool fFullscreen = false;
    int32_t portVRDP = -1;
#ifdef VBOX_SECURELABEL
    bool fSecureLabel = false;
    uint32_t secureLabelPointSize = 12;
    char *secureLabelFontFile = NULL;
#endif
    int rc = VINF_SUCCESS;

    RTPrintf(VBOX_PRODUCT " Simple SDL GUI built %s %s\n", __DATE__, __TIME__);

    /*
     * Parse the command line arguments.
     */
    for (int curArg = 1; curArg < argc; curArg++)
    {
        const char * const pszArg = argv[curArg];
        if (strcmp(pszArg, "-fullscreen") == 0)
            fFullscreen = true;
        else if (strcmp(pszArg, "-vrdp") == 0)
        {
            // -vrdp might take a port number (positive).
            portVRDP = 0;       // indicate that it was encountered.
            if (curArg + 1 < argc && argv[curArg + 1][0] != '-')
            {
                rc = RTStrToInt32Ex(argv[curArg], NULL, 0, &portVRDP);
                if (RT_FAILURE(rc))
                    return SyntaxError("cannot vrpd port: %s (%Rrc)\n", argv[curArg], rc);
                if (portVRDP < 0 || portVRDP >= 0x10000)
                    return SyntaxError("vrdp port number is out of range: %RI32\n", portVRDP);
            }
        }
#ifdef VBOX_SECURELABEL
        else if (strcmp(pszArg, "-securelabel") == 0)
        {
            fSecureLabel = true;
            LogFlow(("Secure labelling turned on\n"));
        }
        else if (strcmp(pszArg, "-seclabelfnt") == 0)
        {
            if (++curArg >= argc)
                return SyntaxError("missing font file name for secure label!\n");
            secureLabelFontFile = argv[curArg];
        }
        else if (strcmp(pszArg, "-seclabelsiz") == 0)
        {
            if (++curArg >= argc)
                return SyntaxError("missing font point size for secure label!\n");
            secureLabelPointSize = atoi(argv[curArg]);
        }
#endif
        else
        {
            SyntaxError("unrecognized argument '%s'\n", pszArg);
            show_usage();
            return 1;
        }
    }

/*
    gMouse = new Mouse();
    if (FAILED(gMouse->FinalConstruct()))
        goto leave;
*/
#if defined(USE_SDL)
    /* First console, then framebuffer!! */
    gConsole = new SDLConsole();
    gFramebuffer = new SDLFramebuffer();
#else
#error "todo"
#endif
    gDisplay = new Display();
    if (!gConsole->initialized())
        goto leave;
    
    gDisplay->SetFramebuffer(0, gFramebuffer);
    gConsole->Init(gDisplay);
    for (int i = 0; i < RT_ELEMENTS(DrvObj); i++) {
        rc = gDisplay->ConnectToDrv(DrvObj[i].pfnDispDrvProbe);
        RTPrintf("Probe %s %s\n", DrvObj[i].DrvDesc, RT_FAILURE(rc) ? "Failed" : "Successful");
	if (!RT_FAILURE(rc)) 
            break;
    }

    // Dummy Display Driver will always successful.

    /* start with something in the titlebar */
    gConsole->updateTitlebar();

    /*
     * Start the VM execution thread. This has to be done
     * asynchronously as powering up can take some time
     * (accessing devices such as the host DVD drive). In
     * the meantime, we have to service the SDL event loop.
     */

    RTTHREAD thread;
    rc = RTThreadCreate(&thread, VMPowerUpThread, 0, 0, RTTHREADTYPE_MAIN_WORKER, 0, "PowerUp");
    if (RT_FAILURE(rc))
    {
        RTPrintf("Error: Thread creation failed with %d\n", rc);
        return -1;
    }


    /* loop until the powerup processing is done */
    do
    {
#if defined(VBOXBFE_WITH_X11) && defined(USE_SDL)
        if (   machineState == VMSTATE_CREATING
            || machineState == VMSTATE_LOADING)
        {
            int event = gConsole->eventWait();

            switch (event)
            {
            case CONEVENT_USR_SCREENRESIZE:
                LogFlow(("CONEVENT_USR_SCREENRESIZE\n"));
                gFramebuffer->resize();
                /* notify the display that the resize has been completed */
                gDisplay->ResizeCompleted();
                break;

            case CONEVENT_USR_TITLEBARUPDATE:
                gConsole->updateTitlebar();
                break;

            case CONEVENT_USR_QUIT:
                RTPrintf("Error: failed to power up VM! No error text available.\n");
                goto leave;
            }
        }
        else
#endif
            RTThreadSleep(1000);
    }
    while (   machineState == VMSTATE_CREATING
           || machineState == VMSTATE_LOADING);

    if (machineState == VMSTATE_TERMINATED)
        goto leave;

    /* did the power up succeed? */
    if (machineState != VMSTATE_RUNNING)
    {
        RTPrintf("Error: failed to power up VM! No error text available (rc = 0x%x state = %d)\n", rc, machineState);
        goto leave;
    }

    gConsole->updateTitlebar();

    /*
     * Main event loop
     */
    LogFlow(("VBoxSDL: Entering big event loop\n"));

    while (1)
    {
        int event = gConsole->eventWait();

        // RTPrintf("%s: %d, event: %x, %d %d %d %d\n", __FUNCTION__, __LINE__, event, cbScanline, cBits, cx, cy);
        switch (event)
        {
        case CONEVENT_NONE:
            break;

        case CONEVENT_QUIT:
        case CONEVENT_USR_QUIT:
            goto leave;

        case CONEVENT_SCREENUPDATE:
            /// @todo that somehow doesn't seem to work!
            gFramebuffer->repaint();
            break;

        case CONEVENT_USR_TITLEBARUPDATE:
            gConsole->updateTitlebar();
            break;

        case CONEVENT_USR_SCREENRESIZE:
        {
            LogFlow(("CONEVENT_USR_SCREENRESIZE\n"));
            gFramebuffer->resize();
            /* notify the display that the resize has been completed */
            gDisplay->ResizeCompleted();
            break;
        }

#ifdef VBOX_SECURELABEL
        case CONEVENT_USR_SECURELABELUPDATE:
        {
           /*
             * Query the new label text
             */
            Bstr key = VBOXSDL_SECURELABEL_EXTRADATA;
            Bstr label;
            gMachine->COMGETTER(ExtraData)(key, label.asOutParam());
            Utf8Str labelUtf8 = label;
            /*
             * Now update the label
             */
            gFramebuffer->setSecureLabelText(labelUtf8.raw());
            break;
        }
#endif /* VBOX_SECURELABEL */

        }

    }

leave:
    LogFlow(("Returning from main()!\n"));


    delete gFramebuffer;
    delete gConsole;
    delete gDisplay;
    // gMouse->FinalRelease();
    // delete gMouse;

    RTLogFlush(NULL);
    return RT_FAILURE (rc) ? 1 : 0;
}

#ifndef VBOX_WITH_HARDENING
/**
 * Main entry point.
 */
int main(int argc, char **argv)
{

    /*
     * Before we do *anything*, we initialize the runtime.
     */
    int rc = RTR3InitExe(argc, &argv, 0);
    if (RT_FAILURE(rc))
        return FatalError("RTR3InitExe failed rc=%Rrc\n", rc);

    return TrustedMain(argc, argv, NULL);
}
#endif /* !VBOX_WITH_HARDENING */

/** VM asynchronous operations thread */
DECLCALLBACK(int) VMPowerUpThread(RTTHREAD Thread, void *pvUser)
{
    int rc = VINF_SUCCESS;
    int rc2;

    /*
     * Setup the release log instance in current directory.
     */
    if (g_fReleaseLog)
    {
        static const char * const s_apszGroups[] = VBOX_LOGGROUP_NAMES;
        static char s_szError[RTPATH_MAX + 128] = "";
        PRTLOGGER pLogger;
        rc2 = RTLogCreateEx(&pLogger, RTLOGFLAGS_PREFIX_TIME_PROG, "all",
                            "VBOX_RELEASE_LOG", RT_ELEMENTS(s_apszGroups), s_apszGroups, RTLOGDEST_FILE,
                            NULL /* pfnBeginEnd */, 0 /* cHistory */, 0 /* cbHistoryFileMax */, 0 /* uHistoryTimeMax */,
                            s_szError, sizeof(s_szError), "./VBoxBFE.log");
        if (RT_SUCCESS(rc2))
        {
            /* some introductory information */
            RTTIMESPEC TimeSpec;
            char szNowUct[64];
            RTTimeSpecToString(RTTimeNow(&TimeSpec), szNowUct, sizeof(szNowUct));
            RTLogRelLogger(pLogger, 0, ~0U,
                           "VBoxBFE %s (%s %s) release log\n"
                           "Log opened %s\n",
                           VBOX_VERSION_STRING, __DATE__, __TIME__,
                           szNowUct);

            /* register this logger as the release logger */
            RTLogRelSetDefaultInstance(pLogger);
        }
        else
            RTPrintf("Could not open release log (%s)\n", s_szError);
    }

    /*
     * Start VM (also from saved state) and track progress
     */
    LogFlow(("VMPowerUp\n"));
    machineState = VMSTATE_RUNNING;
    /*
     * On failure destroy the VM.
     */
    if (RT_FAILURE(rc))
        goto failure;

    return 0;

failure:
    machineState = VMSTATE_TERMINATED;

    return 0;
}
