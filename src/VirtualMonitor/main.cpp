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
#include "product-generated.h"
#include "version-generated.h"
#include "Common.h"
#include <stdio.h>

#include "Display.h"
static DisplayParam cmdParam;
#define RTPrintf( x, ...) fprintf(stderr, (x), __VA_ARGS__)
void Usage()
{
    RTPrintf("-x Number\t specify x resolution\n");
    RTPrintf("-y Number\t specify y resolution\n");
	RTPrintf("-log [filename] specify a filename for logging\n");
    RTPrintf("-bpp Number\t specify bit per pixel, currently only support 32bit true color\n");
    RTPrintf("-a4 IPv4 address\t listen on specified IPv4 address only\n");
    RTPrintf("-p4 Number\t listen on specify IPv4 port\n");
    RTPrintf("-a6 IPv6\t address. listen on specified IPv4 address only\n");
    RTPrintf("-p6 Number\t listen on specify IPv4 port\n");
    RTPrintf("-dummy\t use dummy driver, this option is for developer\n");
    RTPrintf("-tf filename\t use filename as input pixel, this option is for developer\n");
    RTPrintf("-v \t show version information\n");
    RTPrintf("-h \t show this help message\n");
	RTPrintf("\n example\n");
	RTPrintf("VirtualMonitor -x 300 -y 400\n");
	RTPrintf("it will create a virtual monitor with 300*400*32.\n");
	RTPrintf("And will listen on all interface with useable port\n");
	RTPrintf("5800 is the default listening port, if it is was using by other program.\n");
	RTPrintf("Then it will choose another port.\n");
}

void dump_cmd(DisplayParam *cmd)
{
    RTPrintf("x: %d, y: %d, bpp: %d\n", cmd->x, cmd->y, cmd->bpp);
    RTPrintf("IPv4: %s: %d\n", cmd->net.ipv4Addr, cmd->net.ipv4Port);
    RTPrintf("IPv6: %s: %d\n", cmd->net.ipv6Addr, cmd->net.ipv6Port);
}

int decode_cmd(int argc, char **argv)
{
    int i, i1;
    memset(&cmdParam, 0, sizeof(cmdParam));
	RTPrintf("decode_cmd Arguments %d\n", argc);
    for (i = i1 = 1; i < argc; i++) {
		RTPrintf("\nArguments[%d]=%s", i, argv[i]);
		if ( (i+1) < argc )
		{
			RTPrintf("\nArguments[%d]=%s", i+1, argv[i+1]);
		}
        if (strcmp(argv[i], "-h") == 0) {
			// show help message
			return -1;
		}
        if (strcmp(argv[i], "-x") == 0) { /* X resolution */
            if (i + 1 >= argc) {
                return -1;
            }
            cmdParam.x = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-y") == 0) { /* X resolution */
            if (i + 1 >= argc) {
                return -1;
            }
            cmdParam.y = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-bpp") == 0) { /* bit per pixel */
            if (i + 1 >= argc) {
                return -1;
            }
            cmdParam.bpp = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-p6") == 0) { /* ipv6 port */
            if (i + 1 >= argc) {
                return -1;
            }
            cmdParam.net.ipv6Port = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-p4") == 0) { /* ipv4 port */
            if (i + 1 >= argc) {
                return -1;
            }
            cmdParam.net.ipv4Port = atoi(argv[++i]);
			RTPrintf("IPV4 Port %d\n", cmdParam.net.ipv4Port);
        } else if (strcmp(argv[i], "-a6") == 0) { /* ipv6 Address*/
            if (i + 1 >= argc || !RTNetIsIPv6AddrStr(argv[i+1])) {
                return -1;
            }
            strncpy(cmdParam.net.ipv6Addr, argv[++i], sizeof(cmdParam.net.ipv6Addr));
        } else if (strcmp(argv[i], "-a4") == 0) { /* ipv4 Address*/
            if (i + 1 >= argc || !RTNetIsIPv4AddrStr(argv[i+1])) {
                return -1;
            }
            strncpy(cmdParam.net.ipv4Addr, argv[++i], sizeof(cmdParam.net.ipv4Addr));
        } else if (strcmp(argv[i], "-tf") == 0) { /* Testing file input. */
            if (i + 1 >= argc) {
                return -1;
            }
            cmdParam.inputFile = argv[++i];
			// testing file only works with dummy driver, so enable it automatically.
			cmdParam.enableDummyDriver = true;
        } else if (strcmp(argv[i], "-dummy") == 0) { /* use dummy Driver */
			cmdParam.enableDummyDriver = true;
		} else if (strcmp(argv[i], "-v") == 0) {
			RTPrintf("%s Version: %d.%d.%d\n", VBOX_PRODUCT,
				VBOX_VERSION_MAJOR, VBOX_VERSION_MINOR, VBOX_VERSION_BUILD);
			exit(0);
		} else if (strcmp(argv[i], "-log") == 0) {
			if ( (i + 1) <= argc) {
				cmdParam.logFilePath = argv[++i];
				if (cmdParam.logFilePath == NULL) continue;
				printf("\nTrying to open logfile %s\n", cmdParam.logFilePath);				
			    if((cmdParam.logFileHandle = freopen(cmdParam.logFilePath, "w", stderr)) == NULL)
				{
				  printf("Unable to redirect output to logfile %s\n", cmdParam.logFilePath);
				}
				else
				{
				  printf("Logging now to this file \n");
				}
			}
			else
			{
				printf("Filename argument missing\n");
				cmdParam.logFilePath = NULL;
				cmdParam.logFileHandle = NULL;
			}
		}				
    }
    if (cmdParam.x == 0) {
        cmdParam.x = 800;
    }
    if (cmdParam.y == 0) {
        cmdParam.y = 600;
    }
    if (cmdParam.bpp != 8 && cmdParam.bpp != 16 && cmdParam.bpp != 24 && cmdParam.bpp != 32) {
        cmdParam.bpp = 32;
    }
    // Only 32 bpp works on current XPDM driver
    cmdParam.bpp = 32;
    dump_cmd(&cmdParam);
    return 0;
}

extern int VirtualMonitorMain(DisplayParam cmd);
extern "C" DECLEXPORT(int) TrustedMain (int argc, char **argv, char **envp)
{
    if (decode_cmd(argc, argv)) {
        Usage();
        return 0;
    }
    int rc = VirtualMonitorMain(cmdParam);
	if (cmdParam.logFilePath != NULL && cmdParam.logFileHandle != NULL)
	{
		fclose(cmdParam.logFileHandle);
	}
	return rc;
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

// #ifndef VBOX_WITH_HARDENING
/**
 * Main entry point.
 */
int main(int argc, char **argv)
{
    /*
     * Before we do *anything*, we initialize the runtime.
     */
#if defined (RT_OS_WINDOWS)
	if (IsWow64()) {
		RTPrintf("Your are runing 32bit VirtualMonitor on 64bit windows\n");
		RTPrintf("Please Download 64bit version of VirtualMonitor\n");
		return -1;
	}
#endif
    int rc = RTR3InitExe(argc, &argv, 0);
    if (RT_FAILURE(rc))
        return FatalError("RTR3InitExe failed rc=%Rrc\n", rc);

    return TrustedMain(argc, argv, NULL);
}
// #endif /* !VBOX_WITH_HARDENING */

