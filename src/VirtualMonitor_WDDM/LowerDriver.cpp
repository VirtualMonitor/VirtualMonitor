/* This file is part of VirtualMonitor.
 *
 * CopyRight (c) 2013-2013 Anshul Makkar (www.justkernel.com)
 *
 * VirtualMonitor  is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * VirtualMonitor is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with VirtualMonitor.  If not, see <http://www.gnu.org/licenses/>.
 */

/******************************************************************************
  Child Enumeration Process (Source WinDDK)

The following sequence of steps describes how the display port driver, display miniport driver, and video present network (VidPN) manager collaborate at
initialization time to enumerate child devices of a display adapter.

1) The display port driver calls the display miniport driver's DxgkDdiStartDevice function.
DxgkDdiStartDevice returns (in the NumberOfChildren parameter) the number of devices that are (or could become by docking) children
of the display adapter. DxgkDdiStartDevice also returns (in the NumberOfVideoPresentSources parameter) the number N of video present
sources supported by the display adapter. Those video present sources will subsequently be identified by the numbers 0, 1, ... N -1.
2) The display port driver calls the display miniport driver's DxgkDdiQueryChildRelations function, which enumerates child devices
of the display adapter. DxgkDdiQueryChildRelations fills in an array of DXGK_CHILD_DESCRIPTOR structures: one for each child device.
Note that all child devices of the display adapter are on-board: monitors and other external devices that connect to the display adapter
are not considered child devices.
3) For each child device (enumerated as described in Step 1) that has an HPD awareness value of HpdAwarenessInterruptible or HpdAwarenessPolled,
the display port driver calls the display miniport driver's DxgkDdiQueryChildStatus function to determine whether the child device has
an external device connected to it.
4) The display port driver creates a PDO for each child device that satisfies one of the following conditions:
--> The child device has an HPD awareness value of HpdAwarenessAlwaysConnected.
-->The child device has an HPD awareness value of HpdAwarenessPolled or HpdAwarenessInterruptible, and the operating system knows from
a previous query or notification that the child device has an external device connected.
5) The display port driver calls the display miniport driver's DxgkDdiQueryDeviceDescriptor function for each child device that satisfies
one of the following conditions:
-->The child device is known to have an external device connected.
-->The child device is assumed to have an external device connected.
--> The child device has a type of TypeOther.
6) The VidPN manager obtains identifiers for all of the video present sources and video present targets supported by the display adapter.
The video present sources are identified by the numbers 0, 1, ... N - 1, where N is the number of sources returned by the display miniport
driver's DxgkDdiStartDevice function. The video present targets have unique integer identifiers that were previously created by the display miniport
driver during DxgkDdiQueryChildRelations. Each child device of type TypeVideoOutput is associated with a video present target,
and the ChildUid member of the child device's DXGK_CHILD_DESCRIPTOR structure is used as the identifier for the video present target.
*******************************************************************************/
#include "Driver.h"
#include <Dispmprt.h.>

#define JUSTKERN_KM '_YAR'
// Forward declarations
#define DXG_REG_PATH L"\\REGISTRY\\MACHINE\\SYSTEM\\CURRENTCONTROLSET\\SERVICES\\DXGKrnl"
#define DBG 2
#define HORZ_RESOLUTION  800
#define VERT_RESOLUTION  600
#define BIT_DEPTH 4
//#define CHILD_UDID 0xd2d0001
#define CHILD_UDID 0xAB1010B
static PDEVICE_EXTENSION pHwDeviceExtension;

/*Resolution to be supported*/
D3DKMDT_GRAPHICS_RENDERING_FORMAT g_VidPnSourceMode = {
                                                        {HORZ_RESOLUTION, VERT_RESOLUTION},		// PrimSurfSize
                                                        {HORZ_RESOLUTION, VERT_RESOLUTION},		// VisibleRegionSize
                                                         HORZ_RESOLUTION * BIT_DEPTH,            // Stride
                                                         D3DDDIFMT_A8R8G8B8,						// PixelFormat
                                                         D3DKMDT_CB_SRGB,						// ColorBasis
                                                         D3DKMDT_PVAM_DIRECT
                                                      };
                                                      				// PixelValueAccessMode
SIZE_T g_NumAllVidPnSourceModes[1] = {1};

D3DKMDT_GRAPHICS_RENDERING_FORMAT*  g_AllVidPnSourceModes = {&g_VidPnSourceMode};

/*dummy is this the place where i can place my buffer. I doubt*/
D3DKMDT_VIDEO_SIGNAL_INFO
g_VidPnTargetMode = {
                     D3DKMDT_VSS_VESA_DMT,					// VideoStandard
                     {1344, 806},							// TotalSize
                     {HORZ_RESOLUTION, VERT_RESOLUTION},     // ActiveSize
                     {60004, 1000},							// VSyncFreq
                     {48363, 1},								// HSyncFreq
                     65000000,								// PixelRate
                     D3DDDI_VSSLO_PROGRESSIVE
                    };				// ScanLineOrdering

D3DKMDT_VIDEO_SIGNAL_INFO*  g_AllVidPnTargetModes = {&g_VidPnTargetMode};

extern "C" NTSTATUS DriverEntry(IN PDRIVER_OBJECT pDriverObject,IN PUNICODE_STRING pRegistryPath	);
extern "C" {
    DXGKDDI_ADD_DEVICE                      justkernAddDevice;
    DXGKDDI_START_DEVICE                    justkernStartDevice;
    DXGKDDI_STOP_DEVICE                     justkernStopDevice;
    DXGKDDI_REMOVE_DEVICE                   justkernRemoveDevice;
    DXGKDDI_DISPATCH_IO_REQUEST             justkernDispatchIoRequest;
    DXGKDDI_INTERRUPT_ROUTINE               justkernInterruptRoutine;
    DXGKDDI_DPC_ROUTINE                     justkernDpcRoutine;
    DXGKDDI_QUERY_CHILD_RELATIONS           justkernQueryChildRelations;
    DXGKDDI_QUERY_CHILD_STATUS              justkernQueryChildStatus;
    DXGKDDI_QUERY_DEVICE_DESCRIPTOR         justkernQueryDeviceDescriptor;
    DXGKDDI_SET_POWER_STATE                 justkernSetPowerState;
    DXGKDDI_NOTIFY_ACPI_EVENT               justkernNotifyAcpiEvent;
    DXGKDDI_RESET_DEVICE                    justkernResetDevice;
    DXGKDDI_UNLOAD                          justkernUnload;
    DXGKDDI_QUERY_INTERFACE                 justkernQueryInterface;
    DXGKDDI_CONTROL_ETW_LOGGING             justkernD3DDDIControlEtwLogging;
    DXGKDDI_QUERYADAPTERINFO                justkernD3DDDIQueryAdapterInfo;
    DXGKDDI_CREATEDEVICE                    justkernD3DDDICreateDevice;
    DXGKDDI_CREATEALLOCATION                justkernD3DDDICreateAllocation;
    DXGKDDI_DESTROYALLOCATION               justkernD3DDDIDestroyAllocation;
    DXGKDDI_DESCRIBEALLOCATION              justkernD3DDDIDescribeAllocation;
    DXGKDDI_GETSTANDARDALLOCATIONDRIVERDATA justkernD3DDDIGetStandardAllocationDriverData;
    DXGKDDI_ACQUIRESWIZZLINGRANGE           justkernD3DDDIAcquireSwizzlingRange;
    DXGKDDI_RELEASESWIZZLINGRANGE           justkernD3DDDIReleaseSwizzlingRange;
    DXGKDDI_PATCH                           justkernD3DDDIPatch;
    DXGKDDI_SUBMITCOMMAND                   justkernD3DDDISubmitCommand;
    DXGKDDI_PREEMPTCOMMAND                  justkernD3DDDIPreemptCommand;
    DXGKDDI_BUILDPAGINGBUFFER               justkernD3DDDIBuildPagingBuffer;
    DXGKDDI_SETPALETTE                      justkernD3DDDISetPalette;
    DXGKDDI_SETPOINTERPOSITION              justkernD3DDDISetPointerPosition;
    DXGKDDI_SETPOINTERSHAPE                 justkernD3DDDISetPointerShape;
    DXGKDDI_ISSUPPORTEDVIDPN                justkernD3DDDIIsSupportedVidPn;
    DXGKDDI_RECOMMENDFUNCTIONALVIDPN        justkernD3DDDIRecommendFunctionalVidPn;
    DXGKDDI_ENUMVIDPNCOFUNCMODALITY         justkernD3DDDIEnumVidPnCofuncModality;
    DXGKDDI_RESETFROMTIMEOUT                justkernD3DDDIResetFromTimeout;
    DXGKDDI_RESTARTFROMTIMEOUT              justkernD3DDDIRestartFromTimeout;
    DXGKDDI_ESCAPE                          justkernD3DDDIEscape;
    DXGKDDI_COLLECTDBGINFO                  justkernD3DDDICollectDbgInfo;
    DXGKDDI_SETVIDPNSOURCEADDRESS           justkernD3DDDISetVidPnSourceAddress;
    DXGKDDI_SETVIDPNSOURCEVISIBILITY        justkernD3DDDISetVidPnSourceVisibility;
    DXGKDDI_COMMITVIDPN                     justkernD3DDDICommitVidPn;
    DXGKDDI_UPDATEACTIVEVIDPNPRESENTPATH    justkernD3DDDIUpdateActiveVidPnPresentPath;
    DXGKDDI_RECOMMENDMONITORMODES           justkernD3DDDIRecommendMonitorModes;
    DXGKDDI_RECOMMENDVIDPNTOPOLOGY          justkernD3DDDIRecommendVidPnTopology;
    DXGKDDI_GETSCANLINE                     justkernD3DDDIGetScanLine;
    DXGKDDI_CONTROLINTERRUPT                justkernD3DDDIControlInterrupt;
    DXGKDDI_QUERYVIDPNHWCAPABILITY          justkernD3DDDIQueryVidPnHWCapability;
    //
    // Device functions
    //
    DXGKDDI_QUERYCURRENTFENCE               justkernDXGKDDI_QUERYCURRENTFENCE;
    DXGKDDI_DESTROYDEVICE                   justkernD3DDDIDestroyDevice;
    DXGKDDI_OPENALLOCATIONINFO              justkernD3DDDIOpenAllocation;
    DXGKDDI_CLOSEALLOCATION                 justkernD3DDDICloseAllocation;
    DXGKDDI_RENDER                          justkernD3DDDIRender;
    DXGKDDI_PRESENT                         justkernD3DDDIPresent;

    //
    // Context functions
    //
    DXGKDDI_CREATECONTEXT                   justkernD3DDDICreateContext;
    DXGKDDI_DESTROYCONTEXT                  justkernD3DDDIDestroyContext;
    DXGKDDI_CREATEOVERLAY                   justkernD3DDICreateOverlay;
    DXGKDDI_STOPCAPTURE                     justkernD3DDIStopCapture;
    DXGKDDI_UPDATEOVERLAY                   justkernD3DDIUpdateOverlay;
    DXGKDDI_FLIPOVERLAY                     justkernD3DDIFlipOverlay;
    DXGKDDI_DESTROYOVERLAY                  justkernD3DDIDestroyOverlay;
    DXGKDDI_LINK_DEVICE                     justkernD3DDILinkDevice;
}

extern "C" {
    NTSTATUS ManipulateDriverInitData(IN PDRIVER_OBJECT DriverObject, IN PUNICODE_STRING RegistryPath,
                                      DRIVER_INITIALIZATION_DATA *Structure_Address);
    static VOID DriverUnload(IN PDRIVER_OBJECT	pDriverObject);
    static NTSTATUS DispatchPassThru (IN PDEVICE_OBJECT	pDevFilterObj, IN PIRP pIrp);
    NTSTATUS GenericCompletion(IN PDEVICE_OBJECT pDevObj, IN PIRP pIrp, IN PVOID pContext);
    NTSTATUS CreateDevice (PDRIVER_OBJECT pDriverObject);
    VOID DRIVER_IMAGE_NOTIFY_ROUTINE (IN PUNICODE_STRING  FullImageName,IN HANDLE  ProcessId, // where image is mapped
                                      IN PIMAGE_INFO  ImageInfo);
}
#pragma alloc_text(PAGE,justkernAddDevice)
#pragma alloc_text(PAGE,justkernStartDevice)
#pragma alloc_text(PAGE,justkernStopDevice)
#pragma alloc_text(PAGE,justkernRemoveDevice)
#pragma alloc_text(PAGE,justkernDispatchIoRequest)
#pragma alloc_text(PAGE,justkernQueryInterface)
#pragma alloc_text(PAGE,justkernQueryChildRelations)
#pragma alloc_text(PAGE,justkernQueryChildStatus)
#pragma alloc_text(PAGE,justkernQueryDeviceDescriptor)
#pragma alloc_text(PAGE,justkernUnload)
#pragma alloc_text(INIT,DriverEntry)

/**
 *******************************************************************************
 @brief	    DriverEntry
 @remark     Main Driver Entry
 @param[in]  PDRIVER_OBJECT pDriverObject - Passed from I/O Manager
 @return     STATUS_SUCCESS or STATUS_UNSUCCESSFUL. For more detailed failure info
 *******************************************************************************
 */
extern "C" NTSTATUS DriverEntry (IN PDRIVER_OBJECT pDriverObject, IN PUNICODE_STRING pRegistryPath)
{
    DbgPrint("JustKernel: DriverEntry\n");

    ULONG ulDeviceNumber = 0;
    NTSTATUS status = STATUS_SUCCESS;
    UNICODE_STRING devName;
    UNICODE_STRING symLinkName;
    PDEVICE_EXTENSION pDevExt;
    PDEVICE_OBJECT DevObj;


    // Form the internal Device Name
    RtlInitUnicodeString(&devName, L"\\Device\\dlkmd_1");
    RtlInitUnicodeString(&symLinkName, L"\\DosDevices\\LoFilter_1");
    //etwregister missing

    status = IoCreateDevice(pDriverObject,//creating main device objct
                            sizeof(DEVICE_EXTENSION), &devName, FILE_DEVICE_UNKNOWN, 0, FALSE, &DevObj);
    if (!NT_SUCCESS(status))
    {
        DbgPrint("LOWERFILTER: Iocreate failed\n");
        return status;
    }

    pDevExt = (PDEVICE_EXTENSION)DevObj->DeviceExtension;
    pDevExt->pDevice = DevObj;	// back pointer

    status = CreateDevice(pDriverObject);			//create Device object

    status = IoCreateSymbolicLink( &symLinkName, &devName );
    if (!NT_SUCCESS(status))
    {
        // if it fails now, must delete Device object
        DbgPrint("LOWERFILTER: Iocreatesymboliclink failed\n");
        DbgPrint("error Code value == %08x\n", status);
        IoDeleteDevice( DevObj );
        IoDeleteSymbolicLink(&symLinkName);
        return status;
    }

    // Assume (initially) nothing is overridden
    for (int i=0; i<=IRP_MJ_MAXIMUM_FUNCTION; i++)
        if (i!=IRP_MJ_POWER)
            pDriverObject->MajorFunction[i] = DispatchPassThru;
    pDriverObject->DriverUnload = DriverUnload;

    // Initialize Event logging
    InitializeEventLog(pDriverObject);
    return status;
}

NTSTATUS DispatchPassThru(IN PDEVICE_OBJECT pDevObj, IN PIRP pIrp)
{
    ULONG	IOCTL_FunctionCode;
    FILTER_PDEVICE_EXTENSION pFilterExt = (FILTER_PDEVICE_EXTENSION)pDevObj->DeviceExtension;
    PIO_STACK_LOCATION pIrpStack = IoGetCurrentIrpStackLocation( pIrp );
    IOCTL_FunctionCode = pIrpStack->Parameters.DeviceIoControl.IoControlCode;
    DWORD *Address;
    DWORD der=0x2345;

    DbgPrint("LOWERFILTER: Current Going IOCTL Function Code is  : %x\n",IOCTL_FunctionCode);

    switch(IOCTL_FunctionCode)
    {
        /*IOCTL_VIDEO_CREATE_CHILD_NODE : defined in driver.h. Kind of hacky. Found through RE.
          @todo Details , I am tryig to find.*/
        case  2293823:
            Address = (DWORD*)pIrp->UserBuffer;
            *Address = (DWORD)ManipulateDriverInitData;
            pIrp->IoStatus.Information =4;
            pIrp->IoStatus.Status = STATUS_SUCCESS;
            IoCompleteRequest (pIrp, IO_NO_INCREMENT);
            break;
        default:
            /**Dispatch pass thru -->pass irp to dxgkrnl
             * Copy args to the next level
             * IoCopyCurrentIrpStackLocationToNext(pIrp);
             */
            PIO_STACK_LOCATION pNextIrpStack = IoGetNextIrpStackLocation( pIrp );
            *pNextIrpStack = *pIrpStack;

            // Set up a completion routine to handle the bubbling
            //	of the "pending" mark of an IRP
            IoSetCompletionRoutine(pIrp, GenericCompletion, NULL, TRUE, TRUE, TRUE );
            // Pass the IRP to the target.
            return IoCallDriver(pFilterExt->pNextLowerDriver, pIrp );
    }
    return STATUS_SUCCESS;
}

//***********************************************************************************************
NTSTATUS GenericCompletion(
        IN PDEVICE_OBJECT pDevObj,
        IN PIRP pIrp,
        IN PVOID pContext ) {

    if ( pIrp->PendingReturned )
        IoMarkIrpPending( pIrp );

    return STATUS_SUCCESS;
}

VOID DriverUnload(IN PDRIVER_OBJECT	pDriverObject)
{
    UNICODE_STRING symLinkName;
    FILTER_PDEVICE_EXTENSION pDevFilterExt;
    PDEVICE_OBJECT pDevFilterObj;
    DbgPrint("LOWERFILTER: DriverUnload\n");
    PDEVICE_OBJECT deviceObject = pDriverObject->DeviceObject;
    RtlInitUnicodeString(&symLinkName, L"\\DosDevices\\LoFilter_1");
    IoDeleteSymbolicLink(&symLinkName);
}

NTSTATUS CreateDevice (PDRIVER_OBJECT pDriverObject)
{
    NTSTATUS status;

    FILTER_PDEVICE_EXTENSION pDevFilterExt;
    PDEVICE_OBJECT pDevFilterObj;
    UNICODE_STRING strDxgKrnl;
    PFILE_OBJECT FileObject;
    PDEVICE_OBJECT DeviceObject;
    UNICODE_STRING ddk;
    //IRP pIrp;
    IO_STATUS_BLOCK  ios;
    KEVENT evt;
    PIRP pIrp;
    UNICODE_STRING driverPath;

    RtlInitUnicodeString(&ddk, L"\\Device\\Dxgkrnl\\0");
    RtlInitUnicodeString(&driverPath, DXG_REG_PATH);
    KeInitializeEvent(&evt, NotificationEvent, FALSE);
    status = ZwLoadDriver(&driverPath);
    if(!NT_SUCCESS(status))
    {
        DbgPrint("LOWERFILTER:Failed to load the DxgKernel driver %d\n", error);
    }
    status = IoGetDeviceObjectPointer(&ddk, FILE_ALL_ACCESS, &FileObject, &DeviceObject);
    if (!NT_SUCCESS(status))
    {
        // if it fails now, must delete Device object
        DbgPrint("error Code value == %08x\n", status);
        DbgPrint("LOWERFILTER: IoGetDeviceObjectPointer failed\n");
        DriverUnload (pDriverObject);
        return status;
    }
    /** callng dxgkrnl driver with IOCTL : IOCTL_VIDEO_CREATE_CHILD */
    pIrp = IoBuildDeviceIoControlRequest(IOCTL_VIDEO_CREATE_CHILD,DeviceObject, NULL, 0, &m_Buffer, sizeof(m_Buffer), TRUE, &evt, &ios);
    PIO_STACK_LOCATION pIrpStack = IoGetCurrentIrpStackLocation(pIrp);		//debug
    if (pIrp != NULL)
    {
        status = IoCallDriver(DeviceObject, pIrp);
        if (status == STATUS_PENDING)
        {
            KeWaitForSingleObject(&evt, Executive, KernelMode, FALSE, NULL);
            status = ios.Status;
        }
    }

    // Now create the device
    status = IoCreateDevice(pDriverObject, sizeof(FILTER_DEVICE_EXTENSION), NULL, FILE_DEVICE_UNKNOWN, 0, TRUE, &pDevFilterObj);
    if (!NT_SUCCESS(status))
    {
        DbgPrint("LOWERFILTER: Filter Iocreate failed\n");
        return status;
    }
    // Initialize the Device Extension
    pDevFilterExt = (FILTER_PDEVICE_EXTENSION)pDevFilterObj->DeviceExtension;
    pDevFilterExt->pDevice = pDevFilterObj;	// back pointer
    pDevFilterExt->pTargetDevice = DeviceObject;			//saving Dxgkrnl device object pointer

    IoInitializeRemoveLock (&pDevFilterExt->RemoveLock ,PORTIO_TAG, 1 /* MaxLockedMinutes */,5);
    /* attach lowerfilter driver to dxgkrnl driver. Our driver will be on top of dxgkernel. */
    pDevFilterExt->pNextLowerDriver = IoAttachDeviceToDeviceStack(pDevFilterObj, DeviceObject);
    pDevFilterObj->StackSize =pDevFilterExt->pNextLowerDriver->StackSize+1;
    pDevFilterObj->Flags |= (pDevFilterExt->pNextLowerDriver->Flags & (DO_BUFFERED_IO | DO_DIRECT_IO));// | POWER_INRUSH | POWER_PAGABLE));
    pDevFilterObj->DeviceType = pDevFilterExt->pNextLowerDriver->DeviceType;
    pDevFilterObj->Characteristics = pDevFilterExt->pNextLowerDriver->Characteristics;
    return STATUS_SUCCESS;
}

NTSTATUS ManipulateDriverInitData(IN PDRIVER_OBJECT DriverObject, IN PUNICODE_STRING RegistryPath, DRIVER_INITIALIZATION_DATA *Manipulated_Structure_Address )
{
    Primary_Structure_Address = *Manipulated_Structure_Address;
    NTSTATUS (*pt2Function)(DWORD,DWORD,DRIVER_INITIALIZATION_DATA *);
    DbgPrint("LOWERFILTER: ManipulateDriverInitData is called by Primary: Ya hooo \n");

    DWORD rv_Obj = (DWORD) DriverObject;
    DWORD Reg_Path = (DWORD) RegistryPath;
    memcpy(&pt2Function,&m_Buffer,4);

    Manipulated_Structure_Address->Version				 = DXGKDDI_INTERFACE_VERSION;;
    Manipulated_Structure_Address->DxgkDdiAddDevice				= justkernAddDevice;
    Manipulated_Structure_Address->DxgkDdiStartDevice             = justkernStartDevice;
    Manipulated_Structure_Address->DxgkDdiStopDevice              = justkernStopDevice;
    Manipulated_Structure_Address->DxgkDdiRemoveDevice			 = justkernRemoveDevice;
    Manipulated_Structure_Address->DxgkDdiQueryInterface          = justkernQueryInterface;
    Manipulated_Structure_Address->DxgkDdiQueryAdapterInfo        = justkernD3DDDIQueryAdapterInfo;
    Manipulated_Structure_Address->DxgkDdiInterruptRoutine        = justkernInterruptRoutine;
    Manipulated_Structure_Address->DxgkDdiDpcRoutine				 = justkernDpcRoutine;
    Manipulated_Structure_Address->DxgkDdiQueryChildRelations		 = justkernQueryChildRelations;
    Manipulated_Structure_Address->DxgkDdiQueryChildStatus        = justkernQueryChildStatus;
    Manipulated_Structure_Address->DxgkDdiQueryDeviceDescriptor   = justkernQueryDeviceDescriptor;
    Manipulated_Structure_Address->DxgkDdiDispatchIoRequest       = justkernDispatchIoRequest;
    Manipulated_Structure_Address->DxgkDdiSetPowerState           = justkernSetPowerState;
    Manipulated_Structure_Address->DxgkDdiNotifyAcpiEvent         = justkernNotifyAcpiEvent;
    Manipulated_Structure_Address->DxgkDdiResetDevice             = justkernResetDevice;
    Manipulated_Structure_Address->DxgkDdiUnload                  = justkernUnload;
    Manipulated_Structure_Address->DxgkDdiControlEtwLogging       = justkernD3DDDIControlEtwLogging;
    Manipulated_Structure_Address->DxgkDdiCreateDevice            = justkernD3DDDICreateDevice;
    Manipulated_Structure_Address->DxgkDdiCreateAllocation        = justkernD3DDDICreateAllocation;
    Manipulated_Structure_Address->DxgkDdiDestroyAllocation       = justkernD3DDDIDestroyAllocation;
    Manipulated_Structure_Address->DxgkDdiDescribeAllocation      = justkernD3DDDIDescribeAllocation;
    Manipulated_Structure_Address->DxgkDdiGetStandardAllocationDriverData = justkernD3DDDIGetStandardAllocationDriverData;
    Manipulated_Structure_Address->DxgkDdiAcquireSwizzlingRange   = justkernD3DDDIAcquireSwizzlingRange;
    Manipulated_Structure_Address->DxgkDdiReleaseSwizzlingRange   = justkernD3DDDIReleaseSwizzlingRange;
    Manipulated_Structure_Address->DxgkDdiPatch                   = justkernD3DDDIPatch;
    Manipulated_Structure_Address->DxgkDdiSubmitCommand           = justkernD3DDDISubmitCommand;
    Manipulated_Structure_Address->DxgkDdiPreemptCommand          = justkernD3DDDIPreemptCommand;
    Manipulated_Structure_Address->DxgkDdiBuildPagingBuffer       = justkernD3DDDIBuildPagingBuffer;
    Manipulated_Structure_Address->DxgkDdiSetPalette              = justkernD3DDDISetPalette;
    Manipulated_Structure_Address->DxgkDdiSetPointerShape         = justkernD3DDDISetPointerShape;
    Manipulated_Structure_Address->DxgkDdiSetPointerPosition      = justkernD3DDDISetPointerPosition;
    Manipulated_Structure_Address->DxgkDdiRecommendFunctionalVidPn = justkernD3DDDIRecommendFunctionalVidPn;
    Manipulated_Structure_Address->DxgkDdiEnumVidPnCofuncModality  = justkernD3DDDIEnumVidPnCofuncModality;
    Manipulated_Structure_Address->DxgkDdiIsSupportedVidPn         = justkernD3DDDIIsSupportedVidPn;
    Manipulated_Structure_Address->DxgkDdiQueryCurrentFence       = justkernDXGKDDI_QUERYCURRENTFENCE;
    Manipulated_Structure_Address->DxgkDdiOpenAllocation          = justkernD3DDDIOpenAllocation;
    Manipulated_Structure_Address->DxgkDdiCloseAllocation          = justkernD3DDDICloseAllocation;
    Manipulated_Structure_Address->DxgkDdiRender                  = justkernD3DDDIRender;
    Manipulated_Structure_Address->DxgkDdiPresent                 = justkernD3DDDIPresent;
    Manipulated_Structure_Address->DxgkDdiResetFromTimeout        = justkernD3DDDIResetFromTimeout;
    Manipulated_Structure_Address->DxgkDdiRestartFromTimeout      = justkernD3DDDIRestartFromTimeout;
    Manipulated_Structure_Address->DxgkDdiEscape                  = justkernD3DDDIEscape;
    Manipulated_Structure_Address->DxgkDdiCollectDbgInfo          = justkernD3DDDICollectDbgInfo;
    Manipulated_Structure_Address->DxgkDdiControlInterrupt        = justkernD3DDDIControlInterrupt;
    Manipulated_Structure_Address->DxgkDdiGetScanLine             = justkernD3DDDIGetScanLine;
    Manipulated_Structure_Address->DxgkDdiSetVidPnSourceAddress			= justkernD3DDDISetVidPnSourceAddress;
    Manipulated_Structure_Address->DxgkDdiSetVidPnSourceVisibility		= justkernD3DDDISetVidPnSourceVisibility;
    Manipulated_Structure_Address->DxgkDdiUpdateActiveVidPnPresentPath	= justkernD3DDDIUpdateActiveVidPnPresentPath;
    Manipulated_Structure_Address->DxgkDdiCommitVidPn					= justkernD3DDDICommitVidPn;
    Manipulated_Structure_Address->DxgkDdiRecommendMonitorModes			= justkernD3DDDIRecommendMonitorModes;
    Manipulated_Structure_Address->DxgkDdiRecommendVidPnTopology		= justkernD3DDDIRecommendVidPnTopology;
    Manipulated_Structure_Address->DxgkDdiCreateContext					= justkernD3DDDICreateContext;
    Manipulated_Structure_Address->DxgkDdiDestroyContext				= justkernD3DDDIDestroyContext;
    Manipulated_Structure_Address->DxgkDdiDestroyDevice					= justkernD3DDDIDestroyDevice;
    Manipulated_Structure_Address->DxgkDdiStopCapture					= justkernD3DDIStopCapture;
    Manipulated_Structure_Address->DxgkDdiCreateOverlay					= justkernD3DDICreateOverlay;
    Manipulated_Structure_Address->DxgkDdiUpdateOverlay					= justkernD3DDIUpdateOverlay;
    Manipulated_Structure_Address->DxgkDdiFlipOverlay					= justkernD3DDIFlipOverlay;
    Manipulated_Structure_Address-> DxgkDdiDestroyOverlay				= justkernD3DDIDestroyOverlay;

    NTSTATUS status = (*pt2Function)(rv_Obj,Reg_Path,Manipulated_Structure_Address);

    return status;
}
/**
 *******************************************************************************
 @brief	    justkernAddDevice
 @remark     function creates a context block for a display adapter and returns
 a handle that represents the display adapter.
 @param[in]  IN_CONST_PDEVICE_OBJECT PhysicalDeviceObject
 @return     STATUS_SUCCESS or STATUS_UNSUCCESSFUL. For more detailed failure info
 *******************************************************************************
 */
NTSTATUS justkernAddDevice(
        IN_CONST_PDEVICE_OBJECT PhysicalDeviceObject,
        OUT PVOID *MiniportDeviceContext
        )
{
    NTSTATUS (*ptrFunction)(IN_CONST_PDEVICE_OBJECT PhysicalDeviceObject,OUT PVOID );
    DbgPrint("LOFILTER: AddDevice called\n");
    memcpy(&ptrFunction,&Primary_Structure_Address.DxgkDdiAddDevice,4);
    NTSTATUS Status = (*ptrFunction)(PhysicalDeviceObject,MiniportDeviceContext);
    return Status;
}

/********************************************************************************
 @brief	    justkernStartDevice
 @remark     function prepares a display adapter to receive I/O requests,
 Explicitly increase the number of child and sources.The display port driver calls the display
 miniport driver's DxgkDdiStartDevice function. DxgkDdiStartDevice returns (in the NumberOfChildren parameter)
 the number of devices that are (or could become by docking) children of the display adapter.
 DxgkDdiStartDevice also returns (in the NumberOfVideoPresentSources parameter) the number N of video present
 sources supported by the display adapter. Those video present sources will subsequently be identified by the numbers 0, 1, ... N -1.
 @param[in]  PDXGK_START_INFO DxgkStartInfo
 @return     STATUS_SUCCESS or STATUS_UNSUCCESSFUL. For more detailed failure info
 ********************************************************************************/
NTSTATUS
justkernStartDevice(IN PVOID MiniportDeviceContext, IN PDXGK_START_INFO DxgkStartInfo, IN PDXGKRNL_INTERFACE DxgkInterface,
                    OUT PULONG NumberOfVideoPresentSources, OUT PULONG NumberOfChildren)
{
    ULONG temp = 0;
    //PDEVICE_EXTENSION *ppExtensionArray = (PDEVICE_EXTENSION*)MiniportDeviceContext;
    NTSTATUS (*justkernStartDevice_ptr)(IN PVOID , IN PDXGK_START_INFO , IN PDXGKRNL_INTERFACE ,OUT PULONG ,OUT PULONG );

    PAGED_CODE();
    DbgPrint("LOFILTER: StartDeviceCalled\n");
    pHwDeviceExtension = (PDEVICE_EXTENSION)ExAllocatePoolWithTag(NonPagedPool, sizeof(DEVICE_EXTENSION), JUSTKERN_KM);
    if (pHwDeviceExtension == NULL)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(pHwDeviceExtension, sizeof(DEVICE_EXTENSION));
    pHwDeviceExtension->ddiCallback = *DxgkInterface;

    memcpy(&justkernStartDevice_ptr, &Primary_Structure_Address.DxgkDdiStartDevice,4);
    NTSTATUS Status = (*justkernStartDevice_ptr)(MiniportDeviceContext, DxgkStartInfo, DxgkInterface, NumberOfVideoPresentSources, NumberOfChildren);
    if(NT_SUCCESS(Status))
    {
        temp = *NumberOfVideoPresentSources;
        DbgPrint("LOFILTER: No.Of VideoPresentSources = %d\n", temp);
        temp++;
        *NumberOfVideoPresentSources = temp;
        temp = *NumberOfChildren;
        DbgPrint("LOFILTER: NoOfChildren = %d\n", temp);
        temp++;
        *NumberOfChildren = temp;
        pHwDeviceExtension->ChildCount =*NumberOfChildren;
        pHwDeviceExtension->NuberOfVIDpnSources =*NumberOfVideoPresentSources;
        ///ppExtensionArray[2] = pHwDeviceExtension;		//HRD
    }

    return Status;
}

NTSTATUS justkernStopDevice(IN PVOID MiniportDeviceContext )
{
    NTSTATUS (*justkernStopDevice_ptr)(IN PVOID );
    PAGED_CODE();
    DbgPrint("LOFILTER: StopDevice called\n");
    memcpy(&justkernStopDevice_ptr,&Primary_Structure_Address.DxgkDdiStopDevice,4);
    NTSTATUS Status = (*justkernStopDevice_ptr)( MiniportDeviceContext );
    return Status;
}

NTSTATUS justkernRemoveDevice(IN PVOID MiniportDeviceContext)
{
    NTSTATUS (*justkernRemoveDevice_ptr)(IN PVOID );
    PAGED_CODE();
    DbgPrint("LOFILTER: Remove Device called\n");
    memcpy(&justkernRemoveDevice_ptr,&Primary_Structure_Address.DxgkDdiRemoveDevice,4);
    NTSTATUS Status = (*justkernRemoveDevice_ptr)( MiniportDeviceContext );
    return Status;
}

NTSTATUS justkernQueryInterface(IN PVOID MiniportDeviceContext,IN PQUERY_INTERFACE QueryInterface )
{
    NTSTATUS (*justkernQueryInterface_ptr)(IN PVOID MiniportDeviceContext,IN PQUERY_INTERFACE QueryInterface );
    PAGED_CODE();
    DbgPrint("LOFILTER: QueryInterface called\n");
    memcpy(&justkernQueryInterface_ptr,&Primary_Structure_Address.DxgkDdiQueryInterface,4);
    NTSTATUS Status = (*justkernQueryInterface_ptr)(MiniportDeviceContext,QueryInterface );
    return Status;
}

NTSTATUS justkernD3DDDIQueryAdapterInfo( VOID *InterfaceContext,CONST DXGKARG_QUERYADAPTERINFO *pDDIQAIData)
{
    DXGK_DRIVERCAPS        *pDriverCaps;
    DXGK_QUERYSEGMENTIN    *pQuerySegmentIn  = (DXGK_QUERYSEGMENTIN *)pDDIQAIData->pInputData;
    DXGK_QUERYSEGMENTOUT   *pQuerySegmentOut = (DXGK_QUERYSEGMENTOUT *)pDDIQAIData->pOutputData;


    DbgPrint("LOFILTER: D3DDDIQueryAdapterInfo called\n");
    NTSTATUS (*justkernD3DDDIQueryAdapterInfo_ptr)( VOID *, CONST DXGKARG_QUERYADAPTERINFO * );
    memcpy(&justkernD3DDDIQueryAdapterInfo_ptr,&Primary_Structure_Address.DxgkDdiQueryAdapterInfo,4);
    NTSTATUS Status = (*justkernD3DDDIQueryAdapterInfo_ptr)(InterfaceContext,pDDIQAIData );
    return Status;
}

/********************************************************************************
 @brief	    justkernQueryChildRelations
 @remark     function enumerates the child devices of a display adapter.
 The display port driver calls the display miniport driver's DxgkDdiQueryChildRelations function,
 which enumerates child devices of the display adapter. DxgkDdiQueryChildRelations fills in an array of
 DXGK_CHILD_DESCRIPTOR structures: one for each child device. Note that all child devices of the
 display adapter are on-board: monitors and other external devices that connect to the display adapter are not
 considered child devices.
 @param[in]  PVOID pvMiniportDeviceContext
 @return     STATUS_SUCCESS or STATUS_UNSUCCESSFUL. For more detailed failure info
 ********************************************************************************/
NTSTATUS justkernQueryChildRelations(IN PVOID pvMiniportDeviceContext,IN OUT PDXGK_CHILD_DESCRIPTOR pChildRelations,IN ULONG ChildRelationsSize )
{
    PDEVICE_EXTENSION *ppExtensionArray = (PDEVICE_EXTENSION*)pvMiniportDeviceContext;
    //PDEVICE_EXTENSION pHwDeviceExtension;
    ULONG RequiredSize = 0;
    PDXGK_CHILD_DESCRIPTOR pDxgkChildDescriptor = NULL;

    NTSTATUS (*justkernQueryChildRelations_ptr)(IN PVOID ,IN OUT PDXGK_CHILD_DESCRIPTOR ,IN ULONG );
    PAGED_CODE();
    DbgPrint("LOFILTER: justkernQueryChildRelations called\n");
    memcpy(&justkernQueryChildRelations_ptr,&Primary_Structure_Address.DxgkDdiQueryChildRelations,4);
    NTSTATUS Status = (*justkernQueryChildRelations_ptr)(pvMiniportDeviceContext ,pChildRelations, ChildRelationsSize);
    if(NT_SUCCESS(Status))
    {
        // Verify the passed in array size.
        RequiredSize = pHwDeviceExtension->ChildCount * sizeof(DXGK_CHILD_DESCRIPTOR);
        DbgPrint("LOFILTER: childcount= %d\n", pHwDeviceExtension->ChildCount);

        if (RequiredSize > ChildRelationsSize)
        {
            return STATUS_BUFFER_TOO_SMALL;
        }

        pDxgkChildDescriptor = (PDXGK_CHILD_DESCRIPTOR) ExAllocatePoolWithTag(PagedPool,sizeof(DXGK_CHILD_DESCRIPTOR),JUSTKERN_KM);
        if (pDxgkChildDescriptor == NULL)
        {
            return STATUS_INSUFFICIENT_RESOURCES;
        }
        memcpy(pDxgkChildDescriptor,&pChildRelations[0],sizeof(DXGK_CHILD_DESCRIPTOR));			//HRD

        memcpy(&pChildRelations[pHwDeviceExtension->ChildCount-1],pDxgkChildDescriptor,sizeof(DXGK_CHILD_DESCRIPTOR));
        pChildRelations[pHwDeviceExtension->ChildCount-1].ChildUid = CHILD_UDID;
        pChildRelations[pHwDeviceExtension->ChildCount-1].AcpiUid = CHILD_UDID;
        //pChildRelations[1].ChildUid = CHILD_UDID;
        ExFreePoolWithTag(pDxgkChildDescriptor, JUSTKERN_KM);
    }
    return Status;
}
/****************************************************************************************************
@Explaination: For each child device (enumerated as described in Step 1) that has an HPD awareness value of HpdAwarenessInterruptible
or HpdAwarenessPolled, the display port driver calls the display miniport driver's DxgkDdiQueryChildStatus function to
determine whether the child device has an external device connected to it.
 ****************************************************************************************************/
NTSTATUS justkernQueryChildStatus( IN PVOID MiniportDeviceContext, IN PDXGK_CHILD_STATUS ChildStatus,IN BOOLEAN NonDestructiveOnly )
{
    NTSTATUS  (*justkernQueryChildStatus_ptr)(IN PVOID ,IN PDXGK_CHILD_STATUS ,IN BOOLEAN );
    PAGED_CODE();
    DbgPrint("LOFILTER: QueryChildStatus called CHILD_UDID=%lx\n", ChildStatus->ChildUid);
    memcpy(&justkernQueryChildStatus_ptr,&Primary_Structure_Address.DxgkDdiQueryChildStatus,4);
    if(ChildStatus->ChildUid == CHILD_UDID)
    {
        ChildStatus->HotPlug.Connected = TRUE;
        return STATUS_SUCCESS;
    }
    NTSTATUS Status =  (*justkernQueryChildStatus_ptr)(MiniportDeviceContext,ChildStatus,NonDestructiveOnly );
    return Status;
}

VOID justkernDpcRoutine(PVOID MiniportDeviceContext)
{
    //	DbgPrint("LOFILTER: DpcRoutine called\n"); dbgprint disble in intrpt routine
    VOID (*justkernDpcRoutine_ptr)(PVOID );
    memcpy(&justkernDpcRoutine_ptr,&Primary_Structure_Address.DxgkDdiDpcRoutine,4);
    (*justkernDpcRoutine_ptr)(MiniportDeviceContext );
}
/******************************************************************************************************
The display port driver calls the display miniport driver's DxgkDdiQueryDeviceDescriptor function for each child device
that satisfies one of the following conditions:
-->The child device is known to have an external device connected.
-->The child device is assumed to have an external device connected.
-->The child device has a type of TypeOther.
*******************************************************************************************************/
NTSTATUS justkernQueryDeviceDescriptor( IN_CONST_PVOID MiniportDeviceContext,IN_ULONG ChildUid,INOUT_PDXGK_DEVICE_DESCRIPTOR DeviceDescriptor )
{
    NTSTATUS (*justkernQueryDeviceDescriptor_ptr)(IN_CONST_PVOID ,IN_ULONG ,INOUT_PDXGK_DEVICE_DESCRIPTOR  );
    PAGED_CODE();
    DbgPrint("LOFILTER:justkernQueryDeviceDescriptor called\n");

    if(ChildUid == CHILD_UDID)
     {
		 /*return from here only as no descriptor data
		 for our udid from miniport */
         return STATUS_MONITOR_NO_MORE_DESCRIPTOR_DATA;
     }

    memcpy(&justkernQueryDeviceDescriptor_ptr,&Primary_Structure_Address.DxgkDdiQueryDeviceDescriptor ,4);
    NTSTATUS Status =  (*justkernQueryDeviceDescriptor_ptr)(MiniportDeviceContext,ChildUid,DeviceDescriptor);
    return Status;
}
//******************************************************************************
BOOLEAN justkernInterruptRoutine( PVOID MiniportDeviceContext, ULONG MessageNumber  )
{
    //DbgPrint("LOFILTER:justkernInterruptRoutine called\n");  dbgprint disable in intrupt routine
    BOOLEAN (*justkernInterruptRoutine_ptr)(PVOID ,ULONG  );
    memcpy(&justkernInterruptRoutine_ptr,&Primary_Structure_Address.DxgkDdiInterruptRoutine ,4);
    BOOLEAN Ret= (*justkernInterruptRoutine_ptr)(MiniportDeviceContext ,MessageNumber  );
    return Ret;
}


//**************************************************************************************

NTSTATUS justkernDispatchIoRequest( IN PVOID MiniportDeviceContext,IN ULONG ViewIndex,IN PVIDEO_REQUEST_PACKET VideoRequestPacket)
{
    NTSTATUS (*justkernDispatchIoRequest_ptr)( IN PVOID ,IN ULONG ,IN PVIDEO_REQUEST_PACKET );
    PAGED_CODE();
    DbgPrint("LOFILTER:justkernDispatchIoRequest called\n");
    memcpy(&justkernDispatchIoRequest_ptr,&Primary_Structure_Address.DxgkDdiDispatchIoRequest ,4);
    NTSTATUS Status =  (*justkernDispatchIoRequest_ptr)(MiniportDeviceContext,ViewIndex,VideoRequestPacket);
    return Status;
}

NTSTATUS justkernSetPowerState( IN PVOID MiniportDeviceContext,IN ULONG HardwareUid,
                                IN DEVICE_POWER_STATE DevicePowerState,IN POWER_ACTION ActionType )
{
    DbgPrint("LOFILTER:justkernSetPowerState called\n");
    NTSTATUS (*justkernSetPowerState_ptr)( IN PVOID ,IN ULONG ,IN DEVICE_POWER_STATE ,IN POWER_ACTION );
    memcpy(&justkernSetPowerState_ptr,&Primary_Structure_Address.DxgkDdiSetPowerState ,4);
    NTSTATUS Status =  (*justkernSetPowerState_ptr)(MiniportDeviceContext,HardwareUid,DevicePowerState,ActionType );
    return Status;
}

NTSTATUS justkernNotifyAcpiEvent(IN PVOID MiniportDeviceContext,IN DXGK_EVENT_TYPE EventType,
                                 IN ULONG Event,IN PVOID Argument,OUT PULONG AcpiFlags )
{
    DbgPrint("LOFILTER:justkernNotifyAcpiEvent called\n");
    NTSTATUS (*justkernNotifyAcpiEvent_ptr)(IN PVOID ,IN DXGK_EVENT_TYPE ,IN ULONG ,IN PVOID ,OUT PULONG );
    memcpy(&justkernNotifyAcpiEvent_ptr,&Primary_Structure_Address.DxgkDdiNotifyAcpiEvent ,4);
    NTSTATUS Status =  (*justkernNotifyAcpiEvent_ptr)(MiniportDeviceContext,EventType,Event,Argument,AcpiFlags  );
    return Status;
}

VOID justkernResetDevice(IN PVOID MiniportDeviceContext)
{
    DbgPrint("LOFILTER:justkernResetDevice called\n");
    VOID (*justkernResetDevice_ptr)(IN PVOID );
    memcpy(&justkernResetDevice_ptr,&Primary_Structure_Address.DxgkDdiResetDevice ,4);
    (*justkernResetDevice_ptr)(MiniportDeviceContext);
}

VOID justkernUnload( VOID )
{
    VOID (*justkernUnload_ptr)( VOID );
    PAGED_CODE();
    DbgPrint("LOFILTER:justkernUnload called\n");
    memcpy(&justkernUnload_ptr,&Primary_Structure_Address.DxgkDdiUnload ,4);
    (*justkernUnload_ptr)();
}

VOID justkernD3DDDIControlEtwLogging(IN BOOLEAN Enable,IN ULONG Flags,IN UCHAR Level )
{
    DbgPrint("LOFILTER:justkernD3DDDIControlEtwLogging called\n");
    VOID (*justkernD3DDDIControlEtwLogging_ptr)(IN BOOLEAN ,IN ULONG ,IN UCHAR  );
    memcpy(&justkernD3DDDIControlEtwLogging_ptr,&Primary_Structure_Address.DxgkDdiControlEtwLogging ,4);
    (*justkernD3DDDIControlEtwLogging_ptr)(Enable, Flags,Level);
}

NTSTATUS justkernD3DDDICreateDevice(VOID *InterfaceContext,DXGKARG_CREATEDEVICE   *pDDICDD)
{
    DbgPrint("LOFILTER:justkernD3DDDICreateDevice called\n");
    NTSTATUS (*justkernD3DDDICreateDevice_ptr)(VOID *InterfaceContext,DXGKARG_CREATEDEVICE   *pDDICDD);
    memcpy(&justkernD3DDDICreateDevice_ptr,&Primary_Structure_Address.DxgkDdiCreateDevice ,4);
    NTSTATUS Status =  (*justkernD3DDDICreateDevice_ptr)(InterfaceContext, pDDICDD  );
    return Status;

}

NTSTATUS justkernD3DDDICreateAllocation(VOID *InterfaceContext,DXGKARG_CREATEALLOCATION   *pDDICAData)
{
    DbgPrint("LOFILTER:justkernD3DDDICreateAllocation called\n");
    NTSTATUS (*justkernD3DDDICreateAllocation_ptr)(VOID *InterfaceContext,DXGKARG_CREATEALLOCATION   *pDDICAData);
    memcpy(&justkernD3DDDICreateAllocation_ptr,&Primary_Structure_Address.DxgkDdiCreateAllocation ,4);
    NTSTATUS Status =  (*justkernD3DDDICreateAllocation_ptr)(InterfaceContext, pDDICAData  );
    return Status;
}

NTSTATUS justkernD3DDDIDestroyAllocation(VOID *InterfaceContext,CONST DXGKARG_DESTROYALLOCATION    *pDDIDAData)
{
    DbgPrint("LOFILTER:justkernD3DDDIDestroyAllocation called\n");
    NTSTATUS (*justkernD3DDDIDestroyAllocation_ptr)(VOID *InterfaceContext,CONST DXGKARG_DESTROYALLOCATION    *pDDIDAData);
    memcpy(&justkernD3DDDIDestroyAllocation_ptr,&Primary_Structure_Address.DxgkDdiDestroyAllocation ,4);
    NTSTATUS Status =  (*justkernD3DDDIDestroyAllocation_ptr)(InterfaceContext,pDDIDAData  );
    return Status;
}

NTSTATUS justkernD3DDDIDescribeAllocation(VOID* InterfaceContext,DXGKARG_DESCRIBEALLOCATION* pDDIDescribeAlloc)
{
    DbgPrint("LOFILTER:justkernD3DDDIDescribeAllocation called\n");
    NTSTATUS (*justkernD3DDDIDescribeAllocation_ptr)(VOID* InterfaceContext,DXGKARG_DESCRIBEALLOCATION* pDDIDescribeAlloc);
    memcpy(&justkernD3DDDIDescribeAllocation_ptr,&Primary_Structure_Address.DxgkDdiDescribeAllocation ,4);
    NTSTATUS Status =  (*justkernD3DDDIDescribeAllocation_ptr)(InterfaceContext,pDDIDescribeAlloc );
    return Status;
}

NTSTATUS justkernD3DDDIGetStandardAllocationDriverData(VOID *InterfaceContext,DXGKARG_GETSTANDARDALLOCATIONDRIVERDATA   *pDDIGetStandardAllocationDriverData)
{
    DbgPrint("LOFILTER:justkernD3DDDIGetStandardAllocationDriverData called\n");
    NTSTATUS (*justkernD3DDDIGetStandardAllocationDriverData_ptr)(VOID *InterfaceContext,DXGKARG_GETSTANDARDALLOCATIONDRIVERDATA   *pDDIGetStandardAllocationDriverData);
    memcpy(&justkernD3DDDIGetStandardAllocationDriverData_ptr,&Primary_Structure_Address.DxgkDdiGetStandardAllocationDriverData ,4);
    NTSTATUS Status =  (*justkernD3DDDIGetStandardAllocationDriverData_ptr)(InterfaceContext,  pDDIGetStandardAllocationDriverData );
    return Status;
}

NTSTATUS justkernD3DDDIAcquireSwizzlingRange(VOID *InterfaceContext,DXGKARG_ACQUIRESWIZZLINGRANGE   *pDDIASRData)
{
    DbgPrint("LOFILTER:justkernD3DDDIAcquireSwizzlingRange called\n");
    NTSTATUS (*justkernD3DDDIAcquireSwizzlingRange_ptr)(VOID *InterfaceContext,DXGKARG_ACQUIRESWIZZLINGRANGE   *pDDIASRData);
    memcpy(&justkernD3DDDIAcquireSwizzlingRange_ptr,&Primary_Structure_Address.DxgkDdiAcquireSwizzlingRange ,4);
    NTSTATUS Status =  (*justkernD3DDDIAcquireSwizzlingRange_ptr)(InterfaceContext,pDDIASRData );
    return Status;
}

NTSTATUS justkernD3DDDIReleaseSwizzlingRange(VOID *InterfaceContext,CONST DXGKARG_RELEASESWIZZLINGRANGE *pDDIRSRData)
{
    DbgPrint("LOFILTER:justkernD3DDDIReleaseSwizzlingRange called\n");

    NTSTATUS (*justkernD3DDDIReleaseSwizzlingRange_ptr)(VOID *InterfaceContext,CONST DXGKARG_RELEASESWIZZLINGRANGE *pDDIRSRData);
    memcpy(&justkernD3DDDIReleaseSwizzlingRange_ptr,&Primary_Structure_Address.DxgkDdiReleaseSwizzlingRange ,4);
    NTSTATUS Status =  (*justkernD3DDDIReleaseSwizzlingRange_ptr)(InterfaceContext,pDDIRSRData );
    return Status;
}

NTSTATUS justkernD3DDDIPatch( VOID *InterfaceContext,CONST DXGKARG_PATCH  *pDDIPatchData)
{
    DbgPrint("LOFILTER:justkernD3DDDIPatch called\n");
    NTSTATUS (*justkernD3DDDIPatch_ptr)(VOID *,CONST DXGKARG_PATCH  *);
    memcpy(&justkernD3DDDIPatch_ptr,&Primary_Structure_Address.DxgkDdiPatch ,4);
    NTSTATUS Status =  (*justkernD3DDDIPatch_ptr)(InterfaceContext,pDDIPatchData );
    return Status;
}

NTSTATUS justkernD3DDDISubmitCommand(VOID *pKmdContext,CONST DXGKARG_SUBMITCOMMAND    *pDDISCData)
{
    //DbgPrint("LOFILTER:justkernD3DDDISubmitCommand called\n");
    NTSTATUS (*justkernD3DDDISubmitCommand_ptr)(VOID *,CONST DXGKARG_SUBMITCOMMAND *);
    memcpy(&justkernD3DDDISubmitCommand_ptr,&Primary_Structure_Address.DxgkDdiSubmitCommand  ,4);
    NTSTATUS Status =  (*justkernD3DDDISubmitCommand_ptr)(pKmdContext,pDDISCData );
    return Status;
}

NTSTATUS justkernD3DDDIPreemptCommand(HANDLE hAdapter, CONST DXGKARG_PREEMPTCOMMAND* pDDIPCData)
{
    DbgPrint("LOFILTER:justkernD3DDDIPreemptCommand called\n");
    NTSTATUS (*justkernD3DDDIPreemptCommand_ptr)(HANDLE , CONST DXGKARG_PREEMPTCOMMAND* );
    memcpy(&justkernD3DDDIPreemptCommand_ptr,&Primary_Structure_Address.DxgkDdiPreemptCommand  ,4);
    NTSTATUS Status =  (*justkernD3DDDIPreemptCommand_ptr)(hAdapter,pDDIPCData );
    return Status;
}

NTSTATUS justkernD3DDDIBuildPagingBuffer(HANDLE hAdapter,DXGKARG_BUILDPAGINGBUFFER  *pParam)
{
    DbgPrint("LOFILTER:justkernD3DDDIBuildPagingBuffer called\n");
    NTSTATUS (*justkernD3DDDIBuildPagingBuffer_ptr)(HANDLE ,DXGKARG_BUILDPAGINGBUFFER  * );
    memcpy(&justkernD3DDDIBuildPagingBuffer_ptr,&Primary_Structure_Address.DxgkDdiBuildPagingBuffer  ,4);
    NTSTATUS Status =  (*justkernD3DDDIBuildPagingBuffer_ptr)(hAdapter,pParam );
    return Status;
}

NTSTATUS APIENTRY justkernD3DDDISetPalette(VOID *InterfaceContext,CONST DXGKARG_SETPALETTE   *pSetPalette)
{
    DbgPrint("LOFILTER:justkernD3DDDISetPalette called\n");
    NTSTATUS (*justkernD3DDDISetPalette_ptr)(VOID * ,CONST DXGKARG_SETPALETTE   * );
    memcpy(&justkernD3DDDISetPalette_ptr,&Primary_Structure_Address.DxgkDdiSetPalette  ,4);
    NTSTATUS Status =  (*justkernD3DDDISetPalette_ptr)(InterfaceContext,pSetPalette );
    return Status;
}

NTSTATUS APIENTRY justkernD3DDDISetPointerShape(VOID *InterfaceContext,CONST DXGKARG_SETPOINTERSHAPE  *pPointerAttributes)
{
    //DbgPrint("LOFILTER:justkernD3DDDISetPointerShape called\n");
    NTSTATUS (*justkernD3DDDISetPointerShape_ptr)(VOID * ,CONST DXGKARG_SETPOINTERSHAPE  * );
    memcpy(&justkernD3DDDISetPointerShape_ptr,&Primary_Structure_Address.DxgkDdiSetPointerShape  ,4);
    NTSTATUS Status =  (*justkernD3DDDISetPointerShape_ptr)(InterfaceContext,pPointerAttributes );
    return Status;
}

NTSTATUS APIENTRY justkernD3DDDISetPointerPosition( VOID *InterfaceContext,CONST DXGKARG_SETPOINTERPOSITION   *pSetPos)
{
    //DbgPrint("LOFILTER:justkernD3DDDISetPointerPosition called\n");
    NTSTATUS (*justkernD3DDDISetPointerPosition_ptr)(VOID * ,CONST DXGKARG_SETPOINTERPOSITION  * );
    memcpy(&justkernD3DDDISetPointerPosition_ptr,&Primary_Structure_Address.DxgkDdiSetPointerPosition  ,4);
    NTSTATUS Status =  (*justkernD3DDDISetPointerPosition_ptr)(InterfaceContext,pSetPos );
    return Status;
}

NTSTATUS APIENTRY  justkernD3DDDIIsSupportedVidPn(CONST HANDLE  hAdapter,OUT DXGKARG_ISSUPPORTEDVIDPN*  pIsSupportedVidPnArg)
{
    //DbgPrint("LOFILTER:justkernD3DDDIIsSupportedVidPn called\n");
    NTSTATUS (*justkernD3DDDIIsSupportedVidPn_ptr)(CONST HANDLE  hAdapter,OUT DXGKARG_ISSUPPORTEDVIDPN*  pIsSupportedVidPnArg );
    memcpy(&justkernD3DDDIIsSupportedVidPn_ptr,&Primary_Structure_Address.DxgkDdiIsSupportedVidPn ,4);
    NTSTATUS Status =  (*justkernD3DDDIIsSupportedVidPn_ptr)(hAdapter,pIsSupportedVidPnArg );
    return Status;
}

NTSTATUS APIENTRY justkernD3DDDIRecommendFunctionalVidPn(CONST HANDLE  hAdapter,CONST DXGKARG_RECOMMENDFUNCTIONALVIDPN* CONST  pRecommendFunctionalVidPnArg )
{
    DbgPrint("LOFILTER:justkernD3DDDIRecommendFunctionalVidPn called\n");
    NTSTATUS (*justkernD3DDDIRecommendFunctionalVidPn_ptr)(CONST HANDLE  hAdapter,CONST DXGKARG_RECOMMENDFUNCTIONALVIDPN* CONST  pRecommendFunctionalVidPnArg );
    memcpy(&justkernD3DDDIRecommendFunctionalVidPn_ptr,&Primary_Structure_Address.DxgkDdiRecommendFunctionalVidPn ,4);
    NTSTATUS Status =  (*justkernD3DDDIRecommendFunctionalVidPn_ptr)(hAdapter,pRecommendFunctionalVidPnArg );
    return Status;
}


/**
 *******************************************************************************
 @brief	    DetermineSourceModeSet
 @remark     Determine SourceMode Set Test
 @param[in]  D3DKMDT_HVIDPN
 @return     STATUS_SUCCESS or STATUS_UNSUCCESSFUL. For more detailed failure info
 *******************************************************************************
 */
NTSTATUS DetermineSourceModeSet(CONST D3DKMDT_HVIDPNi_hVidPn, CONST D3DKMDT_VIDPN_PRESENT_PATH* CONST i_pVidPnPresentPath,
                                CONST DXGK_VIDPN_INTERFACE* CONST i_pDxgkVidPnInterface,
                                __out_bcount(MAX_SOURCE_MODES) BOOLEAN* CONST   o_arr_bSourceModeInclusion,
                                CONST SIZE_T i_sztNumSourceModes)
{
    D3DKMDT_HVIDPNTARGETMODESET	hVidPnTargetModeSet								= NULL;
    CONST DXGK_VIDPNTARGETMODESET_INTERFACE*  pDxgVidPnTargetModeSetInterface	= NULL;
    CONST D3DKMDT_VIDPN_TARGET_MODE*	pPinnedVidPnTargetModeInfo				= NULL;
    D3DKMDT_VIDEO_PRESENT_SOURCE_MODE_ID	SourceModeId		                = 0;
    BOOLEAN Acquire_Flag														= 0;

    NTSTATUS ntStatus = i_pDxgkVidPnInterface->pfnAcquireTargetModeSet(i_hVidPn, i_pVidPnPresentPath->VidPnTargetId, &hVidPnTargetModeSet,
                                                                       &pDxgVidPnTargetModeSetInterface);
    if(!NT_SUCCESS(ntStatus))
    {
        return ntStatus;
    }
    ntStatus = pDxgVidPnTargetModeSetInterface->pfnAcquirePinnedModeInfo(hVidPnTargetModeSet,&pPinnedVidPnTargetModeInfo);
    if ( ntStatus == STATUS_GRAPHICS_MODE_NOT_PINNED )
    {
        pPinnedVidPnTargetModeInfo = NULL;
    }
    else if ( !NT_SUCCESS(ntStatus) )
    {
        return ntStatus;
    }
}

/**
 *******************************************************************************
 @brief	    DetermineTargetModeSet
 @remark     Determine Target Mode Set Test
 @param[in]  D3DKMDT_HVIDPN
 @return     STATUS_SUCCESS or STATUS_UNSUCCESSFUL. For more detailed failure info
 *******************************************************************************
 */
NTSTATUS DetermineTargetModeSet(CONST D3DKMDT_HVIDPN	i_hVidPn,
                                CONST D3DKMDT_VIDPN_PRESENT_PATH* CONST	i_pVidPnPresentPath,
                                CONST DXGK_VIDPN_INTERFACE* CONST	i_pDxgkVidPnInterface,
                                __out_bcount(MAX_TARGET_MODES) BOOLEAN* CONST   o_arr_bTargetModeInclusion,
                                CONST SIZE_T	i_sztNumTargetModes)
{
    D3DKMDT_HVIDPNSOURCEMODESET	hVidPnSourceModeSet								= NULL;
    const DXGK_VIDPNSOURCEMODESET_INTERFACE*	pDxgVidPnSourceModeSetInterface = NULL;
    CONST D3DKMDT_VIDPN_SOURCE_MODE*	pPinnedVidPnSourceModeInfo				= NULL;
    BOOLEAN* pbTargetModeInclusion = o_arr_bTargetModeInclusion;
    D3DKMDT_VIDEO_PRESENT_TARGET_MODE_ID	TargetModeId						= 0;

    NTSTATUS ntStatus = i_pDxgkVidPnInterface->pfnAcquireSourceModeSet(i_hVidPn, i_pVidPnPresentPath->VidPnSourceId,
                                                                       &hVidPnSourceModeSet, &pDxgVidPnSourceModeSetInterface);
    if(!NT_SUCCESS(ntStatus))
    {
        return ntStatus;
    }
    ntStatus = pDxgVidPnSourceModeSetInterface->pfnAcquirePinnedModeInfo(hVidPnSourceModeSet, &pPinnedVidPnSourceModeInfo);
    if ( ntStatus == STATUS_GRAPHICS_MODE_NOT_PINNED )
    {
        pPinnedVidPnSourceModeInfo = NULL;
    }
    else if ( !NT_SUCCESS(ntStatus) )
    {
        return ntStatus;
    }
}

/**
 *******************************************************************************
 @brief	    AddSourceMode
 @remark     Add Source Mode
 @param[in]  D3DKMDT_HVIDPNSOURCEMODESET
 @return     STATUS_SUCCESS or STATUS_UNSUCCESSFUL. For more detailed failure info
 *******************************************************************************
 */
NTSTATUS AddSourceMode(CONST D3DKMDT_HVIDPNSOURCEMODESET i_hSourceModeSet,
                       CONST DXGK_VIDPNSOURCEMODESET_INTERFACE* CONST i_pDxgkVidPnSourceModeSetInterface,
                       CONST D3DKMDT_GRAPHICS_RENDERING_FORMAT* CONST i_pVidPnSourceModeInfo,
                       __out_opt D3DKMDT_VIDEO_PRESENT_SOURCE_MODE_ID* CONST  o_pVidPnSourceModeId,
                       CONST D3DKMDT_VIDEO_PRESENT_SOURCE_MODE_ID i_VidPnSourceModeId)

{
    D3DKMDT_VIDPN_SOURCE_MODE*  pVidPnSourceMode = NULL;

    /* Acquire mode info placeholder */
    NTSTATUS ntStatus = i_pDxgkVidPnSourceModeSetInterface->pfnCreateNewModeInfo(i_hSourceModeSet, &pVidPnSourceMode);
    if ( !NT_SUCCESS(ntStatus) )
    {
        return ntStatus;
    }

    ASSERT( ARGUMENT_PRESENT(pVidPnSourceMode) );

    /* Populate mode info.Set the source mode ID, if provided by the caller (otherwise OS will assign one) */
    if ( i_VidPnSourceModeId != D3DDDI_ID_UNINITIALIZED)
    {
        //       pVidPnSourceMode->Id = i_VidPnSourceModeId;
    }

    pVidPnSourceMode->Type            = D3DKMDT_RMT_GRAPHICS;
    pVidPnSourceMode->Format.Graphics = *i_pVidPnSourceModeInfo;

    /* Store the ID before adding the mode (and invalidating pVidPnSourceMode) */
    if ( o_pVidPnSourceModeId )
    {
        //  *o_pVidPnSourceModeId = pVidPnSourceMode->Id;
    }

    /* Add the mode */
    ntStatus = i_pDxgkVidPnSourceModeSetInterface->pfnAddMode(i_hSourceModeSet,pVidPnSourceMode);
    if (!NT_SUCCESS(ntStatus))
    {
        /* Release the source mode info placeholder, in case we failed to add it to the mode set */
        {
            NTSTATUS ntReleaseStatus = i_pDxgkVidPnSourceModeSetInterface->pfnReleaseModeInfo(i_hSourceModeSet, pVidPnSourceMode);
            ASSERT(NT_SUCCESS(ntReleaseStatus));
        }
        return ntStatus;
    }
    return STATUS_SUCCESS;
}

/**
 *******************************************************************************
 @brief	    PopulateSourceModeSet_Test
 @remark     Populate source mode set test
 @param[in]  D3DKMDT_HVIDPN
 @return     STATUS_SUCCESS or STATUS_UNSUCCESSFUL. For more detailed failure info
 *******************************************************************************
 */
NTSTATUS PopulateSourceModeSet_Test(CONST D3DKMDT_HVIDPN	i_hVidPn,
                                    CONST DXGK_VIDPN_INTERFACE* CONST	i_pDxgkVidPnInterface,
                                    CONST D3DDDI_VIDEO_PRESENT_SOURCE_ID   i_VidPnSourceId,
                                    CONST BOOLEAN* CONST	i_arr_bSourceModesToEnum,
                                    BOOLEAN* CONST	i_arr_bIsSourceModeSetPopulated)
{
    D3DKMDT_HVIDPNSOURCEMODESET               hSourceModeSet                   = NULL;
    CONST DXGK_VIDPNSOURCEMODESET_INTERFACE*  pDxgkVidPnSourceModeSetInterface = NULL;

    D3DKMDT_VIDEO_PRESENT_SOURCE_MODE_ID SourceModeIndex = 0;
    NTSTATUS ntStatus = i_pDxgkVidPnInterface->pfnCreateNewSourceModeSet(i_hVidPn,i_VidPnSourceId,&hSourceModeSet,&pDxgkVidPnSourceModeSetInterface);
    ASSERT( NT_SUCCESS(ntStatus) );
    ASSERT( ARGUMENT_PRESENT(hSourceModeSet) );
    ASSERT( ARGUMENT_PRESENT(pDxgkVidPnSourceModeSetInterface) );

    /* For each mode to be added to the source mode set */
    for ( SourceModeIndex = 0;SourceModeIndex < g_NumAllVidPnSourceModes[0];++SourceModeIndex )			//HRD
    {
        NTSTATUS ntStatus = AddSourceMode(hSourceModeSet, pDxgkVidPnSourceModeSetInterface, g_AllVidPnSourceModes, NULL, NULL);
        ASSERT( NT_SUCCESS(ntStatus) );
    }
    ntStatus = i_pDxgkVidPnInterface->pfnAssignSourceModeSet(i_hVidPn,i_VidPnSourceId, hSourceModeSet);
    ASSERT( NT_SUCCESS(ntStatus) );
    return STATUS_SUCCESS;
}
/**
 *******************************************************************************
 @brief	    AddTargetMode
 @remark     Add Target Mode
 @param[in]  PVOID MiniportDeviceContext
 @return     STATUS_SUCCESS or STATUS_UNSUCCESSFUL. For more detailed failure info
 *******************************************************************************
 */
NTSTATUS AddTargetMode(CONST D3DKMDT_HVIDPNTARGETMODESET	i_hTargetModeSet,
                       CONST DXGK_VIDPNTARGETMODESET_INTERFACE* CONST	i_pDxgkVidPnTargetModeSetInterface,
                       CONST D3DKMDT_VIDEO_SIGNAL_INFO* CONST	i_pVidPnTargetModeInfo,
                       __out_opt D3DKMDT_VIDEO_PRESENT_TARGET_MODE_ID* CONST	o_pVidPnTargetModeId,
                       CONST D3DKMDT_VIDEO_PRESENT_TARGET_MODE_ID	i_VidPnTarge,
                       CONST D3DKMDT_MODE_PREFERENCE                          i_Preference
                       )
{
    D3DKMDT_VIDPN_TARGET_MODE*  pVidPnTargetMode = NULL;

    /* Acquire mode info placeholder */
    NTSTATUS ntStatus = i_pDxgkVidPnTargetModeSetInterface->pfnCreateNewModeInfo(i_hTargetModeSet,&pVidPnTargetMode);
    if ( !NT_SUCCESS(ntStatus) )
    {
        return ntStatus;
    }
    ASSERT( ARGUMENT_PRESENT(pVidPnTargetMode) );

    pVidPnTargetMode->VideoSignalInfo = *i_pVidPnTargetModeInfo;
    pVidPnTargetMode->Preference = i_Preference;

    /* Add the mode */
    ntStatus = i_pDxgkVidPnTargetModeSetInterface->pfnAddMode(i_hTargetModeSet,pVidPnTargetMode);
    if (!NT_SUCCESS(ntStatus))
    {
        /* Release the target mode info placeholder, in case we failed to add it to the mode set */
        {
            NTSTATUS ntReleaseStatus =  i_pDxgkVidPnTargetModeSetInterface->pfnReleaseModeInfo
                (i_hTargetModeSet,pVidPnTargetMode);
            ASSERT( NT_SUCCESS(ntReleaseStatus) );
        }
        return ntStatus;
    }
    return STATUS_SUCCESS;
}
/**
 *******************************************************************************
 @brief	    PopulateTargetModeSet_Test
 @remark     Populate Target Mode Set Test
 @param[in]  D3DKMDT_HVIDPN
 @return     STATUS_SUCCESS or STATUS_UNSUCCESSFUL. For more detailed failure info
 *******************************************************************************
 */
NTSTATUS PopulateTargetModeSet_Test(CONST D3DKMDT_HVIDPN	i_hVidPn,
                                    CONST DXGK_VIDPN_INTERFACE* CONST	i_pDxgkVidPnInterface,
                                    CONST D3DDDI_VIDEO_PRESENT_TARGET_ID	i_VidPnTargetId,
                                    CONST BOOLEAN* CONST	i_arr_bTargetModesToEnum)
{
    D3DKMDT_HVIDPNTARGETMODESET               hTargetModeSet                   = NULL;
    CONST DXGK_VIDPNTARGETMODESET_INTERFACE*  pDxgkVidPnTargetModeSetInterface = NULL;

    D3DKMDT_VIDEO_PRESENT_TARGET_MODE_ID TargetModeIndex = 0;
    NTSTATUS ntStatus = i_pDxgkVidPnInterface->pfnCreateNewTargetModeSet(i_hVidPn, i_VidPnTargetId, &hTargetModeSet, &pDxgkVidPnTargetModeSetInterface);
    ASSERT(NT_SUCCESS(ntStatus));
    ASSERT(ARGUMENT_PRESENT(hTargetModeSet));
    ASSERT(ARGUMENT_PRESENT(pDxgkVidPnTargetModeSetInterface));

    ntStatus = AddTargetMode(hTargetModeSet, pDxgkVidPnTargetModeSetInterface, g_AllVidPnTargetModes, NULL,
                             D3DDDI_ID_UNINITIALIZED, D3DKMDT_MP_PREFERRED);
    ASSERT( NT_SUCCESS(ntStatus) );

    ntStatus = i_pDxgkVidPnInterface->pfnAssignTargetModeSet(i_hVidPn, i_VidPnTargetId, hTargetModeSet);
    ASSERT( NT_SUCCESS(ntStatus) );
    return STATUS_SUCCESS;
}

/**
 *******************************************************************************
 @brief	    PopulateVidPnPresentPathCofuncModality
 @remark     Populates cofunctional video present source and target modes for the
 specified VidPN present path in the specified constraining VidPN.
 @param[in]  D3DKMDT_HVIDPN
 @return     STATUS_SUCCESS or STATUS_UNSUCCESSFUL. For more detailed failure info
 *******************************************************************************
 */
NTSTATUS PopulateVidPnPresentPathCofuncModality(CONST D3DKMDT_HVIDPN i_hConstrainingVidPn,
                                                CONST D3DKMDT_VIDPN_PRESENT_PATH* CONST i_pVidPnPresentPathToEnumModesOn,
                                                CONST DXGK_VIDPN_INTERFACE* CONST i_pDxgkVidPnInterface,
                                                CONST D3DKMDT_ENUMCOFUNCMODALITY_PIVOT_TYPE  i_pEnumPivotType,
                                                BOOLEAN* CONST i_arr_bIsSourceModeSetPopulated
                                                )
{
    BOOLEAN  arr_bSourceModesToEnum[1]    = {FALSE};   // assumes max of 1 mode on a source.
    BOOLEAN  arr_bTargetModesToEnum[1]    = {FALSE};   // assumes max of 1 mode on a target.

    /*Parameter validation (checked-only)*/
    {
        ASSERT( ARGUMENT_PRESENT(i_hConstrainingVidPn) );
        ASSERT( ARGUMENT_PRESENT(i_pVidPnPresentPathToEnumModesOn) );
    }

    NTSTATUS ntStatus = DetermineSourceModeSet(i_hConstrainingVidPn, i_pVidPnPresentPathToEnumModesOn, i_pDxgkVidPnInterface, arr_bSourceModesToEnum,1);			//HRD
    if( !NT_SUCCESS(ntStatus) )
    {
        return ntStatus;
    }

    ntStatus = DetermineTargetModeSet(i_hConstrainingVidPn, i_pVidPnPresentPathToEnumModesOn, i_pDxgkVidPnInterface, arr_bTargetModesToEnum,1);			//HRD
    if( !NT_SUCCESS(ntStatus) )
    {
        return ntStatus;
    }

    if ( i_pEnumPivotType != D3DKMDT_EPT_VIDPNSOURCE )
    {
        NTSTATUS ntStatus = PopulateSourceModeSet_Test(i_hConstrainingVidPn, i_pDxgkVidPnInterface, i_pVidPnPresentPathToEnumModesOn->VidPnSourceId,
                                                       arr_bSourceModesToEnum, i_arr_bIsSourceModeSetPopulated);
        if ( !NT_SUCCESS(ntStatus) )
        {
            return ntStatus;
        }
    }
    if ( i_pEnumPivotType != D3DKMDT_EPT_VIDPNTARGET )
    {
        NTSTATUS ntStatus = PopulateTargetModeSet_Test(i_hConstrainingVidPn, i_pDxgkVidPnInterface, i_pVidPnPresentPathToEnumModesOn->VidPnTargetId,
                                                       arr_bTargetModesToEnum);
        if ( !NT_SUCCESS(ntStatus) )
        {
            return ntStatus;
        }
    }
}

/**
 *******************************************************************************
 @brief	    EnumVidPnCofuncModality_Inspect
 @remark     EnumVidPn CofuncModality Inspect
 @param[in]  HANDLE  hAdapter
 @return     STATUS_SUCCESS or STATUS_UNSUCCESSFUL. For more detailed failure info
 *******************************************************************************
 */
NTSTATUS EnumVidPnCofuncModality_Inspect(CONST HANDLE  hAdapter,CONST DXGKARG_ENUMVIDPNCOFUNCMODALITY*
                                         CONST pEnumCofuncModalityArg,CONST DXGKRNL_INTERFACE*  pDxgKrnlCallback)
{
    D3DKMDT_HVIDPN hConstrainingVidPn											= NULL;
    CONST DXGK_VIDPN_INTERFACE* pDxgkVidPnInterface								= NULL;
    D3DKMDT_HVIDPNTOPOLOGY hConstrainingVidPnTopology							= NULL;
    CONST D3DKMDT_VIDPN_PRESENT_PATH*  pCurrentVidPnPresentPath					= NULL;
    CONST D3DKMDT_VIDPN_PRESENT_PATH*	pNextVidPnPresentPath					= NULL;
    CONST DXGK_VIDPNTOPOLOGY_INTERFACE*		pDxgkVidPnTopologyInterface			= NULL;
    CONST DXGKARG_ENUMVIDPNCOFUNCMODALITY* i_pEnumVidPnCofuncModalityArg		= NULL;

    i_pEnumVidPnCofuncModalityArg = pEnumCofuncModalityArg;
    hConstrainingVidPn = pEnumCofuncModalityArg->hConstrainingVidPn;

    NTSTATUS ntStatus = pDxgKrnlCallback->DxgkCbQueryVidPnInterface(hConstrainingVidPn, DXGK_VIDPN_INTERFACE_VERSION_V1,&pDxgkVidPnInterface);

    /*Acquire topology*/
    ntStatus = pDxgkVidPnInterface->pfnGetTopology(hConstrainingVidPn, &hConstrainingVidPnTopology, &pDxgkVidPnTopologyInterface);
    if (ntStatus!= 0)
    {
        return ntStatus;
    }

    ntStatus = pDxgkVidPnTopologyInterface->pfnAcquireFirstPathInfo(hConstrainingVidPnTopology, &pCurrentVidPnPresentPath);
    if (ntStatus!= 0)
    {
        return ntStatus;
    }

    do
    {
        pNextVidPnPresentPath=NULL;
        ntStatus = pDxgkVidPnTopologyInterface->pfnAcquireNextPathInfo(hConstrainingVidPnTopology, pCurrentVidPnPresentPath, &pNextVidPnPresentPath);
        if (ntStatus!= 0)
        {
            return ntStatus;
        }
        switch (pNextVidPnPresentPath->VidPnTargetId)
        {
            case CHILD_UDID :
            {
                Stat=TRUE;
                if(pEnumCofuncModalityArg->EnumPivot.VidPnTargetId == CHILD_UDID)
                {
                    break;
                }
                BOOLEAN arr_bIsSourceModeSetPopulated[1] = { FALSE };										//HRD
                BOOLEAN bUpdateTransformationSupport = FALSE;
                // Setup the supported rotation / scaling modes.
                D3DKMDT_VIDPN_PRESENT_PATH PresentPathInfo;
                RtlZeroMemory(&PresentPathInfo, sizeof(PresentPathInfo));
                // Copy the current transformation settings.
                PresentPathInfo.VidPnSourceId = pCurrentVidPnPresentPath->VidPnSourceId;
                PresentPathInfo.VidPnTargetId = pCurrentVidPnPresentPath->VidPnTargetId;
                PresentPathInfo.ContentTransformation = pCurrentVidPnPresentPath->ContentTransformation;
                PresentPathInfo.CopyProtection = pCurrentVidPnPresentPath->CopyProtection;
                D3DKMDT_ENUMCOFUNCMODALITY_PIVOT_TYPE EnumPivotType = D3DKMDT_EPT_NOPIVOT;
                switch ( i_pEnumVidPnCofuncModalityArg->EnumPivotType )
                {
                    case D3DKMDT_EPT_VIDPNSOURCE:
                    {
                        if (( i_pEnumVidPnCofuncModalityArg->EnumPivot.VidPnSourceId == pCurrentVidPnPresentPath->VidPnSourceId )||(i_pEnumVidPnCofuncModalityArg->EnumPivot.VidPnSourceId == pNextVidPnPresentPath->VidPnSourceId))	//check provided for ours childuid..
                        {
                            // INVARIANT: Current present path's source is the pivot of this enumeration.
                            EnumPivotType = D3DKMDT_EPT_VIDPNSOURCE;
                        }
                        break;
                    }
                    case D3DKMDT_EPT_VIDPNTARGET:
                    {
                        if (( i_pEnumVidPnCofuncModalityArg->EnumPivot.VidPnTargetId == pCurrentVidPnPresentPath->VidPnTargetId )||( i_pEnumVidPnCofuncModalityArg->EnumPivot.VidPnTargetId == pNextVidPnPresentPath->VidPnTargetId ))	//check provided for ours childuid..
                        {
                            // INVARIANT: Current present path's target is the pivot of this enumeration.
                            EnumPivotType = D3DKMDT_EPT_VIDPNTARGET;
                        }
                        break;
                    }
                    case D3DKMDT_EPT_NOPIVOT:
                        break;
                    case D3DKMDT_EPT_SCALING:
                    case D3DKMDT_EPT_ROTATION:
                    {
                        break;
                    }
                    default:
                    {
                        ASSERT( (i_pEnumVidPnCofuncModalityArg->EnumPivotType == D3DKMDT_EPT_VIDPNSOURCE)
                                ||
                                (i_pEnumVidPnCofuncModalityArg->EnumPivotType == D3DKMDT_EPT_VIDPNTARGET)
                                ||
                                (i_pEnumVidPnCofuncModalityArg->EnumPivotType == D3DKMDT_EPT_NOPIVOT)
                                ||
                                (i_pEnumVidPnCofuncModalityArg->EnumPivotType == D3DKMDT_EPT_SCALING)
                                ||
                                (i_pEnumVidPnCofuncModalityArg->EnumPivotType == D3DKMDT_EPT_ROTATION) );
                        return STATUS_INVALID_PARAMETER;
                    }
                }//end switch statement

                // Populate current present path's source and target mode sets using the function provided by the caller.
                NTSTATUS ntStatus = PopulateVidPnPresentPathCofuncModality(hConstrainingVidPn, pNextVidPnPresentPath, pDxgkVidPnInterface,
                                                                           EnumPivotType, arr_bIsSourceModeSetPopulated);
                if (!NT_SUCCESS(ntStatus))
                {
                    // Return the error to the caller
                    return ntStatus;
                }
                break;					//enum vid pn our function functionality end
            }

            default:
                break;
        }//end switch statement
        ntStatus = pDxgkVidPnTopologyInterface->pfnReleasePathInfo(hConstrainingVidPnTopology,pCurrentVidPnPresentPath);
        pCurrentVidPnPresentPath = pNextVidPnPresentPath;
    }while(pNextVidPnPresentPath);
    return ntStatus;
}

//***********************************************************************************
NTSTATUS APIENTRY justkernD3DDDIEnumVidPnCofuncModality(CONST HANDLE  hAdapter,CONST DXGKARG_ENUMVIDPNCOFUNCMODALITY* CONST  pEnumCofuncModalityArg )
{
    DXGKRNL_INTERFACE*                 pDxgKrnlCallback            = NULL;
    //DbgPrint("LOFILTER:justkernD3DDDIEnumVidPnCofuncModality called\n");

    pDxgKrnlCallback = &(pHwDeviceExtension->ddiCallback);
    EnumVidPnCofuncModality_Inspect(hAdapter,pEnumCofuncModalityArg,pDxgKrnlCallback);

    NTSTATUS (*justkernD3DDDIEnumVidPnCofuncModality_ptr)(CONST HANDLE  hAdapter,CONST DXGKARG_ENUMVIDPNCOFUNCMODALITY* CONST  pEnumCofuncModalityArg );
    memcpy(&justkernD3DDDIEnumVidPnCofuncModality_ptr,&Primary_Structure_Address.DxgkDdiEnumVidPnCofuncModality ,4);
    NTSTATUS Status =  (*justkernD3DDDIEnumVidPnCofuncModality_ptr)(hAdapter,pEnumCofuncModalityArg );
    return Status;
}

NTSTATUS justkernD3DDIStopCapture(CONST HANDLE  hAdapter, CONST DXGKARG_STOPCAPTURE*  pStopCapture)
{
    DbgPrint("LOFILTER:justkernD3DDIStopCapture called\n");
    NTSTATUS (*justkernD3DDIStopCapture_ptr)(CONST HANDLE  hAdapter, CONST DXGKARG_STOPCAPTURE*  pStopCapture);
    memcpy(&justkernD3DDIStopCapture_ptr,&Primary_Structure_Address.DxgkDdiStopCapture,4);
    NTSTATUS status = (*justkernD3DDIStopCapture_ptr)(hAdapter,pStopCapture);
    return status;
}

NTSTATUS justkernD3DDICreateOverlay(CONST HANDLE  hAdapter, DXGKARG_CREATEOVERLAY  *pCreateOverlay)
{
    DbgPrint("LOFILTER:justkernD3DDICreateOverlay called\n");
    NTSTATUS (*justkernD3DDICreateOverlay_ptr)(CONST HANDLE  hAdapter, DXGKARG_CREATEOVERLAY  *pCreateOverlay);
    memcpy(&justkernD3DDICreateOverlay_ptr,&Primary_Structure_Address.DxgkDdiCreateOverlay,4);
    NTSTATUS status = (*justkernD3DDICreateOverlay_ptr)(hAdapter,pCreateOverlay);
    return status;
}


NTSTATUS justkernD3DDDIDestroyDevice(HANDLE  hDevice)
{
    DbgPrint("LOFILTER:justkernD3DDDIDestroyDevice called\n");
    NTSTATUS (*justkernD3DDDIDestroyDevice_ptr)( HANDLE);
    memcpy(&justkernD3DDDIDestroyDevice_ptr,&Primary_Structure_Address.DxgkDdiDestroyDevice,4);
    NTSTATUS status = (*justkernD3DDDIDestroyDevice_ptr)(hDevice);
    return (STATUS_SUCCESS);
}

NTSTATUS justkernD3DDIUpdateOverlay(CONST HANDLE  hOverlay,CONST DXGKARG_UPDATEOVERLAY  *pUpdateOverlay)
{
    DbgPrint("LOFILTER:justkernD3DDIUpdateOverlay called\n");
    NTSTATUS (*justkernD3DDIUpdateOverlay_ptr)(CONST HANDLE  hOverlay,CONST DXGKARG_UPDATEOVERLAY  *pUpdateOverlay);
    memcpy(&justkernD3DDIUpdateOverlay_ptr,&Primary_Structure_Address.DxgkDdiUpdateOverlay,4);
    NTSTATUS status = (*justkernD3DDIUpdateOverlay_ptr)(hOverlay,pUpdateOverlay);
    return status;
}

NTSTATUS justkernD3DDIFlipOverlay( CONST HANDLE  hOverlay,CONST DXGKARG_FLIPOVERLAY  *pFlipOverlay)
{
    DbgPrint("LOFILTER:justkernD3DDIFlipOverlay called\n");
    NTSTATUS (*justkernD3DDIFlipOverlay_ptr)(CONST HANDLE  hOverlay,CONST DXGKARG_FLIPOVERLAY  *pFlipOverlay);
    memcpy(&justkernD3DDIFlipOverlay_ptr,&Primary_Structure_Address.DxgkDdiFlipOverlay,4);
    NTSTATUS status = (*justkernD3DDIFlipOverlay_ptr)(hOverlay,pFlipOverlay);
    return status;
}

NTSTATUS  justkernD3DDIDestroyOverlay(CONST HANDLE  hOverlay)
{
    DbgPrint("LOFILTER:justkernD3DDIDestroyOverlay called\n");
    NTSTATUS (*justkernD3DDIDestroyOverlay_ptr)(CONST HANDLE  hOverlay);
    memcpy(&justkernD3DDIDestroyOverlay_ptr,&Primary_Structure_Address.DxgkDdiDestroyOverlay,4);
    NTSTATUS status = (*justkernD3DDIDestroyOverlay_ptr)(hOverlay);
    return status;
}

NTSTATUS  justkernD3DDDIQueryVidPnHWCapability( __in CONST HANDLE i_hAdapter,
                                                DXGKARG_QUERYVIDPNHWCAPABILITY* io_pVidPnHWCaps)
{
    DbgPrint("LOFILTER:justkernD3DDDIQueryVidPnHWCapability called\n");
    NTSTATUS (*justkernD3DDDIQueryVidPnHWCapability)(__in CONST HANDLE i_hAdapter, DXGKARG_QUERYVIDPNHWCAPABILITY* io_pVidPnHWCaps);
    memcpy(&justkernD3DDDIQueryVidPnHWCapability,&Primary_Structure_Address.DxgkDdiQueryVidPnHWCapability,4);
    NTSTATUS status = (*justkernD3DDDIQueryVidPnHWCapability)(i_hAdapter,io_pVidPnHWCaps);
    return status;
}

NTSTATUS justkernD3DDILinkDevice(__in CONST PDEVICE_OBJECT  PhysicalDeviceObject,__in CONST PVOID  MiniportDeviceContext,__inout PLINKED_DEVICE  LinkedDevice)
{
    DbgPrint("LOFILTER:justkernD3DDILinkDevice called\n");
    NTSTATUS (*justkernD3DDILinkDevice_ptr)(__in CONST PDEVICE_OBJECT  PhysicalDeviceObject,__in CONST PVOID  MiniportDeviceContext,__inout PLINKED_DEVICE  LinkedDevice);
    memcpy(&justkernD3DDILinkDevice_ptr,&Primary_Structure_Address.DxgkDdiLinkDevice,4);
    NTSTATUS status = (*justkernD3DDILinkDevice_ptr)(PhysicalDeviceObject,MiniportDeviceContext,LinkedDevice);
    return status;
}

NTSTATUS justkernD3DDDIOpenAllocation(VOID *InterfaceContext,CONST DXGKARG_OPENALLOCATION    *pDDIDAData)
{
    DbgPrint("LOFILTER:justkernD3DDDIOpenAllocation called\n");
    NTSTATUS (*justkernD3DDDIOpenAllocation_ptr)(VOID *InterfaceContext,CONST DXGKARG_OPENALLOCATION    *pDDIDAData);
    memcpy(&justkernD3DDDIOpenAllocation_ptr,&Primary_Structure_Address.DxgkDdiOpenAllocation,4);
    NTSTATUS status = (*justkernD3DDDIOpenAllocation_ptr)(InterfaceContext,pDDIDAData);
    return status;
}

NTSTATUS justkernD3DDDICloseAllocation(HANDLE  hDevice,CONST DXGKARG_CLOSEALLOCATION  *pDDICAData)
{
    DbgPrint("LOFILTER:justkernD3DDDICloseAllocation called\n");
    NTSTATUS (*justkernD3DDDICloseAllocation_ptr)(HANDLE  hDevice,CONST DXGKARG_CLOSEALLOCATION  *pDDICAData);
    memcpy(&justkernD3DDDICloseAllocation_ptr,&Primary_Structure_Address.DxgkDdiCloseAllocation,4);
    NTSTATUS status = (*justkernD3DDDICloseAllocation_ptr)(hDevice,pDDICAData);
    return status;
}

NTSTATUS justkernD3DDDIRender(HANDLE hContext,DXGKARG_RENDER *pDDIRenderData)
{
    //DbgPrint("LOFILTER:justkernD3DDDIRender called\n");
    NTSTATUS (*justkernD3DDDIRender_ptr)(HANDLE,DXGKARG_RENDER*);
    memcpy(&justkernD3DDDIRender_ptr,&Primary_Structure_Address.DxgkDdiRender,4);
    NTSTATUS status = (*justkernD3DDDIRender_ptr)(hContext,pDDIRenderData);
    return status;
}

NTSTATUS justkernD3DDDIPresent(HANDLE hContext,DXGKARG_PRESENT  *pDDIPresentData)
{
    //DbgPrint("LOFILTER:justkernD3DDDIPresent called\n");
    NTSTATUS (*justkernD3DDDIPresent_ptr)(HANDLE hContext,DXGKARG_PRESENT* pDDIPresentData);
    memcpy(&justkernD3DDDIPresent_ptr,&Primary_Structure_Address.DxgkDdiPresent,4);
    NTSTATUS status = (*justkernD3DDDIPresent_ptr)(hContext,pDDIPresentData);
    return status;
}

NTSTATUS  justkernD3DDDIResetFromTimeout(HANDLE hAdapter)
{
    DbgPrint("LOFILTER:justkernD3DDDIResetFromTimeout called\n");
    NTSTATUS (*justkernD3DDDIResetFromTimeout_ptr)(HANDLE hAdapter);
    memcpy(&justkernD3DDDIResetFromTimeout_ptr,&Primary_Structure_Address.DxgkDdiResetFromTimeout,4);
    NTSTATUS status = (*justkernD3DDDIResetFromTimeout_ptr)(hAdapter);
    return status;

}

NTSTATUS justkernD3DDDIRestartFromTimeout(HANDLE hAdapter)
{
    DbgPrint("LOFILTER:justkernD3DDDIRestartFromTimeout called\n");
    NTSTATUS (*justkernD3DDDIRestartFromTimeout_ptr)(HANDLE hAdapter);
    memcpy(&justkernD3DDDIRestartFromTimeout_ptr,&Primary_Structure_Address.DxgkDdiRestartFromTimeout,4);
    NTSTATUS status = (*justkernD3DDDIRestartFromTimeout_ptr)(hAdapter);
    return status;

}

NTSTATUS  justkernD3DDDIEscape(VOID *InterfaceContext,CONST DXGKARG_ESCAPE   *pDDIEscape)
{
    //DbgPrint("LOFILTER:justkernD3DDDIEscape called\n");
    NTSTATUS (*justkernD3DDDIEscape_ptr)(VOID* InterfaceContext,CONST DXGKARG_ESCAPE   * pDDIEscape);
    memcpy(&justkernD3DDDIEscape_ptr,&Primary_Structure_Address.DxgkDdiEscape,4);
    NTSTATUS status = (*justkernD3DDDIEscape_ptr)(InterfaceContext, pDDIEscape );
    return status;

}

NTSTATUS justkernD3DDDICollectDbgInfo(HANDLE hAdapter,CONST DXGKARG_COLLECTDBGINFO *pDDICollectDbgInfo)
{
    DbgPrint("LOFILTER:justkernD3DDDICollectDbgInfo called\n");
    NTSTATUS (*justkernD3DDDICollectDbgInfo_ptr)(HANDLE hAdapter,CONST DXGKARG_COLLECTDBGINFO * pDDICollectDbgInfo);
    memcpy(&justkernD3DDDICollectDbgInfo_ptr,&Primary_Structure_Address.DxgkDdiCollectDbgInfo,4);
    NTSTATUS status = (*justkernD3DDDICollectDbgInfo_ptr)(hAdapter, pDDICollectDbgInfo );
    return status;
}

NTSTATUS justkernD3DDDIControlInterrupt(VOID *pKmdContext,CONST DXGK_INTERRUPT_TYPE  InterruptType,BOOLEAN EnableInterrupt)
{
    DbgPrint("LOFILTER:justkernD3DDDIControlInterrupt called\n");
    NTSTATUS (*justkernD3DDDIControlInterrupt_ptr)(VOID* pKmdContext,CONST DXGK_INTERRUPT_TYPE InterruptType  , BOOLEAN EnableInterrupt );
    memcpy(&justkernD3DDDIControlInterrupt_ptr,&Primary_Structure_Address.DxgkDdiControlInterrupt,4);
    NTSTATUS status = (*justkernD3DDDIControlInterrupt_ptr)(pKmdContext, InterruptType,EnableInterrupt );
    return status;
}

NTSTATUS justkernD3DDDIGetScanLine(VOID* InterfaceContext,DXGKARG_GETSCANLINE*    pGetScanLine)
{
    DbgPrint("LOFILTER:justkernD3DDDIGetScanLine called\n");
    NTSTATUS (*justkernD3DDDIGetScanLine_ptr)(VOID* InterfaceContext ,DXGKARG_GETSCANLINE* pGetScanLine);
    memcpy(&justkernD3DDDIGetScanLine_ptr,&Primary_Structure_Address.DxgkDdiGetScanLine,4);
    NTSTATUS status = (*justkernD3DDDIGetScanLine_ptr)(InterfaceContext, pGetScanLine );
    return status;
}

NTSTATUS justkernD3DDDISetVidPnSourceAddress(HANDLE hAdapter,CONST DXGKARG_SETVIDPNSOURCEADDRESS*    pDDISetVidPnSourceAddress)
{
    DbgPrint("LOFILTER:justkernD3DDDISetVidPnSourceAddress called\n");
    NTSTATUS (*justkernD3DDDISetVidPnSourceAddress_ptr)(HANDLE hAdapter,CONST DXGKARG_SETVIDPNSOURCEADDRESS* pDDISetVidPnSourceAddress);
    memcpy(&justkernD3DDDISetVidPnSourceAddress_ptr,&Primary_Structure_Address.DxgkDdiSetVidPnSourceAddress,4);
    NTSTATUS status = (*justkernD3DDDISetVidPnSourceAddress_ptr)(hAdapter, pDDISetVidPnSourceAddress );
    return status;
}

NTSTATUS justkernD3DDDISetVidPnSourceVisibility(HANDLE hAdapter,CONST DXGKARG_SETVIDPNSOURCEVISIBILITY*  pDDISetVidPnSourceVisibility)
{
    DbgPrint("LOFILTER:justkernD3DDDISetVidPnSourceVisibility called\n");
    NTSTATUS (*justkernD3DDDISetVidPnSourceVisibility_ptr)( HANDLE hAdapter,CONST DXGKARG_SETVIDPNSOURCEVISIBILITY* pDDISetVidPnSourceVisibility);
    memcpy(&justkernD3DDDISetVidPnSourceVisibility_ptr,&Primary_Structure_Address.DxgkDdiSetVidPnSourceVisibility,4);
    NTSTATUS status = (*justkernD3DDDISetVidPnSourceVisibility_ptr)(hAdapter, pDDISetVidPnSourceVisibility );
    return status;
}

NTSTATUS justkernD3DDDIUpdateActiveVidPnPresentPath(HANDLE hAdapter,CONST DXGKARG_UPDATEACTIVEVIDPNPRESENTPATH*   pUpdateActiveVidPnPresentPathArg)
{
    DbgPrint("LOFILTER:justkernD3DDDIUpdateActiveVidPnPresentPath called\n");
    NTSTATUS (*justkernD3DDDIUpdateActiveVidPnPresentPath_ptr)(HANDLE hAdapter,CONST DXGKARG_UPDATEACTIVEVIDPNPRESENTPATH* pUpdateActiveVidPnPresentPathArg);
    memcpy(&justkernD3DDDIUpdateActiveVidPnPresentPath_ptr,&Primary_Structure_Address.DxgkDdiUpdateActiveVidPnPresentPath,4);
    NTSTATUS status = (*justkernD3DDDIUpdateActiveVidPnPresentPath_ptr)(hAdapter, pUpdateActiveVidPnPresentPathArg );
    return status;
}

NTSTATUS justkernD3DDDICommitVidPn(HANDLE i_hAdapter,CONST DXGKARG_COMMITVIDPN*  i_pDDICommitVidPN)
{
    DbgPrint("LOFILTER:justkernD3DDDICommitVidPn called\n");
    NTSTATUS (*justkernD3DDDICommitVidPn_ptr)(HANDLE   i_hAdapter,CONST DXGKARG_COMMITVIDPN*  i_pDDICommitVidPN);
    memcpy(&justkernD3DDDICommitVidPn_ptr,&Primary_Structure_Address.DxgkDdiCommitVidPn,4);
    NTSTATUS status = (*justkernD3DDDICommitVidPn_ptr)(i_hAdapter, i_pDDICommitVidPN );
    return status;
}

NTSTATUS justkernD3DDDIRecommendMonitorModes(HANDLE hAdapter,CONST DXGKARG_RECOMMENDMONITORMODES*  pRecommendMonitorModesArg)
{


    DbgPrint("LOFILTER:justkernD3DDDIRecommendMonitorModes called\n");
    NTSTATUS (*justkernD3DDDIRecommendMonitorModes_ptr)(HANDLE  hAdapter,CONST DXGKARG_RECOMMENDMONITORMODES*  pRecommendMonitorModesArg);
    memcpy(&justkernD3DDDIRecommendMonitorModes_ptr,&Primary_Structure_Address.DxgkDdiRecommendMonitorModes,4);
    NTSTATUS status = (*justkernD3DDDIRecommendMonitorModes_ptr)(hAdapter, pRecommendMonitorModesArg );
    return status;
}

NTSTATUS justkernD3DDDIRecommendVidPnTopology(HANDLE hAdapter,CONST DXGKARG_RECOMMENDVIDPNTOPOLOGY*  pRecommendVidPnTopologyArg)
{
    DbgPrint("LOFILTER:justkernD3DDDIRecommendVidPnTopology called\n");
    NTSTATUS (*justkernD3DDDIRecommendVidPnTopology_ptr)(HANDLE hAdapter,CONST DXGKARG_RECOMMENDVIDPNTOPOLOGY*  pRecommendVidPnTopologyArg);
    memcpy(&justkernD3DDDIRecommendVidPnTopology_ptr,&Primary_Structure_Address.DxgkDdiRecommendVidPnTopology,4);
    NTSTATUS status = (*justkernD3DDDIRecommendVidPnTopology_ptr)(hAdapter, pRecommendVidPnTopologyArg );
    return status;
}

NTSTATUS justkernD3DDDICreateContext( IN_CONST_HANDLE hDevice,INOUT_PDXGKARG_CREATECONTEXT    pCreateContext)

{
    DbgPrint("LOFILTER:justkernD3DDDICreateContext called\n");
    NTSTATUS (*justkernD3DDDICreateContext_ptr)(IN_CONST_HANDLE  hDevice,INOUT_PDXGKARG_CREATECONTEXT    pCreateContext );
    memcpy(&justkernD3DDDICreateContext_ptr,&Primary_Structure_Address.DxgkDdiCreateContext,4);
    NTSTATUS status = (*justkernD3DDDICreateContext_ptr)(hDevice, pCreateContext );
    return status;
}

NTSTATUS justkernD3DDDIDestroyContext(IN_CONST_HANDLE    hContext )
{
    DbgPrint("LOFILTER:justkernD3DDDIDestroyContext called\n");
    NTSTATUS (*justkernD3DDDIDestroyContext_ptr)(IN_CONST_HANDLE hContext);
    memcpy(&justkernD3DDDIDestroyContext_ptr,&Primary_Structure_Address.DxgkDdiDestroyContext,4);
    NTSTATUS status = (*justkernD3DDDIDestroyContext_ptr)( hContext );
    return status;
}

NTSTATUS APIENTRY justkernDXGKDDI_QUERYCURRENTFENCE(CONST HANDLE  hAdapter, DXGKARG_QUERYCURRENTFENCE*  pCurrentFence)
{
    NTSTATUS (*justkernDXGKDDI_QUERYCURRENTFENCE_ptr)(CONST HANDLE  hAdapter, DXGKARG_QUERYCURRENTFENCE*  pCurrentFence);
    DbgPrint("LOFILTER:justkernD3DDDIDestroyContext called\n");
    memcpy(&justkernDXGKDDI_QUERYCURRENTFENCE_ptr,&Primary_Structure_Address.DxgkDdiQueryCurrentFence,4);
    NTSTATUS status = (*justkernDXGKDDI_QUERYCURRENTFENCE_ptr)( hAdapter,pCurrentFence );
    return status;
}
