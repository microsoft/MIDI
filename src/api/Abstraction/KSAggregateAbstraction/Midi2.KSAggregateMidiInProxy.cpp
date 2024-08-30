// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
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
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(Device, MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD),
        TraceLoggingUInt32(PinId, "Pin id"),
        TraceLoggingUInt8(GroupIndex, "Group index")
        );

    m_callback = Callback;
    m_context = Context;
    m_groupIndex = GroupIndex;

    m_endpointDeviceId = internal::NormalizeEndpointInterfaceIdWStringCopy(Device);

    m_countMidiMessageSent = 0;

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
            MIDI_TRACE_EVENT_ERROR,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Unable to initialize sub-device for input", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingHResult(initResult, MIDI_TRACE_EVENT_HRESULT_FIELD),
            TraceLoggingWideString(Device, MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD),
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
    RETURN_HR_IF_NULL(E_INVALIDARG, Data);
    RETURN_HR_IF_NULL(E_POINTER, m_callback);

    TraceLoggingWrite(
        MidiKSAggregateAbstractionTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_VERBOSE,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Callback received from device. Sending data to next callback.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingUInt32(Length, "data length"),
        TraceLoggingUInt64(Timestamp, MIDI_TRACE_EVENT_MESSAGE_TIMESTAMP_FIELD)
    );

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
        uint32_t umpMessage[MAXIMUM_LOOPED_UMP_DATASIZE / sizeof(uint32_t)];
        UINT wordCount;
        for (wordCount = 0; wordCount < _countof(umpMessage) && m_BS2UMP.availableUMP(); wordCount++)
        {
            umpMessage[wordCount] = m_BS2UMP.readUMP();
        }

        if (wordCount > 0)
        {
            // in case it goes null while we're in the loop
            RETURN_HR_IF_NULL(E_POINTER, m_callback);

            // there are 4 bytes per each 32 bit UMP returned by the parser.
            auto hr = m_callback->Callback(&(umpMessage[0]), wordCount * sizeof(uint32_t), Timestamp, Context);

            if (SUCCEEDED(hr))
            {
                m_countMidiMessageSent++;

                TraceLoggingWrite(
                    MidiKSAggregateAbstractionTelemetryProvider::Provider(),
                    MIDI_TRACE_EVENT_METRICS,
                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(L"Messages forwarded so far.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                    TraceLoggingUInt64(m_countMidiMessageSent, "count")
                );
            }
            else
            {
                TraceLoggingWrite(
                    MidiKSAggregateAbstractionTelemetryProvider::Provider(),
                    MIDI_TRACE_EVENT_ERROR,
                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(L"HRESULT Error forwarding message to callback.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                    TraceLoggingHResult(hr, MIDI_TRACE_EVENT_HRESULT_FIELD)
                );

                LOG_IF_FAILED(hr);
            }
        }
    }

    return S_OK;
}

HRESULT
CMidi2KSAggregateMidiInProxy::Cleanup()
{
    TraceLoggingWrite(
        MidiKSAggregateAbstractionTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_METRICS,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Cleaning up", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD),
        TraceLoggingUInt64(m_countMidiMessageSent, "Count messages forwarded")
    );

    m_callback = nullptr;

    m_device->Cleanup();
    m_device = nullptr;

    return S_OK;
}
