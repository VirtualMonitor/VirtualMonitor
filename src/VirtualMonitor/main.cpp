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
    RTPrintf("-auto Specific for Trapeze. Detect resolution from config files\n");	
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

typedef bool (*lineCallBack) (const char *pLine, const int len);


bool ReadTextFile(const char* pFileName, lineCallBack fun)
{
	RTPrintf("\nReadUnicodeFile open file %s\n", pFileName);
	FILE *fp = fopen(pFileName,"r+b");
	char *pLine = NULL;
	char *pBuffer = NULL;	
	DWORD totalSize = 0;
	bool success = true;
	
	unsigned int lastLength = 0;
	if (!fp)
	{
		RTPrintf("\nUnable to open file %s", pFileName);
		return false;
	}
	if (!fun)
	{
		RTPrintf("\nCallback not specified. Returning ");
		return false;
	}
	fseek(fp, 0L, SEEK_END);
	totalSize = ftell(fp);
	fseek(fp, 0L, SEEK_SET);
	
	RTPrintf("\nFile Size is %d", totalSize);
	pBuffer = (char*) malloc(totalSize);
	if (!pBuffer)
	{
	    RTPrintf("\nUnable to allocate memory of bytes %d", totalSize);
		return false;
	}
	
	if(success)
	{
		DWORD readBytes = 0, curRead = 0, remaining = 0;
		curRead = 0;
		remaining = totalSize;
		readBytes = 0;
		while(readBytes < totalSize)
		{
			curRead = fread( pBuffer + readBytes , 1 , remaining, fp);
			if (curRead == 0 || feof(fp))
			{
				break;
			}
			readBytes += curRead;
			remaining -= curRead;
		}
		if (remaining)
		{
			RTPrintf("\nUnable to read complete file. Read %d out of %d", readBytes, totalSize);
		}
	}
	// All the contents of file has been read. Now check the file type
	PWSTR pHeader = reinterpret_cast<PWSTR>(pBuffer);
	if ( (*pHeader) == 0xFFFE || (*pHeader) == 0xFEFF)
	{
		DWORD tempSize = 0;
		char *tempBuffer = NULL;
		DWORD curSz = totalSize- 2;
		RTPrintf("\nReadFile is Unicode indeed. Signature %x", (*pHeader));
		pHeader++;
		
		success = false;
		//Note: Skip the header signature while reading file
		tempSize = WideCharToMultiByte(CP_UTF8, 0, (LPCWSTR)(pHeader), curSz, NULL, 0, NULL, NULL);
		RTPrintf("\nFile requires memory of %d to be converted", tempSize);	
		// Do not allocate more than 1Meg
		if (tempSize < 0x100000)
		{
			tempBuffer = (char*) malloc(tempSize);
			if (tempBuffer)
			{
				RTPrintf("\nAllocated memory. Starting conversion");
				if ( 0 == WideCharToMultiByte(CP_UTF8, 0, (LPCWSTR)(pHeader), curSz, tempBuffer, tempSize, NULL, NULL))
				{
					RTPrintf("\nConversion completed. Swapping memory");
					// Reassign original buffer to new buffer
					free(pBuffer);
					pBuffer = tempBuffer;
					totalSize = tempSize;
					success = true;
				}
				else
				{
					RTPrintf("\nUnable to do Conversion. Freeing memory");
					free(tempBuffer);
				}
			}
		}
	}
	else if (memcmp(pBuffer, "\xEF\xBB\xBF", 3) == 0)
    {
		DWORD tempSize = 0;
		char *tempBuffer = NULL;
		success = false;
        // UTF-8
		RTPrintf("\nReadFile is UTF-8 indeed");
		tempBuffer = (char*) malloc(totalSize - 3);	
		if (tempBuffer)
		{
			memcpy(tempBuffer, pBuffer + 3, totalSize - 3);
			free(pBuffer);
			pBuffer = tempBuffer;
			totalSize = totalSize -3;
			success = true;
		}
	}
	
	for (unsigned int i = 0, lastlength = 0 ; i < totalSize ; i++, lastLength++)
	{
		if (pBuffer[i] == '\r' || pBuffer[i] == '\n')
		{	
			// replace line terminator by NULL character
			pBuffer[i] = 0;
			if ( (lastLength > 1) && fun)
			{
				if(!fun(pLine, lastLength))
				{
					//Function says no need to continue
					break;
				}
			}
			pLine = pBuffer + i + 1;
			lastLength = 0;
		}
	}
	if (!pBuffer)
	{
		free(pBuffer);
	}
	fclose(fp);
	return success;
}

static int techVehicle = 0;
static int defaultx = 1280, defaulty = 800;

bool GetResolution (const char *pLine, const int len)
{
	bool retVal = true;
	int number = 0, x = 0, y = 0;

	
	if (pLine != NULL && len > 0)
	{
		if (strstr(pLine, "#"))
		{
			return true;
		}
		if ( 3 == sscanf(pLine, "%d %d %d", &number, &x, &y))
		{
			if ( techVehicle == number)
			{
				printf("\nGot Techical Vehicle %d resolution %d X %d",techVehicle, x, y );
				cmdParam.x = x;
				cmdParam.y = y;
				retVal = false;
			}
			else if (number == 0 && x && y)
			{
				defaultx = x;
				defaulty = y;	
			    printf("\nUpdating default resolution %d X %d",x, y );
			}
		}
	}
	return retVal;
}

const char configVirtMonitor[] = "d:\\Programs\\Ecu\\Config\\CnfVirtualMonitor.txt";
				
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
				RTPrintf("\nTrying to open logfile %s\n", cmdParam.logFilePath);				
			    if((cmdParam.logFileHandle = freopen(cmdParam.logFilePath, "w", stderr)) == NULL)
				{
				  RTPrintf("Unable to redirect output to logfile %s\n", cmdParam.logFilePath);
				}
				else
				{
				  RTPrintf("Logging now to this file \n");
				}
			}
			else
			{
				RTPrintf("Filename argument missing\n");
				cmdParam.logFilePath = NULL;
				cmdParam.logFileHandle = NULL;
			}
		}
		else if (strcmp(argv[i], "-auto") == 0) 
		{
			char buffer[128];
			cmdParam.x = 0;
			cmdParam.y = 0;
			DWORD retVal = GetEnvironmentVariableA("TRAPEZE_VEHICLE_NUMBER", buffer, 128);
			if (retVal > 0 && retVal < 128)
			{
				techVehicle = atoi(buffer); 
				RTPrintf("\nTrapeze Technical Vehicle number %d", techVehicle);
				if ( ReadTextFile( configVirtMonitor, GetResolution ) )
				{
					RTPrintf("\nX resolution= %d Y resolution= %d",cmdParam.x, cmdParam.y );
				}	
			}
			else
			{
				RTPrintf("\nUnable to determine Trapeze Technical Vehicle Number");
			}
		}					
    }
    if (cmdParam.x == 0) {
        cmdParam.x = defaultx;
    }
    if (cmdParam.y == 0) {
        cmdParam.y = defaulty;
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

