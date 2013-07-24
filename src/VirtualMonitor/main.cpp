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


#include "Display.h"
static DisplayParam cmdParam;

void Usage()
{
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

    RTPrintf("%s: %d argc: %d\n", __FUNCTION__, __LINE__, argc);
        
    for (i = i1 = 1; i < argc; i++) {
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
    return VirtualMonitorMain(cmdParam);
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
    int rc = RTR3InitExe(argc, &argv, 0);
    if (RT_FAILURE(rc))
        return FatalError("RTR3InitExe failed rc=%Rrc\n", rc);

    return TrustedMain(argc, argv, NULL);
}
// #endif /* !VBOX_WITH_HARDENING */

