#include <windows.h>
#include <commctrl.h>
#include <setupapi.h>
#include <cpl.h>
#include <tchar.h>
#include <stdio.h>
#include <Newdev.h>

typedef BOOL (*PINSTALL_NEW_DEVICE)(HWND, LPGUID, PDWORD);
typedef BOOL (*PUpdateDriverForPlugAndPlayDevices)(HWND hwndParent,
												LPCTSTR HardwareId,
												LPCTSTR FullInfPath,
												DWORD InstallFlags,
												PBOOL bRebootRequired);
static GUID  DisplayGuid = {0x4d36e968, 0xe325, 0x11ce, {0xbf, 0xc1, 0x08, 0x00, 0x2b, 0xe1, 0x03, 0x18}};

#define INSTALL_LOG_FILE "VirtualMonitorInstall.log"
#define DRIVER_NAME "VirtualMonitorVideo\0\0\0"

static FILE *g_logf = NULL;
static OSVERSIONINFOEX osvi;
static BOOL bIsWindowsXPorLater;

static void logError(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
	fprintf(g_logf, "Error: ");
    vfprintf(g_logf, fmt, args);
}

static void logInfo(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
	fprintf(g_logf, "Info: ");
    vfprintf(g_logf, fmt, args);
}

int DoInstallNewDevice()
{
        HMODULE hNewDev = NULL;
        PINSTALL_NEW_DEVICE InstallNewDevice;
        DWORD Reboot;
        BOOL ret;
        LONG rc;
        HWND hwnd = NULL;

        hNewDev = LoadLibrary(_T("newdev.dll"));
        if (!hNewDev)
        {
                rc = 1;
                goto cleanup;
        }

        InstallNewDevice = (PINSTALL_NEW_DEVICE)GetProcAddress(hNewDev, (LPCSTR)"InstallNewDevice");
        if (!InstallNewDevice)
        {
                rc = 2;
                goto cleanup;
        }

        ret = InstallNewDevice(hwnd, &DisplayGuid, &Reboot);
        if (!ret)
        {
                rc = 3;
                goto cleanup;
        }

        if (Reboot != DI_NEEDRESTART && Reboot != DI_NEEDREBOOT)
        {
                /* We're done with installation */
                rc = 0;
                goto cleanup;
        }

        /* We need to reboot */
        if (SetupPromptReboot(NULL, hwnd, FALSE) == -1)
        {
                /* User doesn't want to reboot, or an error occurred */
                rc = 5;
                goto cleanup;
        }

        rc = 0;

cleanup:
        if (hNewDev != NULL)
                FreeLibrary(hNewDev);
        return rc;
}

int InstallInf(_TCHAR *inf)
{
	GUID ClassGUID;
	TCHAR ClassName[256];
	HDEVINFO DeviceInfoSet = 0;
	SP_DEVINFO_DATA DeviceInfoData;
	TCHAR FullFilePath[1024];
	PUpdateDriverForPlugAndPlayDevices pfnUpdateDriverForPlugAndPlayDevices;
	HMODULE hNewDev = NULL;
	DWORD Reboot;
	HWND hwnd = NULL;
	int ret = -1;

	if (!GetFullPathName(inf, sizeof(FullFilePath), FullFilePath, NULL)) {
		logError("GetPath: %x\n", GetLastError());
		return ret;
	}
	logInfo("Inf file: %s\n", FullFilePath);

	hNewDev = LoadLibrary(_T("newdev.dll"));
	if (!hNewDev) {
		logError("newdev.dll: %x\n", GetLastError());
		return ret;
	}

	pfnUpdateDriverForPlugAndPlayDevices = (PUpdateDriverForPlugAndPlayDevices)
									GetProcAddress(hNewDev, (LPCSTR)"UpdateDriverForPlugAndPlayDevicesA");
	if (!pfnUpdateDriverForPlugAndPlayDevices) {
		logError("No UpdateDriverForPlugAndPlayDevices: %x\n", GetLastError());
		goto FreeLib;
	}
 
	if (!SetupDiGetINFClass(FullFilePath, &ClassGUID, ClassName, sizeof(ClassName), 0)) {
		logError("INF Class: %x\n", GetLastError());
		goto FreeLib;
	}

	if ((DeviceInfoSet = SetupDiCreateDeviceInfoList(&ClassGUID,0)) == INVALID_HANDLE_VALUE) {
		logError("DeviceInfoList: %x\n", GetLastError());
		goto FreeLib;
	}
	DeviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
	if (!SetupDiCreateDeviceInfo(DeviceInfoSet,
								ClassName,
								&ClassGUID,
								NULL,
								0,
								DICD_GENERATE_ID,
								&DeviceInfoData)) {
		logError("DeviceInfo: %x\n", GetLastError());
		goto SetupDiCreateDeviceInfoListError;
	}

	if(!SetupDiSetDeviceRegistryProperty(DeviceInfoSet, &DeviceInfoData, SPDRP_HARDWAREID, 
										(LPBYTE)DRIVER_NAME,
										(lstrlen(DRIVER_NAME)+2))) {
		logError("DeviceInfo: %x\n", GetLastError());
		goto SetupDiCreateDeviceInfoListError;
	}
	if (!SetupDiCallClassInstaller(DIF_REGISTERDEVICE, DeviceInfoSet, &DeviceInfoData)) {
		logError("ClassInstaller: %x\n", GetLastError());
		goto SetupDiCreateDeviceInfoListError;
	}
    ret = pfnUpdateDriverForPlugAndPlayDevices(NULL, DRIVER_NAME, FullFilePath, 0, &Reboot);
	if (Reboot == DI_NEEDRESTART || Reboot == DI_NEEDREBOOT) {
		if (SetupPromptReboot(NULL, hwnd, FALSE) == -1) {
			; // ToDo ??
		}
	}
	ret = 0;

SetupDiCreateDeviceInfoListError:
	SetupDiDestroyDeviceInfoList(DeviceInfoSet);
FreeLib:
	FreeLibrary(hNewDev);
	return ret;
}

static void usage()
{
}

static void GetWinVersion()
{
	BOOL bIsWindowsXPorLater;
	ZeroMemory(&osvi, sizeof(OSVERSIONINFOEX));
	osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
	GetVersionEx(&osvi);
	bIsWindowsXPorLater = ((osvi.dwMajorVersion >= 6));
	logInfo("Windows version: %d.%d.%d.%d\n",
			osvi.dwMajorVersion,
			osvi.dwMinorVersion,
			osvi.dwBuildNumber,
			osvi.dwPlatformId);
	logInfo("Service pack: %d.%d ProductType: %d, vista+: %d\n",
			osvi.wServicePackMajor,
	  		osvi.wServicePackMinor,
		  	osvi.wProductType,
			bIsWindowsXPorLater);
	logInfo("%s\n",  osvi.szCSDVersion);
}

int __cdecl _tmain(int argc, _TCHAR *argv[])
{
	g_logf = fopen(INSTALL_LOG_FILE, "w+");
	if (!g_logf) {
		printf("Can't Open install log file\n");
		return -1;
	}

	if (argc < 2) {
		usage();
		goto out;
	}

	GetWinVersion();
	InstallInf(argv[1]);
out:
	if (g_logf)
		fclose(g_logf);
	exit(0);
}


