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

// Maximum number of groups per UMP endpoint or virual cables for USB MIDI 1.0
#define MAX_NUM_GROUPS_CABLES 16

//
// Structures to aid in conversion between USB MIDI 1.0 and UMP
//
    typedef struct MIDI_STREAM_t
    {
        UINT8       buffer[4];
        UINT8       index;
        UINT8       total;
    } MIDI_STREAM_TYPE;

    typedef struct
    {
        UINT8       wordCount;
        union ump_device
        {
            UINT32  umpWords[4];
            UINT8   umpBytes[sizeof(UINT32) * 4];
        } umpData;
    } UMP_PACKET;

//
// Define device context.
//
typedef struct _DEVICE_CONTEXT {
    WDFUSBDEVICE    UsbDevice;
    ACXCIRCUIT      Midi;
    WDF_TRI_STATE   ExcludeD3Cold;

    WDFUSBINTERFACE             UsbControlInterface;
    WDFUSBINTERFACE             UsbMIDIStreamingInterface;
    UINT8                       UsbMIDIStreamingAlt;
    ULONG                       UsbDeviceTraits;

    // Buffers and information for USB MIDI 1.0 and UMP translations
    bool                        midi1IsInSysex[MAX_NUM_GROUPS_CABLES];
    MIDI_STREAM_TYPE            midi1OutSysex[MAX_NUM_GROUPS_CABLES];

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

//
// Function to complete read on IN Pipe
//
_Must_inspect_result_
__drv_maxIRQL(PASSIVE_LEVEL)
NONPAGED_CODE_SEG
VOID USBMIDI2DriverEvtReadComplete(
    _In_    WDFUSBPIPE Pipe,
    _In_    WDFMEMORY  Buffer,
    _In_    size_t     NumBytesTransferred,
    _In_    WDFCONTEXT Context
);

//
// Function to handle failed read on IN Pipe
//
_Must_inspect_result_
__drv_maxIRQL(PASSIVE_LEVEL)
NONPAGED_CODE_SEG
BOOLEAN USBMIDI2DriverEvtReadFailed(
    WDFUSBPIPE      Pipe,
    NTSTATUS        status,
    USBD_STATUS     UsbdStatus
);

#endif // _DEVICE_H_
