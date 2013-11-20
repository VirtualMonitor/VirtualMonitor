#if defined (RT_OS_WINDOWS)
#include <windows.h>

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
#endif

