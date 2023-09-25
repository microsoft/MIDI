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
    WDFUSBDEVICE    UsbDevice;
    ACXCIRCUIT      Midi;
    WDF_TRI_STATE   ExcludeD3Cold;

    WDFUSBINTERFACE             UsbControlInterface;
    WDFUSBINTERFACE             UsbMIDIStreamingInterface;
    UCHAR                       UsbMIDIStreamingAlt;
    ULONG                       UsbDeviceTraits;

    // Pipes
    // Currently setup for single endpoint pair - need to consider for multiple endpoint pairs
    WDFUSBPIPE                  MidiInPipe;         // in from device
    WDF_USB_PIPE_TYPE           MidiInPipeType;     // Bulk or Interrupt
    ULONG                       MidiInMaxSize;      // maximum transfer size
    WDFUSBPIPE                  MidiOutPipe;        // out to device
    WDF_USB_PIPE_TYPE           MidiOutPipeType;    // Bulk or Interrupt
    ULONG                       MidiOutMaxSize;     // maximum transfer size

    //
    // The folloiwng fileds are used to store device configuration information
    // relevant to the connected device.
    // 
    WDFMEMORY                       DeviceConfigDescriptorMemory;
    WDFMEMORY                       DeviceManfMemory;
    WDFMEMORY                       DeviceNameMemory;
    WDFMEMORY                       DeviceSNMemory;
    WDFMEMORY                       DeviceGTBMemory;

    // Read Ring Buffer
    //READ_RING_TYPE              ReadRingBuf;

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

// Forward Declartion of helper functions
//
// Function to parse descriptors and select usb Interface
//
_Must_inspect_result_
__drv_maxIRQL(PASSIVE_LEVEL)
PAGED_CODE_SEG
NTSTATUS
USBMIDI2DriverSelectInterface(
    _In_ WDFDEVICE    Device
);

//
// Function to connect and prepare pipes for use
//
_Must_inspect_result_
__drv_maxIRQL(PASSIVE_LEVEL)
PAGED_CODE_SEG
NTSTATUS
USBMIDI2DriverEnumeratePipes(
    _In_ WDFDEVICE    Device
);

#endif // _DEVICE_H_
