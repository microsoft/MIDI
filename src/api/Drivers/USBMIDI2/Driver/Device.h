/************************************************************************************
Copyright 2023 Association of Musical Electronics Industry
Copyright 2023 Microsoft
Driver source code developed by AmeNote. Some components Copyright 2023 AmeNote Inc.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
************************************************************************************/
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
#pragma once

#include "Public.h"
#include "Common.h"
#include "StreamEngine.h"

// Used to regulate the IO buffering of driver through to service
#define USB_WRITE_MAX_URBS      5

// Used for handling exchange of SYSEX between USB MIDI 1.0 and UMP
#define UMPTOBS_BUFFER 12
#define SYSEX_START 0xF0
#define SYSEX_STOP  0xF7

/* make prototypes usable from C++ */
#ifdef __cplusplus
extern "C" {
#endif

// Maximum number of groups per UMP endpoint or virual cables for USB MIDI 1.0
#define MAX_NUM_GROUPS_CABLES 16

// Size of ring buffers for sysex bytestream
#define SYSEX_BS_RB_SIZE    UMPTOBS_BUFFER + 4  // size coming in plus at least one USB MIDI1.0 SYSEX Packet

//
// Structure to aid in UMP SYSEX to USB MIDI 1.0
//
typedef struct UMP_TO_MIDI1_SYSEX_t
{
    bool                inSysex;
    UINT8               sysexBS[SYSEX_BS_RB_SIZE];
    UINT8               usbMIDI1Tail;
    UINT8               usbMIDI1Head;
} UMP_TO_MIDI1_SYSEX;

typedef struct
{
    UINT8       wordCount;
    union ump_device
    {
        UINT32  umpWords[4];
        UINT8   umpBytes[sizeof(UINT32) * 4];
    } umpData;
} UMP_PACKET, *PUMP_PACKET;

//
// States for Continuous Reader
// 
typedef enum
{
    USBMIDI2_CONT_RDR_IDLE = 0,
    USBMIDI2_CONT_RDR_ACTIVE,
    USBMIDI2_CONT_RDR_RUNNING,
    USBMIDI2_CONT_RDR_STOP
} USBMIDI2_CONT_RDR_STATE;

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
    UINT16                      UsbMIDIbcdMSC;
    UINT8                       UsbMIDIInterfaceNumber;
    ULONG                       UsbDeviceTraits;
    UINT16                      UsbOutMask;         // Primarily for legacy support, mask for CableIDs supported
    UINT16                      UsbInMask;          // Primarily for legacy support, mask for CableIDs supported
    HANDLE                      WriteEvent;
    HANDLE                      ReadEvent;
    KEVENT                      UsbWriteEvent;      // Used to indicate that a USB Out Write packet has been completed
    UINT                        UsbWriteIOUrbs;
    KSPIN_LOCK                  UsbWriteIOUrbsLock;
    KEVENT                      UsbReadBufferEvent; // Used to indicate that data has been pulled from Read Buffer

    // Buffers and information for USB MIDI 1.0 and UMP translations
    bool                        midi1IsInSysex[MAX_NUM_GROUPS_CABLES];
    UMP_TO_MIDI1_SYSEX          midi1OutSysex[MAX_NUM_GROUPS_CABLES];

    // Pipes
    // Currently setup for single endpoint pair - need to consider for multiple endpoint pairs
    WDFUSBPIPE                  MidiInPipe;         // in from device
    WDF_USB_PIPE_TYPE           MidiInPipeType;     // Bulk or Interrupt
    ULONG                       MidiInMaxSize;      // maximum transfer size
    WDFUSBPIPE                  MidiOutPipe;        // out to device
    WDF_USB_PIPE_TYPE           MidiOutPipeType;    // Bulk or Interrupt
    ULONG                       MidiOutMaxSize;     // maximum transfer size

    // Continuous Reader
    // Continous Reader state and management
    USBMIDI2_CONT_RDR_STATE     ContRdrState = USBMIDI2_CONT_RDR_IDLE;
    KSPIN_LOCK                  ContRdrLock;

    //
    // The folloiwng fileds are used to store device configuration information
    // relevant to the connected device.
    // 
    USHORT                      DeviceVID;
    USHORT                      DevicePID;
    WDFMEMORY                   DeviceConfigDescriptorMemory;
    WDFMEMORY                   DeviceManfMemory;
    WDFMEMORY                   DeviceProductNameMemory;
    WDFMEMORY                   DeviceNameMemory;
    WDFMEMORY                   DeviceSNMemory;
    WDFMEMORY                   DeviceGTBMemory;
    WDFMEMORY                   DeviceInGTBIDs;
    WDFMEMORY                   DeviceOutGTBIDs;
    WDFMEMORY                   DeviceWriteMemory;
    PUCHAR                      DeviceWriteBuffer;
    size_t                      DeviceWriteBufferIndex;
    WDFREQUEST                  DeviceUSBWriteRequest;

    //
    // Following fields to help with diagnostics on buffer management
    // 
    UINT                        NumberBuffersProcessed;

    //
    // Streaming Engine
    //
    //
    StreamEngine*               pStreamEngine;
} DEVICE_CONTEXT, *PDEVICE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DEVICE_CONTEXT, GetDeviceContext)

// Device callbacks.
EVT_WDF_DRIVER_DEVICE_ADD           EvtBusDeviceAdd;
EVT_WDF_DEVICE_PREPARE_HARDWARE     EvtDevicePrepareHardware;
EVT_WDF_DEVICE_RELEASE_HARDWARE     EvtDeviceReleaseHardware;
EVT_WDF_DEVICE_CONTEXT_CLEANUP      EvtDeviceContextCleanup;
EVT_WDF_DEVICE_D0_ENTRY             EvtDeviceD0Entry;
EVT_WDF_DEVICE_D0_EXIT              EvtDeviceD0Exit;

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

// Forward Declartion of helper functions
//
// Function to Fetch and parse Group Terminal Block information
//
_Must_inspect_result_
__drv_requiresIRQL(PASSIVE_LEVEL)
PAGED_CODE_SEG
NTSTATUS
USBMIDI2DriverGetGTB(
    _In_ WDFDEVICE    Device
);

//
// Function to Create Group Terminal Block information from
// USB MIDI 1.0 device confiugation descriptor information.
//
_Must_inspect_result_
__drv_requiresIRQL(PASSIVE_LEVEL)
PAGED_CODE_SEG
NTSTATUS
USBMIDI2DriverCreateGTB(
    _In_ WDFDEVICE    Device
);

//
// Function to get flags indicating in and Out GTB IDs active
// from Endpoint information in USB descriptors.
//
_Must_inspect_result_
__drv_requiresIRQL(PASSIVE_LEVEL)
PAGED_CODE_SEG
NTSTATUS
USBMIDI2DriverGetGTBIndexes(
    _In_    WDFDEVICE      Device
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
// Function to handle continuous reader setting
// 
__drv_maxIRQL(PASSIVE_LEVEL)
NONPAGED_CODE_SEG
VOID
USBMIDI2DriverIoContinuousReader(
    _In_ WDFDEVICE  Device,
    _In_ BOOLEAN    runContinuousReader,
    _In_ BOOLEAN    setActive
);

//
// Function to complete read on IN Pipe
//
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
BOOLEAN 
USBMIDI2DriverEvtReadFailed(
    WDFUSBPIPE      Pipe,
    NTSTATUS        status,
    USBD_STATUS     UsbdStatus
);

// 
// Aid Function for USB Write
__drv_maxIRQL(PASSIVE_LEVEL)
NONPAGED_CODE_SEG
VOID
USBMIDI2DriverIoWrite(
    _In_ WDFDEVICE  Device,
    _In_ PVOID      BufferStart,
    _In_ size_t     numBytes,
    _In_ BOOLEAN    isMoreData
);

_Must_inspect_result_
__drv_maxIRQL(PASSIVE_LEVEL)
NONPAGED_CODE_SEG
BOOLEAN
USBMIDI2DriverSendToUSB(
    _In_ WDFREQUEST         usbRequest,
    _In_ WDFMEMORY          reqMemory,
    _In_ WDFUSBPIPE         pipe,
    _In_ size_t             Length,
    _In_ PDEVICE_CONTEXT    pDeviceContext,
    _In_ BOOLEAN            deleteRequest
);

__drv_maxIRQL(PASSIVE_LEVEL)
NONPAGED_CODE_SEG
VOID
USBMIDI2DriverEvtRequestWriteCompletionRoutineDelete(
    _In_ WDFREQUEST                  Request,
    _In_ WDFIOTARGET                 Target,
    _In_ PWDF_REQUEST_COMPLETION_PARAMS CompletionParams,
    _In_ WDFCONTEXT                  Context
);

__drv_maxIRQL(PASSIVE_LEVEL)
NONPAGED_CODE_SEG
VOID
USBMIDI2DriverEvtRequestWriteCompletionRoutineDelete(
    _In_ WDFREQUEST                  Request,
    _In_ WDFIOTARGET                 Target,
    _In_ PWDF_REQUEST_COMPLETION_PARAMS CompletionParams,
    _In_ WDFCONTEXT                  Context
);

__drv_maxIRQL(PASSIVE_LEVEL)
NONPAGED_CODE_SEG
VOID
USBMIDI2DriverEvtRequestWriteCompletionRoutine(
    _In_ WDFREQUEST                  Request,
    _In_ WDFIOTARGET                 Target,
    _In_ PWDF_REQUEST_COMPLETION_PARAMS CompletionParams,
    _In_ WDFCONTEXT                  Context
);

// TODO: Put into dedicated library file
__drv_maxIRQL(PASSIVE_LEVEL)
NONPAGED_CODE_SEG
BOOL
USBMIDI1ToUMP(
    _In_    PUINT32        usbMidi1Pkt,
    _Inout_ bool           *pbIsInSysex,
    _Out_   PUMP_PACKET    umpPkt
);
