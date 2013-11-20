#ifndef __VIRTUAL_MONITOR_COMMON_H__
#define __VIRTUAL_MONITOR_COMMON_H__

#if defined (RT_OS_WINDOWS)
#include <windows.h>
#include <setupapi.h>
#include <Cfgmgr32.h>

#ifdef __cplusplus
extern "C" {
#endif

	BOOL IsWow64();
	HDEVINFO GetDevInfoFromDeviceId(SP_DEVINFO_DATA *dev_info_data, CHAR *device_id);
	void DestroyDevInfo(HDEVINFO info);
	BOOL GetDevStatus(HDEVINFO h, SP_DEVINFO_DATA *dev_info_data, PULONG status, PULONG problem);

#ifdef __cplusplus
}
#endif
#endif

#endif /* __VIRTUAL_MONITOR_COMMON_H__ */
