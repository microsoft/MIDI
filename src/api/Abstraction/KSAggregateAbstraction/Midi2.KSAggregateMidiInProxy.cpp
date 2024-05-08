// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

#include "pch.h"

_Use_decl_annotations_
HRESULT
CMidi2KSAggregateMidiInProxy::Initialize(
    LPCWSTR Device,
    HANDLE Filter,
    UINT PinId,
    ULONG BufferSize,
    DWORD* MmcssTaskId,
    IMidiCallback* Callback,
    LONGLONG Context,
    BYTE GroupIndex
)
{
    TraceLoggingWrite(
        MidiKSAggregateAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(Device, "device id"),
        TraceLoggingUInt32(PinId, "pin id"),
        TraceLoggingUInt8(GroupIndex, "group index")
        );

    m_callback = Callback;
    m_context = Context;
    m_groupIndex = GroupIndex;

    // create the KS device and add to our runtime map
    //m_device.reset(new (std::nothrow) KSMidiInDevice());
    m_device = std::make_unique<KSMidiInDevice>();
    RETURN_IF_NULL_ALLOC(m_device);

    auto initResult =
        m_device->Initialize(
            Device,
            Filter,
            PinId,
            MidiTransport::MidiTransport_StandardByteStream,
            BufferSize,
            MmcssTaskId,
            (IMidiCallback* )this,
            Context
        );

    if (FAILED(initResult))
    {
        TraceLoggingWrite(
            MidiKSAggregateAbstractionTelemetryProvider::Provider(),
            __FUNCTION__,
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Unable to initialize sub-device for input", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingHResult(initResult, MIDI_TRACE_EVENT_HRESULT_FIELD),
            TraceLoggingWideString(Device, "device id"),
            TraceLoggingUInt32(PinId, "pin id")
        );

        m_device = nullptr;
        RETURN_IF_FAILED(initResult);
    }

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidi2KSAggregateMidiInProxy::Callback(
    PVOID Data, 
    UINT Length,
    LONGLONG Timestamp,
    LONGLONG Context
)
{
    //TraceLoggingWrite(
    //    MidiKSAggregateAbstractionTelemetryProvider::Provider(),
    //    __FUNCTION__,
    //    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
    //    TraceLoggingPointer(this, "this"),
    //    TraceLoggingWideString(L"Message received from bytestream endpoint", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    //);

    // Translate bytestream to UMP, adding in correct group number, and call m_callback with the result

    // Send the bytestream byte(s) to the parser

    m_BS2UMP.defaultGroup = m_groupIndex;

    BYTE* data = (BYTE*)Data;
    for (UINT i = 0; i < Length; i++)
    {
        m_BS2UMP.bytestreamParse(data[i]);
    }

    // retrieve the UMP(s) from the parser
    // and sent it on
    while (m_BS2UMP.availableUMP())
    {
        uint32_t umpMessage[MAXIMUM_LOOPED_DATASIZE / sizeof(uint32_t)];
        UINT umpCount;
        for (umpCount = 0; umpCount < _countof(umpMessage) && m_BS2UMP.availableUMP(); umpCount++)
        {
            umpMessage[umpCount] = m_BS2UMP.readUMP();
        }

        if (umpCount > 0)
        {
            // there are 4 bytes per each 32 bit UMP returned by the parser.
            RETURN_IF_FAILED(m_callback->Callback(&(umpMessage[0]), umpCount * sizeof(uint32_t), Timestamp, Context));
        }
    }

    return S_OK;
}

HRESULT
CMidi2KSAggregateMidiInProxy::Cleanup()
{
    TraceLoggingWrite(
        MidiKSAggregateAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Cleanup", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    m_callback = nullptr;

    m_device->Cleanup();
    m_device = nullptr;

    return S_OK;
}
