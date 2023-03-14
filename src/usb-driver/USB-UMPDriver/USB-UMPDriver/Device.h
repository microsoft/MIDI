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

Module Name:

    device.h

Abstract:

    This file contains the device definitions.

Environment:

    Kernel-mode Driver Framework

--*/

#include "public.h"

#include "bytestreamToUMP.h"
#include "umpToBytestream.h"

EXTERN_C_START

//
// Structure to aid in conversion of USB MIDI 1.0 to UMP
typedef struct MIDI_STREAM_t
{
    UCHAR       buffer[4];
    UCHAR       index;
    UCHAR       total;
} MIDI_STREAM_TYPE;

//
// Structure to buffer read data between USB continuous reader
// and EvtIoRead callback from framework. Implements as a ring
// buffer.
//
typedef struct READ_RING_t
{
    WDFMEMORY   RingBufMemory;
    size_t      ringBufSize;
    size_t      ringBufHead;
    size_t      ringBufTail;
} READ_RING_TYPE, *PREAD_RING_TYPE;
#define USBUMPDRIVER_RING_BUF_SIZE 256  // twice size of HS USB buffer

//
// The device context performs the same job as
// a WDM device extension in the driver frameworks
//
typedef struct _DEVICE_CONTEXT
{
    WDFUSBDEVICE UsbDevice;
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

    // Read Ring Buffer
    READ_RING_TYPE              ReadRingBuf;

    // Utility for converting MIDI 1.0 <-> MIDI 2.0
    // Currently part of MIDI Association protected materials, to be released to Public soon
    // Used with permission
    bytestreamToUMP             streamToUMP;
    MIDI_STREAM_TYPE            streamPkt;
    umpToBytestream             umpToStream;
    MIDI_STREAM_TYPE            midi1UsbPkt;

    //
    // The following fields are used during event logging to 
    // report the events relative to this specific instance 
    // of the device.
    //

    WDFMEMORY                       DeviceNameMemory;
    PCWSTR                          DeviceName;

    WDFMEMORY                       LocationMemory;
    PCWSTR                          Location;

} DEVICE_CONTEXT, *PDEVICE_CONTEXT;

//
// This macro will generate an inline function called DeviceGetContext
// which will be used to get a pointer to the device context memory
// in a type safe manner.
//
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DEVICE_CONTEXT, DeviceGetContext)

//
// Function to initialize the device's queues and callbacks
//
NTSTATUS
USBUMPDriverCreateDevice(
    _Inout_ PWDFDEVICE_INIT DeviceInit
    );

//
// Function to select the device's USB configuration and get a WDFUSBDEVICE
// handle
//
EVT_WDF_DEVICE_PREPARE_HARDWARE USBUMPDriverEvtDevicePrepareHardware;

//
// PnP Power events
//
EVT_WDF_DEVICE_D0_ENTRY                 USBUMPDriverEvtDeviceD0Entry;
EVT_WDF_DEVICE_D0_EXIT                  USBUMPDriverEvtDeviceD0Exit;

//
// Callbacks for continuous reader
//
EVT_WDF_USB_READER_COMPLETION_ROUTINE   USBUMPDriverEvtReadComplete;
EVT_WDF_USB_READERS_FAILED              USBUMPDriverEvtReadFailed;

//
// Callbacks for read queue handling
// 
EVT_WDF_REQUEST_COMPLETION_ROUTINE      USBUMPDriverReadQueueEvtCompletionRoutine;

//
// Callbacks to write to USB MIDI Stream
//
EVT_WDF_IO_QUEUE_IO_WRITE               USBUMPDriverEvtIoWrite;
EVT_WDF_REQUEST_COMPLETION_ROUTINE      USBUMPDriverEvtRequestWriteCompletionRoutine;

// Forward Declartion of helper functions
//
// Function to parse descriptors and select usb Interface
//
NTSTATUS
USBUMPDriverSelectInterface(
    _In_ WDFDEVICE    Device
);

//
// Function to connect and prepare pipes for use
//
NTSTATUS
USBUMPDriverEnumeratePipes(
    _In_ WDFDEVICE    Device
);

//
// Function to fill and submit to read queue
//
BOOLEAN
USBUMPDriverFillReadQueue(
    _In_    PUINT32             pBuffer,
    _In_    size_t              bufferSize,
    _In_    PDEVICE_CONTEXT     pDeviceContext
);

//
// Function to write to USB through Out Pipe
//
BOOLEAN USBUMPDriverSendToUSB(
    _In_ WDFMEMORY          reqMemory,
    _In_ WDFUSBPIPE         pipe,
    _In_ size_t             Length,
    _In_ PDEVICE_CONTEXT    pDeviceContext
);

EXTERN_C_END
