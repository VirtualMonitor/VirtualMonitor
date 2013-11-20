#ifndef __VIRTUAL_MONITOR_COMMON_H__
#define __VIRTUAL_MONITOR_COMMON_H__

#if defined (RT_OS_WINDOWS)
#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif
	BOOL IsWow64();
#ifdef __cplusplus
}
#endif
#endif

#endif /* __VIRTUAL_MONITOR_COMMON_H__ */
