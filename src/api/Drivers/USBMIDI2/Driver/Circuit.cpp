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

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY
    KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR
    PURPOSE.

Module Name:

    Circuit.cpp

Abstract:

    Plug and Play module. This file contains routines to handle pnp requests.

Environment:

    Kernel mode

--*/

#include "Pch.h"
#include "Common.h"

#include "Trace.h"
#include "Circuit.tmh"

static
KSDATAFORMAT g_MidiFormat =
{
    sizeof(KSDATAFORMAT),
    0,
    0,
    0,
    STATICGUIDOF(KSDATAFORMAT_TYPE_MUSIC),
    STATICGUIDOF(KSDATAFORMAT_SUBTYPE_UNIVERSALMIDIPACKET),
    STATICGUIDOF(KSDATAFORMAT_SPECIFIER_NONE)
};

DEFINE_GUID(MIDI_COMPONENT_GUID,    0xb0eb4f78, 0xfaa7, 0x4f28, 0x8f, 0x08, 0x9b, 0xf4, 0x16, 0x7f, 0x99, 0xfc);

_Use_decl_annotations_
PAGED_CODE_SEG
VOID
EvtPinContextCleanup(
    WDFOBJECT      Pin
   )
/*++

Routine Description:

    Pin cleanup context.

Arguments:

    WDF ACX pin object

--*/
{
    // The context is associated with a WDFOBJECT, hence the cleanup is 
    // guaranteed to be called at PASSIVE_LEVEL.
    _Analysis_assume_(KeGetCurrentIrql() == PASSIVE_LEVEL);

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Entry");

    UNREFERENCED_PARAMETER(Pin);

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Exit");
}

_Use_decl_annotations_
PAGED_CODE_SEG
NTSTATUS
AddMidi(
    WDFDEVICE      Device
    )
{
    NTSTATUS            status;
    PDEVICE_CONTEXT     devCtx;
    ACXCIRCUIT          midiCircuit = nullptr;

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Entry");

    devCtx = GetDeviceContext(Device);
    ASSERT(devCtx != nullptr);

    //
    // Create a midi circuit.
    //
    status = CreateMidiCircuit(Device, &midiCircuit);
    if (!NT_SUCCESS(status)) 
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "CreateMidiCircuit failed %!STATUS!", status);
        goto exit;
    }

    devCtx->Midi = midiCircuit;

    status = STATUS_SUCCESS;

exit:
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Exit");

    return status;
}

_Use_decl_annotations_
PAGED_CODE_SEG
NTSTATUS
CreateMidiCircuit(
    WDFDEVICE      Device,
    ACXCIRCUIT *   Circuit
    )
/*++

Routine Description:

    This routine builds the MIDI circuit.

Return Value:

    NT status value

--*/
{
    NTSTATUS                        status;
    WDF_OBJECT_ATTRIBUTES           attributes;
    ACXCIRCUIT                      circuit;
    PACXCIRCUIT_INIT                circuitInit = nullptr;
    MIDI_CIRCUIT_CONTEXT *          circuitCtx;
    ACXPIN                          pin[NumMidiPins];
    ACX_PIN_CONFIG                  pinCfg;
    MIDI_PIN_CONTEXT *              pinCtx;
    ACX_CIRCUIT_PNPPOWER_CALLBACKS  powerCallbacks;
    ACX_DATAFORMAT_CONFIG           formatCfg;
    ACXDATAFORMAT                   formatMidi;
    ACXDATAFORMATLIST               formatList;

    DECLARE_CONST_UNICODE_STRING(circuitName, L"USBMIDI2");

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Entry");
    
    //
    // Init output value.
    //
    *Circuit = nullptr;

    ///////////////////////////////////////////////////////////
    //
    // Create a circuit.
    //

    //
    // Get a CircuitInit structure.
    //
    circuitInit = AcxCircuitInitAllocate(Device);

    //
    // Add circuit identifiers.
    //
    AcxCircuitInitSetComponentId(circuitInit, &MIDI_COMPONENT_GUID);

    status = AcxCircuitInitAssignName(circuitInit, &circuitName);
    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "AcxCircuitInitAssignName failed %!STATUS!", status);
        goto exit;
    }

    //
    // Add circuit type.
    //
    AcxCircuitInitSetCircuitType(circuitInit, AcxCircuitTypeOther);

    //
    // Assign the circuit's pnp-power callbacks.
    //
    ACX_CIRCUIT_PNPPOWER_CALLBACKS_INIT(&powerCallbacks);
    powerCallbacks.EvtAcxCircuitPowerUp = EvtCircuitPowerUp;
    powerCallbacks.EvtAcxCircuitPowerDown = EvtCircuitPowerDown;
    AcxCircuitInitSetAcxCircuitPnpPowerCallbacks(circuitInit, &powerCallbacks);

    status = AcxCircuitInitAssignAcxCreateStreamCallback(circuitInit, EvtCircuitCreateStream);
    if (!NT_SUCCESS(status)) 
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "AcxCircuitInitAssignAcxCreateStreamCallback failed %!STATUS!", status);
        goto exit;
    }

    //
    // Create the circuit.
    //
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, MIDI_CIRCUIT_CONTEXT);
    status = AcxCircuitCreate(Device, &attributes, &circuitInit, &circuit);
    if (!NT_SUCCESS(status)) 
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "AcxCircuitCreate failed %!STATUS!", status);
        goto exit;
    }

    ASSERT(circuit != nullptr);
    circuitCtx = GetMidiCircuitContext(circuit);
    ASSERT(circuitCtx);
    UNREFERENCED_PARAMETER(circuitCtx);
        
    //
    // Post circuit creation initialization.
    //

    ///////////////////////////////////////////////////////////
    //
    // Allocate the formats this circuit supports.
    //
    ACX_DATAFORMAT_CONFIG_INIT_KS(&formatCfg, &g_MidiFormat);
    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.ParentObject = circuit;

    status = AcxDataFormatCreate(Device, &attributes, &formatCfg, &formatMidi);
    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "AcxDataFormatCreate failed %!STATUS!", status);
        goto exit;
    }

    ASSERT(formatMidi != nullptr);

    ///////////////////////////////////////////////////////////
    //
    // Create MIDI out pin.
    //
    ACX_PIN_CONFIG_INIT(&pinCfg);
    pinCfg.Type = AcxPinTypeSink;
    pinCfg.Communication = AcxPinCommunicationSink;
    pinCfg.Category = &KSCATEGORY_AUDIO;
    pinCfg.Name = &KSAUDFNAME_MIDI;
    //pinCfg.MaxStreams = 1;

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, MIDI_PIN_CONTEXT);
    attributes.EvtCleanupCallback = EvtPinContextCleanup;
    attributes.ParentObject = circuit;

    status = AcxPinCreate(circuit, &attributes, &pinCfg, &pin[MidiPinTypeMidiOut]);
    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "AcxPinCreate failed %!STATUS!", status);
        goto exit;
    }

    ASSERT(pin[MidiPinTypeMidiOut] != nullptr);
    pinCtx = GetMidiPinContext(pin[MidiPinTypeMidiOut]);
    ASSERT(pinCtx);

    //
    // Add our supported formats to the Raw mode for the Pin
    //
    formatList = AcxPinGetRawDataFormatList(pin[MidiPinTypeMidiOut]);
    if (formatList == nullptr)
    {
        status = STATUS_INSUFFICIENT_RESOURCES;
        goto exit;
    }

    status = AcxDataFormatListAddDataFormat(formatList, formatMidi);
    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "AcxDataFormatListAddDataFormat failed %!STATUS!", status);
        goto exit;
    }

    ///////////////////////////////////////////////////////////
    //
    // Create MIDI out bridge pin.
    //
    ACX_PIN_CONFIG_INIT(&pinCfg);
    pinCfg.Type = AcxPinTypeSource;
    pinCfg.Communication = AcxPinCommunicationNone;
    pinCfg.Category = &KSCATEGORY_AUDIO;
    pinCfg.Qualifier = AcxPinQualifierBridgeDevice;

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, MIDI_PIN_CONTEXT);
    attributes.EvtCleanupCallback = EvtPinContextCleanup;
    attributes.ParentObject = circuit;

    status = AcxPinCreate(circuit, &attributes, &pinCfg, &pin[MidiPinTypeDeviceMidiOut]);
    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "AcxPinCreate failed %!STATUS!", status);
        goto exit;
    }

    ASSERT(pin[MidiPinTypeDeviceMidiOut] != nullptr);
    pinCtx = GetMidiPinContext(pin[MidiPinTypeDeviceMidiOut]);
    ASSERT(pinCtx);

    ///////////////////////////////////////////////////////////
    //
    // Create MIDI in pin.
    //
    ACX_PIN_CONFIG_INIT(&pinCfg);
    pinCfg.Type = AcxPinTypeSource;
    pinCfg.Communication = AcxPinCommunicationSink;
    pinCfg.Category = &KSCATEGORY_AUDIO;
    pinCfg.Name = &KSAUDFNAME_MIDI;
    //pinCfg.MaxStreams = 1;

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, MIDI_PIN_CONTEXT);
    attributes.EvtCleanupCallback = EvtPinContextCleanup;
    attributes.ParentObject = circuit;

    status = AcxPinCreate(circuit, &attributes, &pinCfg, &pin[MidiPinTypeMidiIn]);
    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "AcxPinCreate failed %!STATUS!", status);
        goto exit;
    }

    ASSERT(pin[MidiPinTypeMidiIn] != nullptr);
    pinCtx = GetMidiPinContext(pin[MidiPinTypeMidiIn]);
    ASSERT(pinCtx);

    //
    // Add our supported formats to the Raw mode for the Pin
    //
    formatList = AcxPinGetRawDataFormatList(pin[MidiPinTypeMidiIn]);
    if (formatList == nullptr)
    {
        status = STATUS_INSUFFICIENT_RESOURCES;
        goto exit;
    }

    status = AcxDataFormatListAddDataFormat(formatList, formatMidi);
    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "AcxDataFormatListAddDataFormat failed %!STATUS!", status);
        goto exit;
    }

    ///////////////////////////////////////////////////////////
    //
    // Create MIDI bridge out pin.
    //
    ACX_PIN_CONFIG_INIT(&pinCfg);
    pinCfg.Type = AcxPinTypeSink;
    pinCfg.Communication = AcxPinCommunicationNone;
    pinCfg.Category = &KSCATEGORY_AUDIO;
    pinCfg.Qualifier = AcxPinQualifierBridgeDevice;

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, MIDI_PIN_CONTEXT);
    attributes.ParentObject = circuit;

    status = AcxPinCreate(circuit, &attributes, &pinCfg, &(pin[MidiPinTypeDeviceMidiIn]));
    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "AcxPinCreate failed %!STATUS!", status);
        goto exit;
    }

    ASSERT(pin[MidiPinTypeDeviceMidiIn] != nullptr);
    pinCtx = GetMidiPinContext(pin[MidiPinTypeDeviceMidiIn]);
    ASSERT(pinCtx);

    ///////////////////////////////////////////////////////////
    //
    // Add the pins
    //
    status = AcxCircuitAddPins(circuit, pin, NumMidiPins);
    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "AcxCircuitAddPins failed %!STATUS!", status);
        goto exit;
    }

    //
    // Explicitly connect the circuit/elements. Note that driver doens't 
    // need to perform this step when circuit/elements are connected in the 
    // same order as they were added to the circuit. By default ACX connects
    // the elements starting from the sink circuit pin and ending on the 
    // source circuit pin on both render and capture devices.
    //
    // Circuit Pins:
    // Pin 0 - Midi Out
    // Pin 1 - Midi bridge Out
    // Pin 2 - Midi In
    // Pin 3 - Midi bridge In
    //
    //      |-------------------------------|
    //      |           CIRCUIT             |
    //      |                               |
    // 0 -->|-------------------------------|--> 1
    //      |                               |
    // 2 <--|-------------------------------|<-- 3
    //      |                               |
    //      |-------------------------------|
    //
    ACX_CONNECTION connections[2];

    ACX_CONNECTION_INIT(&connections[0], circuit, circuit);
    connections[0].FromPin.Id = MidiPinTypeMidiOut;
    connections[0].ToPin.Id = MidiPinTypeDeviceMidiOut;

    ACX_CONNECTION_INIT(&connections[1], circuit, circuit);
    connections[1].FromPin.Id = MidiPinTypeDeviceMidiIn;
    connections[1].ToPin.Id = MidiPinTypeMidiIn;

    //
    // Add the connection
    // For testing add the last connection in a separate step.
    status = AcxCircuitAddConnections(circuit, connections, SIZEOF_ARRAY(connections));
    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "AcxCircuitAddConnections failed %!STATUS!", status);
        goto exit;
    }

    //
    // Set output value.
    //
    *Circuit = circuit;

    //
    // Done.
    //
    status = STATUS_SUCCESS;

exit:
    if (!NT_SUCCESS(status))
    {
        if (circuitInit)
        {
            AcxCircuitInitFree(circuitInit);
        }
    }
    
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Exit");

    return status;
}

_Use_decl_annotations_
PAGED_CODE_SEG
NTSTATUS
EvtCircuitPowerUp (
    WDFDEVICE  Device,
    ACXCIRCUIT Circuit,
    WDF_POWER_DEVICE_STATE PreviousState
    )
{
    UNREFERENCED_PARAMETER(Device);
    UNREFERENCED_PARAMETER(Circuit);
    UNREFERENCED_PARAMETER(PreviousState);

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Entry");

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Exit");

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
PAGED_CODE_SEG
NTSTATUS
EvtCircuitPowerDown (
    WDFDEVICE  Device,
    ACXCIRCUIT Circuit,
    WDF_POWER_DEVICE_STATE TargetState
    )
{
    UNREFERENCED_PARAMETER(Device);
    UNREFERENCED_PARAMETER(Circuit);
    UNREFERENCED_PARAMETER(TargetState);

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Entry");

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Exit");

    return STATUS_SUCCESS;
}

