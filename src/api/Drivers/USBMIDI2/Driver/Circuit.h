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

    Circuit.h

Abstract:

    Contains structure definitions and function prototypes private to
    the driver.

Environment:

    Kernel mode

--*/

#ifndef _CIRCUIT_H_
#define _CIRCUIT_H_

/* make prototypes usable from C++ */
#ifdef __cplusplus
extern "C" {
#endif

//
// Define MIDI circuit context.
//
typedef struct _MIDI_CIRCUIT_CONTEXT {
    ULONG   Dummy;
} MIDI_CIRCUIT_CONTEXT, *PMIDI_CIRCUIT_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(MIDI_CIRCUIT_CONTEXT, GetMidiCircuitContext)

typedef enum _MIDI_PIN_TYPE {
    MidiPinTypeMidiOut,
    MidiPinTypeDeviceMidiOut,
    MidiPinTypeMidiIn,
    MidiPinTypeDeviceMidiIn,
    NumMidiPins
} MIDI_PIN_TYPE, *PMIDI_PIN_TYPE;

typedef struct _MIDI_PIN_CONTEXT {
    ULONG   StreamCount;
} MIDI_PIN_CONTEXT, *PMIDI_PIN_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(MIDI_PIN_CONTEXT, GetMidiPinContext)

// Midi callbacks.
EVT_WDF_DEVICE_CONTEXT_CLEANUP      EvtDeviceContextCleanup;
EVT_ACX_CIRCUIT_CREATE_STREAM       EvtCircuitCreateStream;
EVT_ACX_CIRCUIT_POWER_UP            EvtCircuitPowerUp;
EVT_ACX_CIRCUIT_POWER_DOWN          EvtCircuitPowerDown;
EVT_WDF_DEVICE_CONTEXT_CLEANUP      EvtPinContextCleanup;

EVT_ACX_OBJECT_PROCESS_REQUEST      EvtPinInterfacesCallback;

/* make internal prototypes usable from C++ */
#ifdef __cplusplus
}
#endif

_Must_inspect_result_
__drv_maxIRQL(PASSIVE_LEVEL)
PAGED_CODE_SEG
NTSTATUS
AddMidi(
    _In_    WDFDEVICE       Device
    );

_Must_inspect_result_
__drv_maxIRQL(PASSIVE_LEVEL)
PAGED_CODE_SEG
NTSTATUS
CreateMidiCircuit(
    _In_     WDFDEVICE      Device,
    _Out_    ACXCIRCUIT *   Circuit
    );

#endif // _CIRCUIT_H_
