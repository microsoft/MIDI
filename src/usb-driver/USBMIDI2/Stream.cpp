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

    Steam.cpp - Routines for creating and establishing ACX Circuit

Abstract:

   This file contains ACX support routines for Circuit.

Environment:

    Kernel-mode Driver Framework

--*/
// Copyright (c) Microsoft Corporation. All rights reserved.

#include "Pch.h"

extern wil::fast_mutex_with_critical_region *g_MidiInLock;

static ACX_PROPERTY_ITEM MidiStreamProperties[] =
{
    {
        &KSPROPSETID_MidiLoopedStreaming,
        KSPROPERTY_MIDILOOPEDSTREAMING_BUFFER,
        ACX_PROPERTY_ITEM_FLAG_GET,
        EvtMidiGetLoopedStreamingBufferCallback,
        0,
        sizeof(ULONG),
        sizeof(KSMIDILOOPED_BUFFER),
    },
    {
        &KSPROPSETID_MidiLoopedStreaming,
        KSPROPERTY_MIDILOOPEDSTREAMING_REGISTERS,
        ACX_PROPERTY_ITEM_FLAG_GET,
        EvtMidiGetLoopedStreamingRegistersCallback,
        0,
        0,
        sizeof(KSMIDILOOPED_REGISTERS)
    },
    {
        &KSPROPSETID_MidiLoopedStreaming,
        KSPROPERTY_MIDILOOPEDSTREAMING_NOTIFICATION_EVENT,
        ACX_PROPERTY_ITEM_FLAG_SET,
        EvtMidiSetLoopedStreamingNotificationEventCallback,
        0,
        0,
        sizeof(KSMIDILOOPED_EVENT)
    },
    {
        &KSPROPSETID_MIDI2_ENDPOINT_INFORMATION,
        KSPROPERTY_MIDI2_NATIVEDATAFORMAT,
        ACX_PROPERTY_ITEM_FLAG_GET,
        EvtMidi2NativeDataFormatCallback,
        0,
        0,
        sizeof(GUID)
    },
    {
        &KSPROPSETID_MIDI2_ENDPOINT_INFORMATION,
        KSPROPERTY_MIDI2_GROUP_TERMINAL_BLOCKS,
        ACX_PROPERTY_ITEM_FLAG_GET,
        EvtMidi2GroupTerminalBlocksCallback,
    },
};

static ULONG MidiStreamPropertiesCount = SIZEOF_ARRAY(MidiStreamProperties);

_Use_decl_annotations_
PAGED_CODE_SEG
NTSTATUS
EvtCircuitCreateStream(
    WDFDEVICE       Device,
    ACXCIRCUIT      Circuit,
    ACXPIN          Pin,
    PACXSTREAM_INIT StreamInit,
    ACXDATAFORMAT   StreamFormat,
    const GUID    * SignalProcessingMode,
    ACXOBJECTBAG    VarArguments
    )
/*++

Routine Description:

    This routine create a stream for the specified circuit.

Return Value:

    NT status value

--*/
{
    NTSTATUS                        status;
    WDF_OBJECT_ATTRIBUTES           attributes;
    MIDI_PIN_CONTEXT *              pinCtx;
    ACXSTREAM                       stream;
    MIDI_STREAM_CONTEXT *           streamCtx;
    ACX_STREAM_CALLBACKS            streamCallbacks;
    StreamEngine *                  streamEngine = nullptr;

    PAGED_CODE();

    UNREFERENCED_PARAMETER(Device);
    UNREFERENCED_PARAMETER(Circuit);
    UNREFERENCED_PARAMETER(StreamFormat);
    UNREFERENCED_PARAMETER(SignalProcessingMode);
    UNREFERENCED_PARAMETER(VarArguments);

    ASSERT(IsEqualGUID(*SignalProcessingMode, AUDIO_SIGNALPROCESSINGMODE_RAW));

    if (AcxPinGetId(Pin) != MidiPinTypeMidiIn && AcxPinGetId(Pin) != MidiPinTypeMidiOut)
    {
        status = STATUS_INVALID_PARAMETER;
        goto exit;
    }

    //
    // Only one stream x pin is allowed.
    //
    pinCtx = GetMidiPinContext(Pin);
    ASSERT(pinCtx);

    if (pinCtx->StreamCount)
    {
        status = STATUS_INSUFFICIENT_RESOURCES;
        goto exit;
    }

    //
    // Init streaming callbacks.
    //
    ACX_STREAM_CALLBACKS_INIT(&streamCallbacks);
    streamCallbacks.EvtAcxStreamPrepareHardware     = EvtStreamPrepareHardware;
    streamCallbacks.EvtAcxStreamReleaseHardware     = EvtStreamReleaseHardware;
    streamCallbacks.EvtAcxStreamRun                 = EvtStreamRun;
    streamCallbacks.EvtAcxStreamPause               = EvtStreamPause;

    status = AcxStreamInitAssignAcxStreamCallbacks(StreamInit, &streamCallbacks);
    if (!NT_SUCCESS(status))
    {
        ASSERT(FALSE);
        goto exit;
    }

    //
    // Add properties, events and methods.
    //
    status = AcxStreamInitAssignProperties(StreamInit,
                                           MidiStreamProperties,
                                           MidiStreamPropertiesCount);
    if (!NT_SUCCESS(status)) 
    {
        ASSERT(FALSE);
        goto exit;
    }

    //
    // Create the stream.
    //
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, MIDI_STREAM_CONTEXT);
    attributes.EvtCleanupCallback = EvtStreamCleanup;
    attributes.EvtDestroyCallback = EvtStreamDestroy;
    status = AcxStreamCreate(Device, Circuit, &attributes, &StreamInit, &stream);
    if (!NT_SUCCESS(status)) 
    {
        ASSERT(FALSE);
        goto exit;
    }

    //
    // Post stream creation initialization.
    //
    streamEngine = new (POOL_FLAG_NON_PAGED) StreamEngine(Pin);
    if (streamEngine == nullptr)
    {
        status = STATUS_INSUFFICIENT_RESOURCES;
        ASSERT(FALSE);
        goto exit;
    }

    streamCtx = GetMidiStreamContext(stream);
    ASSERT(streamCtx);

    streamCtx->StreamEngine = streamEngine;

    // Count the stream.
    pinCtx->StreamCount += 1;
    streamCtx->IsCounted = true;

    streamCtx->Pin = Pin;
    WdfObjectReferenceWithTag(Pin, (PVOID)DRIVER_TAG);

    //
    // Done.
    //
    status = STATUS_SUCCESS;

exit:

    return status;
}

_Use_decl_annotations_
PAGED_CODE_SEG
VOID
EvtMidiGetLoopedStreamingBufferCallback(
    WDFOBJECT   Object,
    WDFREQUEST  Request
    )
{
    NTSTATUS                    status = STATUS_NOT_SUPPORTED;
    ACXSTREAM                   stream = (ACXSTREAM)Object;
    PMIDI_STREAM_CONTEXT        streamCtx = nullptr;
    ACX_REQUEST_PARAMETERS      params;
    ULONG_PTR                   outDataCb = 0;
    KSMIDILOOPED_BUFFER *       value;
    ULONG                       minValueSize;
    ULONG                       requestedBufferSize = 0;
    KAPC_STATE                  processApcState = {0};
    BOOLEAN                     processAttached = FALSE;
    PEPROCESS                   process = nullptr;

    PAGED_CODE();

    ASSERT(stream);
    streamCtx = GetMidiStreamContext(stream);
    ASSERT(streamCtx);
    ASSERT(streamCtx->StreamEngine);

    ACX_REQUEST_PARAMETERS_INIT(&params);
    AcxRequestGetParameters(Request, &params);

    ASSERT(params.Type == AcxRequestTypeProperty);
    ASSERT(params.Parameters.Property.Verb == AcxPropertyVerbGet);
    ASSERT(params.Parameters.Property.Control != nullptr);
    ASSERT(params.Parameters.Property.ControlCb >= sizeof(ULONG));
    ASSERT(params.Parameters.Property.Value != nullptr);
    ASSERT(params.Parameters.Property.ValueCb == sizeof(KSMIDILOOPED_BUFFER));

    //
    // Basic validation (ACX validated this already).
    //
    if (nullptr == params.Parameters.Property.Control ||
        params.Parameters.Property.ControlCb < sizeof(ULONG) ||
        nullptr == params.Parameters.Property.Value ||
        params.Parameters.Property.ValueCb < sizeof(KSMIDILOOPED_BUFFER) ||
        nullptr == streamCtx->StreamEngine)
    {
        ASSERT(FALSE);
        outDataCb = 0;
        status = STATUS_INVALID_PARAMETER;
        goto exit;
    }

    //
    // Get and validate buffer size (assume a min of 8 bytes)
    //
    requestedBufferSize = *(PULONG)params.Parameters.Property.Control;
    if (requestedBufferSize < sizeof(ULONGLONG))
    {
        ASSERT(FALSE);
        outDataCb = 0;
        status = STATUS_INVALID_PARAMETER;
        goto exit;
    }

    minValueSize = sizeof(KSMIDILOOPED_BUFFER);
    value = (KSMIDILOOPED_BUFFER*)params.Parameters.Property.Value;

    //
    // Make sure we are running in the caller original process.
    //
    status = AttachToCallerProcess(Request, &processApcState, &processAttached, &process);
    if (!NT_SUCCESS(status))
    {
        ASSERT(FALSE);
        outDataCb = 0;
        goto exit;
    }

    // Make sure process is the original one.
    if (streamCtx->Process != nullptr)
    {
        status = STATUS_INVALID_DEVICE_STATE;
        ASSERT(FALSE);
        outDataCb = 0;
        goto exit;
    }

    ObReferenceObjectWithTag(process, DRIVER_TAG);
    streamCtx->Process = process;

    //
    // Call handler for buffer allocation.
    //
    status = streamCtx->StreamEngine->GetLoopedStreamingBuffer(requestedBufferSize, value);
    if (!NT_SUCCESS(status))
    {
        ASSERT(FALSE);
        outDataCb = 0;
        goto exit;
    }

    //
    // Operation completed successfully.
    //
    outDataCb = minValueSize;
    status = STATUS_SUCCESS;

exit:
    if (processAttached)
    {
        RestoreProcess(&processApcState);
    }

    WdfRequestCompleteWithInformation(Request, status, outDataCb);
}

_Use_decl_annotations_
PAGED_CODE_SEG
VOID
EvtMidiGetLoopedStreamingRegistersCallback(
    WDFOBJECT   Object,
    WDFREQUEST  Request
    )
{
    NTSTATUS                    status = STATUS_NOT_SUPPORTED;
    ACXSTREAM                   stream = (ACXSTREAM)Object;
    PMIDI_STREAM_CONTEXT        streamCtx = nullptr;
    ACX_REQUEST_PARAMETERS      params;
    ULONG_PTR                   outDataCb = 0;
    KSMIDILOOPED_REGISTERS *    value;
    ULONG                       minValueSize;
    KAPC_STATE                  processApcState = {0};
    BOOLEAN                     processAttached = FALSE;
    PEPROCESS                   process = nullptr;

    PAGED_CODE();

    ASSERT(stream);
    streamCtx = GetMidiStreamContext(stream);
    ASSERT(streamCtx);
    ASSERT(streamCtx->StreamEngine);

    ACX_REQUEST_PARAMETERS_INIT(&params);
    AcxRequestGetParameters(Request, &params);

    ASSERT(params.Type == AcxRequestTypeProperty);
    ASSERT(params.Parameters.Property.Verb == AcxPropertyVerbGet);
    ASSERT(params.Parameters.Property.Control == nullptr);
    ASSERT(params.Parameters.Property.ControlCb == 0);
    ASSERT(params.Parameters.Property.Value != nullptr);
    ASSERT(params.Parameters.Property.ValueCb == sizeof(KSMIDILOOPED_REGISTERS));

    //
    // Basic validation (ACX validated this already).
    //
    if (nullptr != params.Parameters.Property.Control ||
        params.Parameters.Property.ControlCb != 0 ||
        nullptr == params.Parameters.Property.Value ||
        params.Parameters.Property.ValueCb < sizeof(KSMIDILOOPED_REGISTERS) ||
        nullptr == streamCtx->StreamEngine)
    {
        ASSERT(FALSE);
        outDataCb = 0;
        status = STATUS_INVALID_PARAMETER;
        goto exit;
    }

    minValueSize = sizeof(KSMIDILOOPED_REGISTERS);
    value = (KSMIDILOOPED_REGISTERS*)params.Parameters.Property.Value;

    //
    // Make sure we are running in the caller original process.
    //
    status = AttachToCallerProcess(Request, &processApcState, &processAttached, &process);
    if (!NT_SUCCESS(status))
    {
        ASSERT(FALSE);
        outDataCb = 0;
        goto exit;
    }

    // Make sure process is the original one.
    if (streamCtx->Process != process)
    {
        status = STATUS_INVALID_DEVICE_STATE;
        ASSERT(FALSE);
        outDataCb = 0;
        goto exit;
    }

    //
    // Call handler for registry mapping.
    //
    status = streamCtx->StreamEngine->GetLoopedStreamingRegisters(value);
    if (!NT_SUCCESS(status))
    {
        ASSERT(FALSE);
        outDataCb = 0;
        goto exit;
    }

    //
    // Operation completed successfully.
    //
    outDataCb = minValueSize;
    status = STATUS_SUCCESS;

exit:
    if (processAttached)
    {
        RestoreProcess(&processApcState);
    }

    WdfRequestCompleteWithInformation(Request, status, outDataCb);
}

_Use_decl_annotations_
PAGED_CODE_SEG
VOID
EvtMidiSetLoopedStreamingNotificationEventCallback(
    WDFOBJECT   Object,
    WDFREQUEST  Request
    )
{
    NTSTATUS                    status = STATUS_NOT_SUPPORTED;
    ACXSTREAM                   stream = (ACXSTREAM)Object;
    PMIDI_STREAM_CONTEXT        streamCtx = nullptr;
    ACX_REQUEST_PARAMETERS      params;
    ULONG_PTR                   outDataCb = 0;
    KSMIDILOOPED_EVENT *        value;
    ULONG                       minValueSize;
    KAPC_STATE                  processApcState = {0};
    BOOLEAN                     processAttached = FALSE;
    PEPROCESS                   process = nullptr;

    PAGED_CODE();

    ASSERT(stream);
    streamCtx = GetMidiStreamContext(stream);
    ASSERT(streamCtx);
    ASSERT(streamCtx->StreamEngine);

    ACX_REQUEST_PARAMETERS_INIT(&params);
    AcxRequestGetParameters(Request, &params);

    ASSERT(params.Type == AcxRequestTypeProperty);
    ASSERT(params.Parameters.Property.Verb == AcxPropertyVerbSet);
    ASSERT(params.Parameters.Property.Control == nullptr);
    ASSERT(params.Parameters.Property.ControlCb == 0);
    ASSERT(params.Parameters.Property.Value != nullptr);
    ASSERT(params.Parameters.Property.ValueCb == sizeof(KSMIDILOOPED_EVENT));

    //
    // Basic validation (ACX validated this already).
    //
    if (nullptr != params.Parameters.Property.Control ||
        params.Parameters.Property.ControlCb != 0 ||
        nullptr == params.Parameters.Property.Value ||
        params.Parameters.Property.ValueCb < sizeof(KSMIDILOOPED_EVENT) ||
        nullptr == streamCtx->StreamEngine)
    {
        ASSERT(FALSE);
        outDataCb = 0;
        status = STATUS_INVALID_PARAMETER;
        goto exit;
    }

    minValueSize = sizeof(KSMIDILOOPED_EVENT);
    value = (KSMIDILOOPED_EVENT*)params.Parameters.Property.Value;

    //
    // Make sure we are running in the caller original process.
    //
    status = AttachToCallerProcess(Request, &processApcState, &processAttached, &process);
    if (!NT_SUCCESS(status))
    {
        ASSERT(FALSE);
        outDataCb = 0;
        goto exit;
    }

    // Make sure process is the original one.
    if (streamCtx->Process != process)
    {
        status = STATUS_INVALID_DEVICE_STATE;
        ASSERT(FALSE);
        outDataCb = 0;
        goto exit;
    }

    //
    // Call handler for registry mapping.
    //
    status = streamCtx->StreamEngine->SetLoopedStreamingNotificationEvent(value);
    if (!NT_SUCCESS(status))
    {
        ASSERT(FALSE);
        outDataCb = 0;
        goto exit;
    }

    //
    // Operation completed successfully.
    //
    outDataCb = minValueSize;
    status = STATUS_SUCCESS;

exit:
    if (processAttached)
    {
        RestoreProcess(&processApcState);
    }

    WdfRequestCompleteWithInformation(Request, status, outDataCb);
}

_Use_decl_annotations_
PAGED_CODE_SEG
VOID
EvtMidi2NativeDataFormatCallback(
    WDFOBJECT   Object,
    WDFREQUEST  Request
    )
{
    NTSTATUS                    status = STATUS_NOT_SUPPORTED;
    ACXSTREAM                   stream = (ACXSTREAM)Object;
    ACX_REQUEST_PARAMETERS      params;
    ULONG_PTR                   outDataCb = 0;

    PAGED_CODE();

    ACX_REQUEST_PARAMETERS_INIT(&params);
    AcxRequestGetParameters(Request, &params);

    WDFDEVICE devCtx = AcxCircuitGetWdfDevice(AcxStreamGetCircuit(stream));
    PDEVICE_CONTEXT pDevCtx = GetDeviceContext(devCtx);

    if (pDevCtx->UsbMIDIStreamingAlt)
    {
        RtlCopyMemory(params.Parameters.Property.Value, &KSDATAFORMAT_SUBTYPE_UNIVERSALMIDIPACKET, sizeof(GUID));
    }
    else
    {
        RtlCopyMemory(params.Parameters.Property.Value, &KSDATAFORMAT_SUBTYPE_MIDI, sizeof(GUID));
    }

    // return the amount of buffer actually used.
    outDataCb = sizeof(GUID);
    status = STATUS_SUCCESS;

    WdfRequestCompleteWithInformation(Request, status, outDataCb);
}

_Use_decl_annotations_
PAGED_CODE_SEG
VOID
EvtMidi2GroupTerminalBlocksCallback(
    WDFOBJECT   /* Object */,
    WDFREQUEST  Request
    )
{
    NTSTATUS                    status = STATUS_NOT_SUPPORTED;
    ACX_REQUEST_PARAMETERS      params;
    ULONG_PTR                   outDataCb = 0;
    ULONG                       minValueSize;

    PAGED_CODE();

    ACX_REQUEST_PARAMETERS_INIT(&params);
    AcxRequestGetParameters(Request, &params);

    // The size of the buffer provided by the caller
    ULONG valueCb = params.Parameters.Property.ValueCb;

    // TODO: set minValueSize to the required size of the group
    // terminal block data.
    minValueSize = MAX_PATH;

    // The following is required because the buffer size
    // is variable. If the size of the data buffer provided is 0, the
    // required size is returned to the caller.
    if (valueCb == 0)
    {
        outDataCb = minValueSize;
        status = STATUS_BUFFER_OVERFLOW;
        goto exit;
    }
    else if (valueCb < minValueSize)
    {
        // if a buffer was provided by the caller, but it is
        // too small, fail.
        outDataCb = 0;
        status = STATUS_BUFFER_TOO_SMALL;
        goto exit;
    }

    // TODO: copy the data to the output buffer, copying a 255 byte
    // empty buffer as a placeholer.
    BYTE data[MAX_PATH] {0};
    RtlCopyMemory(params.Parameters.Property.Value, &data, minValueSize);

    // return the amount of buffer actually used.
    outDataCb = minValueSize;
    status = STATUS_SUCCESS;

exit:
    WdfRequestCompleteWithInformation(Request, status, outDataCb);
}

_Use_decl_annotations_
PAGED_CODE_SEG
VOID
EvtStreamDestroy(
    WDFOBJECT Object
    )
{
    PAGED_CODE();
    UNREFERENCED_PARAMETER(Object);
}

_Use_decl_annotations_
PAGED_CODE_SEG
VOID
EvtStreamCleanup(
    WDFOBJECT Object
    )
{
    PMIDI_STREAM_CONTEXT streamCtx = nullptr;

    PAGED_CODE();

    streamCtx = GetMidiStreamContext((ACXSTREAM)Object);

    // Cleanup stream engine.
    if (streamCtx->StreamEngine)
    {
        BOOLEAN     processAttached = FALSE;
        KAPC_STATE  processApcState = {0};

        if (streamCtx->Process)
        {
            AttachToProcess(streamCtx->Process, &processApcState, &processAttached);
        }

        delete streamCtx->StreamEngine;

        if (processAttached)
        {
            RestoreProcess(&processApcState);
        }

        if (streamCtx->Process)
        {
            ObDereferenceObjectWithTag(streamCtx->Process, DRIVER_TAG);
            streamCtx->Process = nullptr;
        }

        streamCtx->StreamEngine = nullptr;
    }

    // Update stream count for pin.
    if (streamCtx->IsCounted)
    {
        PMIDI_PIN_CONTEXT pinCtx = nullptr;

        ASSERT(streamCtx->Pin);
        pinCtx = GetMidiPinContext(streamCtx->Pin);

        ASSERT(pinCtx->StreamCount == 1);
        pinCtx->StreamCount = 0;

        streamCtx->IsCounted = false;
    }

    // Release ref on Pin.
    if (streamCtx->Pin)
    {
        WdfObjectDereferenceWithTag(streamCtx->Pin, (PVOID)DRIVER_TAG);
        streamCtx->Pin = nullptr;
    }
}

_Use_decl_annotations_
PAGED_CODE_SEG
NTSTATUS
EvtStreamPrepareHardware(
    ACXSTREAM Stream
    )
{
    PMIDI_STREAM_CONTEXT ctx;
    StreamEngine * streamEngine = nullptr;

    PAGED_CODE();

    ctx = GetMidiStreamContext(Stream);
    streamEngine = (StreamEngine*)ctx->StreamEngine;
    return streamEngine->PrepareHardware();
}

_Use_decl_annotations_
PAGED_CODE_SEG
NTSTATUS
EvtStreamReleaseHardware(
    ACXSTREAM Stream
    )
{
    PMIDI_STREAM_CONTEXT ctx;
    StreamEngine * streamEngine = nullptr;

    PAGED_CODE();

    ctx = GetMidiStreamContext(Stream);
    streamEngine = (StreamEngine*)ctx->StreamEngine;
    return streamEngine->ReleaseHardware();
}

_Use_decl_annotations_
PAGED_CODE_SEG
NTSTATUS
EvtStreamRun(
    ACXSTREAM Stream
    )
{
    PMIDI_STREAM_CONTEXT ctx;
    StreamEngine * streamEngine = nullptr;

    PAGED_CODE();

    ctx = GetMidiStreamContext(Stream);
    streamEngine = (StreamEngine*)ctx->StreamEngine;
    return streamEngine->Run();
}


_Use_decl_annotations_
PAGED_CODE_SEG
NTSTATUS
EvtStreamPause(
    ACXSTREAM Stream
    )
{
    PMIDI_STREAM_CONTEXT ctx;
    StreamEngine * streamEngine = nullptr;

    PAGED_CODE();

    ctx = GetMidiStreamContext(Stream);
    streamEngine = (StreamEngine*)ctx->StreamEngine;
    return streamEngine->Pause();
}

_Use_decl_annotations_
PAGED_CODE_SEG
NTSTATUS
AttachToCallerProcess(
    WDFREQUEST        Request,
    KAPC_STATE      * ApcState,
    BOOLEAN         * Attached,
    PEPROCESS       * Process
    )
{
    NTSTATUS    status              = STATUS_SUCCESS;
    PIRP        irp                 = nullptr;
    PEPROCESS   requestorProcess    = nullptr;

    PAGED_CODE();

    //
    // Zero out output parameters.
    //
    *Attached = FALSE;
    if (Process)
    {
        *Process = nullptr;
    }
    RtlZeroMemory(ApcState, sizeof(*ApcState));

    irp = WdfRequestWdmGetIrp(Request);
    requestorProcess = IoGetRequestorProcess(irp);
    if (nullptr == requestorProcess)
    {
        // We can't map memory if there is no process associated with our caller
        status = STATUS_INVALID_PARAMETER;
        ASSERT(FALSE);
        goto exit;
    }

    if (IoGetCurrentProcess() != requestorProcess)
    {
        KeStackAttachProcess((PRKPROCESS)requestorProcess, ApcState);
        *Attached = TRUE;
    }

    if (Process)
    {
        *Process = requestorProcess;
    }

exit:
    return status;
}

_Use_decl_annotations_
PAGED_CODE_SEG
VOID
AttachToProcess(
    PEPROCESS       Process,
    KAPC_STATE    * ApcState,
    BOOLEAN       * Attached
    )
{
    PAGED_CODE();

    //
    // Zero out output parameters.
    //
    *Attached = FALSE;
    RtlZeroMemory(ApcState, sizeof(*ApcState));

    if (IoGetCurrentProcess() != Process)
    {
        KeStackAttachProcess((PRKPROCESS)Process, ApcState);
        *Attached = TRUE;
    }
}

_Use_decl_annotations_
PAGED_CODE_SEG
VOID
RestoreProcess(
    KAPC_STATE * ApcState
    )
{
    PAGED_CODE();
    KeUnstackDetachProcess(ApcState);
}

