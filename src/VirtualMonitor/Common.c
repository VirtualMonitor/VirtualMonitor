#if defined (RT_OS_WINDOWS)
#include "Common.h"

typedef BOOL (WINAPI *LPFN_ISWOW64PROCESS) (HANDLE, PBOOL); 

BOOL IsWow64()
{
#ifndef RT_ARCH_AMD64
	BOOL ret = FALSE;
	BOOL bIsWow64;
	LPFN_ISWOW64PROCESS fnIsWow64Process;
	HMODULE hKer = NULL;
	hKer = LoadLibrary(TEXT("kernel32"));
	if (!hKer)
		return FALSE;
	fnIsWow64Process = (LPFN_ISWOW64PROCESS)GetProcAddress(hKer ,"IsWow64Process"); 
	if (!fnIsWow64Process) {
		goto out;
	}
	if (!fnIsWow64Process(GetCurrentProcess(), &bIsWow64))
		goto out;
	ret = bIsWow64;
out:
	if (hKer)
		FreeLibrary(hKer);
	return ret;
#else
	return FALSE;
#endif
}

void DestroyDevInfo(HDEVINFO info)
{
	if (info)
		SetupDiDestroyDeviceInfoList(info);
}

HDEVINFO GetDevInfoFromDeviceId(SP_DEVINFO_DATA *dev_info_data, CHAR *device_id)
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
		printf("Alloc: %x\n", GetLastError());
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
					printf("LocalAlloc: %x\n", GetLastError());
					goto out;
				}
			} else {
				// EnumNext
				break;
			}
		}

		if (stricmp(buffer, device_id) == 0) {
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

BOOL GetDevStatus(HDEVINFO h, SP_DEVINFO_DATA *dev_info_data, PULONG status, PULONG problem)
{
	SP_DEVINFO_LIST_DETAIL_DATA detail;

	memset(&detail, 0, sizeof(detail));
	detail.cbSize = sizeof(detail);
	
	SetupDiGetDeviceInfoListDetail(h, &detail);
	if (CM_Get_DevNode_Status_Ex(status,
								problem,
								dev_info_data->DevInst,
								0,
								detail.RemoteMachineHandle) != CR_SUCCESS) {
	
		printf("SetupDiGetDeviceInfoListDetail: %x\n", GetLastError());
		return FALSE;
	}
	return TRUE;
}

#endif /* RT_OS_WINDOWS */

