#include <windows.h>
#include <commctrl.h>
#include <setupapi.h>
#include <cpl.h>
#include <tchar.h>
#include <stdio.h>
#include <Newdev.h>
#include <Cfgmgr32.h>
#include <Shlobj.h>
#include <strsafe.h>

typedef BOOL (*PINSTALL_NEW_DEVICE)(HWND, LPGUID, PDWORD);
typedef BOOL (*PUpdateDriverForPlugAndPlayDevices)(HWND hwndParent,
												LPCTSTR HardwareId,
												LPCTSTR FullInfPath,
												DWORD InstallFlags,
												PBOOL bRebootRequired);
static GUID  DisplayGuid = {0x4d36e968, 0xe325, 0x11ce, {0xbf, 0xc1, 0x08, 0x00, 0x2b, 0xe1, 0x03, 0x18}};
#define MIRROR_REG_PATH "SYSTEM\\CurrentControlSet\\Control\\Class\\{4D36E968-E325-11CE-BFC1-08002BE10318}"
#define RUNONCE_REG_PATH "Software\\Microsoft\\Windows\\CurrentVersion\\RunOnce"


#define INSTALL_LOG_FILE "VirtualMonitorInstall.log"
#define DRIVER_NAME "VirtualMonitorVideo\0\0\0"
#define INF	"VirtualMonitor.inf"

static FILE *g_logf = NULL;
static OSVERSIONINFOEX osvi;
static BOOL isWin2k = FALSE;
static BOOL isWinXp = FALSE;
static BOOL isVista = FALSE;
static BOOL isWin7 = FALSE;
static BOOL isSupport = FALSE;

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
	SP_DEVINFO_DATA data;
	UINT i;
	BOOL found;
	CHAR *buffer;
	UINT buffer_size = 8092;
	DWORD required_size;
	DWORD data_type;

	dev_info = SetupDiGetClassDevsEx(NULL, NULL, NULL, DIGCF_ALLCLASSES | DIGCF_PRESENT, NULL, NULL, NULL);
	if (dev_info == NULL)
	{
		return NULL;
	}

#if 0
	SP_DEVINFO_LIST_DETAIL_DATA detail_data;
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
	printf("%s -i\t install driver\n", argv[0] );
	printf("%s -u\t uninstall driver\n", argv[0]);
	printf("%s -h\t show help\n", argv[0]);
}

static void GetWinVersion()
{
	ZeroMemory(&osvi, sizeof(OSVERSIONINFOEX));
	osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
	GetVersionEx((LPOSVERSIONINFO)&osvi);
	logInfo("Windows version: %d.%d.%d.%d\n",
			osvi.dwMajorVersion,
			osvi.dwMinorVersion,
			osvi.dwBuildNumber,
			osvi.dwPlatformId);
	logInfo("Service pack: %d.%d ProductType: %d\n",
			osvi.wServicePackMajor,
	  		osvi.wServicePackMinor,
		  	osvi.wProductType);
	logInfo("%s\n",  osvi.szCSDVersion);

    if ( osvi.dwMajorVersion == 5) {
		if (osvi.dwMinorVersion == 0) {
			isWin2k = TRUE;
		} else {
			isWinXp = TRUE;
		}
    } else if (osvi.dwMajorVersion == 6) {
		if (osvi.dwMinorVersion == 0) {
			isVista = TRUE;
		}
		if (osvi.dwMinorVersion == 1) {
			isWin7 = TRUE;	
		}
	}
	isSupport = isWin2k || isWinXp || isVista || isWin7;
}

HKEY GetDriverRegKey()
{
	CHAR devid[256];
	HKEY hSubKey;
	HKEY hRegRoot;
	BOOL found = FALSE;
	LONG rc;
    LONG query;
    DWORD dwType = 0;
    DWORD dwIdIndex;
    CHAR idName[64];
    DWORD idSize = sizeof(idName);
	DWORD dwSize= sizeof(devid);

	rc = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
						MIRROR_REG_PATH,
						0,
						KEY_READ,
						&hRegRoot);
	if (rc != ERROR_SUCCESS) {
		RegCloseKey(hRegRoot);
		logError("Open driver registry root key failed %x, %d\n", GetLastError() , rc);
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
		rc = RegOpenKeyEx(hRegRoot,
				idName,
				0,
				KEY_READ,
				&hSubKey);
		if (rc != ERROR_SUCCESS) {
			logError("Open sub key for query failed:%s %x\n", idName, GetLastError());
			RegCloseKey(hSubKey);
			continue;
		}
		rc = RegQueryValueEx(hSubKey,
				"MatchingDeviceId",
				0,
				&dwType,
				(PBYTE)&devid,
				&dwSize);
		dwSize = sizeof(devid);
		if (rc != ERROR_SUCCESS) {
			RegCloseKey(hSubKey);
			continue;
		}

		if (dwType == REG_SZ) {
			if (stricmp(devid, DRIVER_NAME) == 0) {
				logInfo("found: %s Driver\n", idName, devid);
				found = TRUE;
				break;
			}
		}
		RegCloseKey(hSubKey);
    } while (query != ERROR_NO_MORE_ITEMS);

	RegCloseKey(hSubKey);
	hSubKey = NULL;
	if (found) {
		rc = RegOpenKeyEx(hRegRoot,
				idName,
				0,
				KEY_ALL_ACCESS,
				&hSubKey);
		if (rc != ERROR_SUCCESS) {
			logError("Open driver registry key for write failed, %x, %d\n", GetLastError(), rc);
		}
	}
	
	RegCloseKey(hRegRoot);
	return hSubKey;
}

BOOL DisableMirror()
{
	LONG rc;
	HKEY hRootKey = NULL;
	HKEY hSubKey;
	DWORD mirror = 0;
	BOOL ret = FALSE;
	int i;

	for (i = 0; i < 6*10; i++) {
		hRootKey = GetDriverRegKey();
		if (hRootKey)
			break;
		Sleep(10*1000);
	} 
	if (!hRootKey) {
		goto out;
	}

	logInfo("Open Driver Setting Key ok: %d\n", i);
	rc = RegOpenKeyEx(hRootKey,
						"Settings",
						0,
						KEY_ALL_ACCESS,
						&hSubKey);
	if (rc != ERROR_SUCCESS) {
		logError("Settings %x, %d\n", GetLastError() , rc);
		goto CloseRoot;
	}

	rc = RegSetValueEx(hSubKey,
						"MirrorDriver",
						0,
						REG_DWORD,
						(const BYTE *)&mirror,
						sizeof(mirror));
	if (rc != ERROR_SUCCESS) {
		logError("Set MirrorDriver Failed %x, %d\n", GetLastError() , rc);
		goto CloseSub;
	}
	ret = TRUE;

CloseSub:
	RegCloseKey(hSubKey);
CloseRoot:
	RegCloseKey(hRootKey);
out:
	return ret;
}

BOOL CleanOemInf()
{
	HKEY hRootKey = NULL;
	CHAR inf[256];
    DWORD dwType = 0;
	BOOL ret = FALSE;
	LONG rc;
	DWORD dwSize;
	int n;

	hRootKey = GetDriverRegKey();
	if (!hRootKey) {
		logError("Open Driver registry failed\n");
		return ret;
	}
	memset(inf, 0, sizeof(inf));
	n = sprintf(inf, "%s", "pnputil.exe -d ");
	dwSize = sizeof(inf) - n;
	rc = RegQueryValueEx(hRootKey,
						"InfPath",
						0,
						&dwType,
						(PBYTE)&inf[n],
						&dwSize);
	if (rc != ERROR_SUCCESS) {
		logError("uninstall get OEM Inf failed: %x %d\n", GetLastError(), rc);
		goto out;
	}
	logInfo("uninstall OEM Inf: %s\n", inf);
	RegCloseKey(hRootKey);

	rc = RegCreateKeyEx(HKEY_LOCAL_MACHINE,
						RUNONCE_REG_PATH,
						0,
						NULL,
						REG_OPTION_NON_VOLATILE,
						KEY_WRITE,
						NULL,
						&hRootKey,
						NULL);
	if (rc != ERROR_SUCCESS) {
		RegCloseKey(hRootKey);
		logError("uninstall Open RunOnce Key failed: %x, %d\n", GetLastError() , rc);
		goto out;
	}

	rc = RegSetValueEx(hRootKey,
						"!VirtualMonitorRemove",
						0,
						REG_SZ,
						(const BYTE*)inf, dwSize+n);
	if (rc != ERROR_SUCCESS) {
		RegCloseKey(hRootKey);
		logError("uninstall Write RunOnce Key failed: %x, %d\n", GetLastError() , rc);
		goto out;
	}
	ret = TRUE;
out:
	RegCloseKey(hRootKey);
	return ret;
}

BOOL FixInfFile(CHAR *inf)
{
	FILE *fp;
	CHAR *buf = NULL;
	int sz;
	int ch;
	BOOL ret = FALSE;
	int line = 0;
	int current = 0;
	int i = 0;


	fp = fopen(inf, "r");
	if (!fp) {
		logError("Read INF File Failed\n");
		goto out;
	}

	fseek(fp, 0L, SEEK_END);
	sz = ftell(fp) + 100;
	fseek(fp, 0L, SEEK_SET);

	buf = (LPTSTR)LocalAlloc(LPTR, sz+100);
	if (!buf) {
		logError("Can't allocate memory for INF\n");	
		goto out;
	}
	memset(buf, 0, sz+100);
	do {
		ch = fgetc(fp);
		buf[i] = ch;
		if (ch == '\n') {
			if (strstr(&buf[current], "MirrorDriver")) {
				sz = sprintf(&buf[current], "%s", "HKR,, MirrorDriver, %REG_DWORD%, ");
				if (isVista || isWin7) {
					sz += sprintf(&buf[current+sz], "%d\n", 1);
				} else {
					sz += sprintf(&buf[current+sz], "%d\n", 0);
				}
				i = current+sz-1;
				current += sz;
			} else {
				current = i+1;
			}
			line++;
		}
		i++;
	} while (ch != EOF);

	fclose(fp);
	
	fp = fopen(inf, "w+");
	if (!fp) {
		logError("Write INF File Failed\n");
		goto out;
	}
	// 1 for EOF
	fwrite(buf, i-1, 1, fp);
	fclose(fp);
out:
	if (fp)
		fclose(fp);
	if (buf)
		LocalFree(buf);
	return ret;
}


//*************************************************************
//  http://msdn.microsoft.com/en-us/library/ms724235%28VS.85%29.aspx
//  RegDelnodeRecurse()
//
//  Purpose:    Deletes a registry key and all its subkeys / values.
//
//  Parameters: hKeyRoot    -   Root key
//              lpSubKey    -   SubKey to delete
//
//  Return:     TRUE if successful.
//              FALSE if an error occurs.
//
//*************************************************************
BOOL RegDelnodeRecurse (HKEY hKeyRoot, LPTSTR lpSubKey)
{
    LPTSTR lpEnd;
    LONG lResult;
    DWORD dwSize;
    TCHAR szName[MAX_PATH];
    HKEY hKey;
    FILETIME ftWrite;

    // First, see if we can delete the key without having
    // to recurse.

    lResult = RegDeleteKey(hKeyRoot, lpSubKey);

    if (lResult == ERROR_SUCCESS) 
        return TRUE;

    lResult = RegOpenKeyEx (hKeyRoot, lpSubKey, 0, KEY_READ, &hKey);

    if (lResult != ERROR_SUCCESS) 
    {
        if (lResult == ERROR_FILE_NOT_FOUND) {
            printf("Key not found.\n");
            return TRUE;
        } 
        else {
            printf("Error opening key.\n");
            return FALSE;
        }
    }

    // Check for an ending slash and add one if it is missing.

    lpEnd = lpSubKey + lstrlen(lpSubKey);

    if (*(lpEnd - 1) != TEXT('\\')) 
    {
        *lpEnd =  TEXT('\\');
        lpEnd++;
        *lpEnd =  TEXT('\0');
    }

    // Enumerate the keys

    dwSize = MAX_PATH;
    lResult = RegEnumKeyEx(hKey, 0, szName, &dwSize, NULL,
                           NULL, NULL, &ftWrite);

    if (lResult == ERROR_SUCCESS) 
    {
        do {

            StringCchCopy (lpEnd, MAX_PATH*2, szName);

            if (!RegDelnodeRecurse(hKeyRoot, lpSubKey)) {
                break;
            }

            dwSize = MAX_PATH;

            lResult = RegEnumKeyEx(hKey, 0, szName, &dwSize, NULL,
                                   NULL, NULL, &ftWrite);

        } while (lResult == ERROR_SUCCESS);
    }

    lpEnd--;
    *lpEnd = TEXT('\0');

    RegCloseKey (hKey);

    // Try again to delete the key.

    lResult = RegDeleteKey(hKeyRoot, lpSubKey);

    if (lResult == ERROR_SUCCESS) 
        return TRUE;

    return FALSE;
}

//*************************************************************
//  http://msdn.microsoft.com/en-us/library/ms724235%28VS.85%29.aspx
//  RegDelnode()
//
//  Purpose:    Deletes a registry key and all its subkeys / values.
//
//  Parameters: hKeyRoot    -   Root key
//              lpSubKey    -   SubKey to delete
//
//  Return:     TRUE if successful.
//              FALSE if an error occurs.
//
//*************************************************************

BOOL RegDelnode (HKEY hKeyRoot, LPTSTR lpSubKey)
{
    TCHAR szDelKey[MAX_PATH*2];

    StringCchCopy (szDelKey, MAX_PATH*2, lpSubKey);
    return RegDelnodeRecurse(hKeyRoot, szDelKey);
}

BOOL RegClean()
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
	BOOL res = FALSE;

    rc = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
						"SYSTEM\\CurrentControlSet\\Control\\Video",
						0,
						KEY_ALL_ACCESS,
						&hRegRoot);
    
    if (rc != ERROR_SUCCESS) {
        logError("RegClean Open Root Key Failed:%x, %d\n", GetLastError(), rc);
        return FALSE;
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

        sprintf(video, "%s\\Video", idName);
        rc = RegOpenKeyEx(hRegRoot,
                            video,
                            0,
                            KEY_QUERY_VALUE,
                            &hSubKey);
        if (rc != ERROR_SUCCESS) {
            logError("RegClean Open Sub Key Failed: %x, %d\n", GetLastError(), rc);
			continue;
        }
        rc = RegQueryValueEx(hSubKey,
                "Service",
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
            if (stricmp(lpDesc, DRIVER_NAME) == 0) {
        		RegCloseKey(hSubKey);
				hSubKey = NULL;
				// RegDeleteTree is not avalible on win2k winXp
				// rc = RegDeleteTree(hRegRoot, idName);
				res = RegDelnode(hRegRoot, idName);
				logInfo("RegClean %s %x %d\n", idName, GetLastError(), res);
			}
        }
		if (hSubKey) {
        	RegCloseKey(hSubKey);
		}
    } while (query != ERROR_NO_MORE_ITEMS);
    RegCloseKey(hRegRoot);

    return TRUE;
}

int __cdecl _tmain(int argc, _TCHAR *argv[])
{
	HDEVINFO h = NULL;
	SP_DEVINFO_DATA dev_info_data;

	if (argc < 2 || !strcmp(argv[1], "-h")) {
		usage(argv);
		goto out;
	}

	g_logf = fopen(INSTALL_LOG_FILE, "w+");
	if (!g_logf) {
		printf("Can't Open install log file\n");
		return -1;
	}

	GetWinVersion();
	if (!isSupport) {
		printf("Unsupported Windows system\n");
		goto out;
	}
	if (isVista || isWin7) {
		if (!IsUserAnAdmin()) {
			printf("Access Denied. Administrator permissions are needed to use the selected options.");
			printf("Use an administrator command prompt to complete these tasks.");
			goto out;
		}
	}


	if (!strcmp(argv[1], "-i")) {
		FixInfFile(INF);
	}
	h = GetDevInfoFromDeviceId(&dev_info_data, DRIVER_NAME);
	if (!strcmp(argv[1], "-i")) {
		if (h) {
			logInfo("Driver already installed\n");
			printf("Driver already installed\n");
			goto out;
		}
		RegClean();
		InstallInf(INF);
		if (isVista || isWin7) {
			DisableMirror();
		}
	} else if (!strcmp(argv[1], "-u")) {
		if (!h) {
			logInfo("Driver not found\n");
			printf("Driver not found\n");
		} else {
			UnInstallDriver(h, &dev_info_data);
			if (isVista || isWin7) {
				CleanOemInf();
			}
			RegClean();
			printf("Please Reboot System\n");
		}
	} else {
		usage(argv);
	}
out:
	if (g_logf)
		fclose(g_logf);
	if (h) {
			DestroyDevInfo(h);
	}
	exit(0);
}
