// File Name:
//		Driver.h
//
// Contents:
//		Constants, structures, and function
//		declarations specific to this driver.
//
#pragma once
//
// Header files
//
//extern "C" {
#include <NTDDK.h>
//}
#include <dispmprt.h>
#include "Unicode.h"
#include "EventLog.h"
#include "Msg.h"
#include <ntstrsafe.h>
#include <string.h>
static BOOLEAN Stat;///Special 
#define PORTIO_TAG 'TROP'

// BUFFER_SIZE_INFO is a driver-defined structure
// that describes the buffers used by the filter



typedef struct _BUFFER_SIZE_INFO
{
	ULONG MaxWriteLength;
	ULONG MaxReadLength;
} BUFFER_SIZE_INFO, *PBUFFER_SIZE_INFO;


enum DRIVER_STATE {Stopped, Started, Removed};

//++
// Description:
//		Driver-defined structure used to hold 
//		miscellaneous device information.
//
// Access:
//		Allocated from NON-PAGED POOL
//		Available at any IRQL
//--
typedef struct _DEVICE_EXTENSION 
{
	PDEVICE_OBJECT pDevice;
	PDEVICE_OBJECT pNextLowerDriver;
	
	PDEVICE_OBJECT pTargetDevice;
	DRIVER_STATE state;		// current state of driver
	UNICODE_STRING symLinkName;
	BUFFER_SIZE_INFO bufferInfo;
	UNICODE_STRING	devName;
	
	// Items used for event logging
	ULONG IrpSequenceNumber;
	UCHAR IrpRetryCount;
    ULONG ChildCount; 
    ULONG NuberOfVIDpnSources;

    DXGKRNL_INTERFACE ddiCallback;   // Kernel runtime callbacks

} DEVICE_EXTENSION, *PDEVICE_EXTENSION;
//*******************************************************************************

typedef struct F_DEVICE_EXTENSION 
{
	PDEVICE_OBJECT pDevice;
	PDEVICE_OBJECT pNextLowerDriver;
	PDEVICE_OBJECT pTargetDevice;
	DRIVER_STATE state;		// current state of driver
	IO_REMOVE_LOCK RemoveLock;
	BUFFER_SIZE_INFO bufferInfo;
	
	// Items used for event logging
	ULONG IrpSequenceNumber;
	UCHAR IrpRetryCount;
	

} FILTER_DEVICE_EXTENSION, *FILTER_PDEVICE_EXTENSION;




#define IOCTL_GET_MAX_BUFFER_SIZE		\
	CTL_CODE( FILE_DEVICE_UNKNOWN, 0x803,	\
		METHOD_BUFFERED, FILE_ANY_ACCESS )

#define NO_BUFFER_LIMIT	((ULONG)(-1))

#define IOCTL_VIDEO_CREATE_CHILD \
    CTL_CODE( FILE_DEVICE_VIDEO, 0x0F, METHOD_NEITHER, FILE_ANY_ACCESS  )

#define IOCTL_VIDEO_INTERFACE_CHILD \
    CTL_CODE( FILE_DEVICE_VIDEO, 0x815, METHOD_NEITHER, FILE_READ_ACCESS | FILE_WRITE_ACCESS  )


static DRIVER_INITIALIZATION_DATA Primary_Structure_Address;
static ULONG m_Buffer;
#define MANIPULATED_TAG '_ITA'
