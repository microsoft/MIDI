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
