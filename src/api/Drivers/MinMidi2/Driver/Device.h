/*++

Copyright (c) Microsoft Corporation.  All rights reserved.

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY
    KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR
    PURPOSE.

Module Name:

    Device.h

Abstract:

    Contains structure definitions and function prototypes private to
    the driver.

Environment:

    Kernel mode

--*/

#ifndef _DEVICE_H_
#define _DEVICE_H_

/* make prototypes usable from C++ */
#ifdef __cplusplus
extern "C" {
#endif

//
// Define device context.
//
typedef struct _DEVICE_CONTEXT {
    ACXCIRCUIT      Midi;
    WDF_TRI_STATE   ExcludeD3Cold;
} DEVICE_CONTEXT, *PDEVICE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DEVICE_CONTEXT, GetDeviceContext)

// Device callbacks.
EVT_WDF_DRIVER_DEVICE_ADD           EvtBusDeviceAdd;
EVT_WDF_DEVICE_PREPARE_HARDWARE     EvtDevicePrepareHardware;
EVT_WDF_DEVICE_RELEASE_HARDWARE     EvtDeviceReleaseHardware;
EVT_WDF_DEVICE_D0_ENTRY             EvtDeviceD0Entry;
EVT_WDF_DEVICE_D0_EXIT              EvtDeviceD0Exit;
EVT_WDF_DEVICE_CONTEXT_CLEANUP      EvtDeviceContextCleanup;

/* make internal prototypes usable from C++ */
#ifdef __cplusplus
}
#endif

//
// Used to store the registry settings path for the driver
//
extern UNICODE_STRING g_RegistryPath;

_Must_inspect_result_
__drv_maxIRQL(PASSIVE_LEVEL)
PAGED_CODE_SEG
NTSTATUS
CopyRegistrySettingsPath(
    _In_ PUNICODE_STRING RegistryPath
    );


_Must_inspect_result_
__drv_maxIRQL(PASSIVE_LEVEL)
PAGED_CODE_SEG
NTSTATUS
SetPowerPolicy(
    _In_ WDFDEVICE      Device
    );

#endif // _DEVICE_H_
