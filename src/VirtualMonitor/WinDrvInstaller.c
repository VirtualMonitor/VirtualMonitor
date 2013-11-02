#include <windows.h>
#include <commctrl.h>
#include <setupapi.h>
#include <cpl.h>
#include <tchar.h>
#include <stdio.h>
#include <Newdev.h>
#include<Cfgmgr32.h>

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
		logError("GetFullPathName: %x\n", GetLastError());
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
		logError("UpdateDriverForPlugAndPlayDevices: %x\n", GetLastError());
		goto FreeLib;
	}
 
	if (!SetupDiGetINFClass(FullFilePath, &ClassGUID, ClassName, sizeof(ClassName), 0)) {
		logError("SetupDiGetINFClass: %x\n", GetLastError());
		goto FreeLib;
	}
#if 0
	printf("{%08lX-%04hX-%04hX-%02hhX%02hhX-%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX}\n",
			ClassGUID.Data1, ClassGUID.Data2, ClassGUID.Data3, ClassGUID.Data4[0],
			ClassGUID.Data4[1], ClassGUID.Data4[2], ClassGUID.Data4[3], ClassGUID.Data4[4],
			ClassGUID.Data4[5], ClassGUID.Data4[6], ClassGUID.Data4[7]);
#endif

	if ((DeviceInfoSet = SetupDiCreateDeviceInfoList(&ClassGUID,0)) == INVALID_HANDLE_VALUE) {
		logError("SetupDiCreateDeviceInfoList: %x\n", GetLastError());
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
		logError("SetupDiCreateDeviceInfo: %x\n", GetLastError());
		goto SetupDiCreateDeviceInfoListError;
	}

	if(!SetupDiSetDeviceRegistryProperty(DeviceInfoSet, &DeviceInfoData, SPDRP_HARDWAREID, 
										(LPBYTE)DRIVER_NAME,
										(lstrlen(DRIVER_NAME)+2))) {
		logError("SetupDiSetDeviceRegistryProperty: %x\n", GetLastError());
		goto SetupDiCreateDeviceInfoListError;
	}
	if (!SetupDiCallClassInstaller(DIF_REGISTERDEVICE, DeviceInfoSet, &DeviceInfoData)) {
		logError("SetupDiCallClassInstaller: %x\n", GetLastError());
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

static void DestroyDevInfo(HDEVINFO info)
{
	if (info)
		SetupDiDestroyDeviceInfoList(info);
}

static HDEVINFO GetDevInfoFromDeviceId(SP_DEVINFO_DATA *dev_info_data, CHAR *device_id)
{
	HDEVINFO dev_info;
	SP_DEVINFO_LIST_DETAIL_DATA detail_data;
	SP_DEVINFO_DATA data;
	UINT i;
	BOOL found;
	char *buffer;
	UINT buffer_size = 8092;
	DWORD required_size;
	DWORD data_type;

	dev_info = SetupDiGetClassDevsEx(NULL, NULL, NULL, DIGCF_ALLCLASSES | DIGCF_PRESENT, NULL, NULL, NULL);
	if (dev_info == NULL)
	{
		return NULL;
	}

#if 0
	memset(&detail_data, 0, sizeof(detail_data));
	detail_data.cbSize = sizeof(detail_data);
	if (SetupDiGetDeviceInfoListDetail(dev_info, &detail_data) == FALSE)
	{
		DestroyDevInfo(dev_info);
		return NULL;
	}
#endif

	memset(&data, 0, sizeof(data));
	data.cbSize = sizeof(data);
	found = FALSE;
	buffer = (LPTSTR)LocalAlloc(LPTR, buffer_size);
	if (!buffer) {
		logError("Alloc: %x\n", GetLastError());
		goto out;
	}
	
	for (i = 0;SetupDiEnumDeviceInfo(dev_info, i, &data); i++) {
		while (!SetupDiGetDeviceRegistryProperty(dev_info,
												&data,
												SPDRP_HARDWAREID,
												&data_type,
												(PBYTE)buffer,
												buffer_size,
												&required_size)) {
			if (ERROR_INSUFFICIENT_BUFFER == GetLastError()) {
				// Change the buffer size.
				if (buffer) {
					LocalFree(buffer);
				}
				// Double the size to avoid problems on 
				// W2k MBCS systems per KB 888609. 
				buffer_size *= 2;
            	buffer = (LPTSTR)LocalAlloc(LPTR, buffer_size);
				if (!buffer) {
					logError("LocalAlloc: %x\n", GetLastError());
					goto out;
				}
			} else {
				// EnumNext
				break;
			}
		}

		if (stricmp(buffer, device_id) == 0) {
			printf("found\n");
			found = TRUE;
		}

		if (found) {
			goto out;
		}

		memset(&data, 0, sizeof(data));
		data.cbSize = sizeof(data);
	}
out:
	if (buffer)
		LocalFree(buffer);
	if (found == FALSE) {
		DestroyDevInfo(dev_info);
		return NULL;
	} else {
		memcpy(dev_info_data, &data, sizeof(data));
		return dev_info;
	}
}

BOOL DeleteDevice(HDEVINFO info, SP_DEVINFO_DATA *dev_info_data)
{
	SP_REMOVEDEVICE_PARAMS p;
	SP_DEVINFO_LIST_DETAIL_DATA detail;
	char device_id[MAX_PATH];
	if (info == NULL || dev_info_data == NULL)
	{
		return FALSE;
	}

	memset(&detail, 0, sizeof(detail));
	detail.cbSize = sizeof(detail);

	if (SetupDiGetDeviceInfoListDetail(info, &detail) == FALSE ||
		CM_Get_Device_ID_Ex(dev_info_data->DevInst, device_id, sizeof(device_id),
		0, detail.RemoteMachineHandle) != CR_SUCCESS)
	{
		logError("CM_Get_Device_ID_Ex: %x\n", GetLastError());
		return FALSE;
	}

	memset(&p, 0, sizeof(p));
	p.ClassInstallHeader.cbSize = sizeof(SP_CLASSINSTALL_HEADER);
	p.ClassInstallHeader.InstallFunction = DIF_REMOVE;
	p.Scope = DI_REMOVEDEVICE_GLOBAL;

	if (SetupDiSetClassInstallParams(info, dev_info_data, &p.ClassInstallHeader, sizeof(p)) == FALSE)
	{
		logError("SetupDiSetClassInstallParams: %x\n", GetLastError());
		return FALSE;
	}

	if (SetupDiCallClassInstaller(DIF_REMOVE, info, dev_info_data) == FALSE)
	{
		logError("SetupDiCallClassInstaller: %x\n", GetLastError());
		return FALSE;
	}

	return TRUE;
}


static BOOL StopDevice(HDEVINFO info, SP_DEVINFO_DATA *dev_info_data)
{
	SP_PROPCHANGE_PARAMS p;
	if (info == NULL || dev_info_data == NULL) {
		return FALSE;
	}

	memset(&p, 0, sizeof(p));
	p.ClassInstallHeader.cbSize = sizeof(SP_CLASSINSTALL_HEADER);
	p.ClassInstallHeader.InstallFunction = DIF_PROPERTYCHANGE;
	p.StateChange = DICS_DISABLE;
	p.Scope = DICS_FLAG_CONFIGSPECIFIC;

	if (SetupDiSetClassInstallParams(info, dev_info_data, &p.ClassInstallHeader, sizeof(p)) == FALSE ||
		SetupDiCallClassInstaller(DIF_PROPERTYCHANGE, info, dev_info_data) == FALSE) {
		return FALSE;
	}

	return TRUE;
}

int UnInstallDriver(HDEVINFO h, SP_DEVINFO_DATA *dev_info_data)
{
	BOOL ret = FALSE;

	StopDevice(h, dev_info_data);
	ret = DeleteDevice(h, dev_info_data);
	return ret;
}

static void usage(_TCHAR *argv[])
{
	printf("%s argv[0] install VirtualMonitor.inf");
	printf("%s argv[0] uninstall");
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
	HDEVINFO h = NULL;
	SP_DEVINFO_DATA dev_info_data;

	g_logf = fopen(INSTALL_LOG_FILE, "w+");
	if (!g_logf) {
		printf("Can't Open install log file\n");
		return -1;
	}

	if (argc < 2) {
		usage(argv);
		goto out;
	}

	GetWinVersion();

	h = GetDevInfoFromDeviceId(&dev_info_data, DRIVER_NAME);
	if (!strcmp(argv[1], "install")) {
		if (h) {
			logInfo("Driver already installed\n");
			printf("Driver already installed\n");
			goto out;
		}
		InstallInf(argv[2]);
	} else if (!strcmp(argv[1], "uninstall")) {
		if (!h) {
			logInfo("Driver not found\n");
			printf("Driver not found\n");
			goto out;
		}
		UnInstallDriver(h, &dev_info_data);
	}
out:
	if (g_logf)
		fclose(g_logf);
	if (h) {
			DestroyDevInfo(h);
	}
	exit(0);
}


