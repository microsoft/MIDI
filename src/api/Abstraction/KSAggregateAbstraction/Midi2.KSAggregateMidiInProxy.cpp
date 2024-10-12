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
    LPCWSTR endpointDeviceInterfaceId,
    HANDLE filter,
    UINT pinId,
    ULONG bufferSize,
    DWORD* mmcssTaskId,
    IMidiCallback* callback,
    LONGLONG context,
    BYTE groupIndex
)
{
    TraceLoggingWrite(
        MidiKSAggregateAbstractionTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(endpointDeviceInterfaceId, MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD),
        TraceLoggingUInt32(pinId, "Pin id"),
        TraceLoggingUInt8(groupIndex, "Group index")
        );

    m_callback = callback;
    m_context = context;
    m_groupIndex = groupIndex;

    m_endpointDeviceId = internal::NormalizeEndpointInterfaceIdWStringCopy(endpointDeviceInterfaceId);

    m_countMidiMessageSent = 0;

    // create the KS device and add to our runtime map
    //m_device.reset(new (std::nothrow) KSMidiInDevice());
    m_device = std::make_unique<KSMidiInDevice>();
    RETURN_IF_NULL_ALLOC(m_device);

    auto initResult =
        m_device->Initialize(
            endpointDeviceInterfaceId,
            filter,
            pinId,
            MidiTransport::MidiTransport_StandardByteStream,
            bufferSize,
            mmcssTaskId,
            (IMidiCallback* )this,
            context
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
            TraceLoggingWideString(endpointDeviceInterfaceId, MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD),
            TraceLoggingUInt32(pinId, "pin id")
        );

        m_device = nullptr;
        RETURN_IF_FAILED(initResult);
    }


    wil::com_ptr_nothrow<IMidiTransform> transformPlugin;

    auto transformId = __uuidof(Midi2BS2UMPTransform);

    RETURN_IF_FAILED(CoCreateInstance(transformId, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&transformPlugin)));

    RETURN_IF_FAILED(transformPlugin->Activate(__uuidof(IMidiDataTransform), (void**)(&m_bs2UmpTransform)));

    TRANSFORMCREATIONPARAMS creationParams{};
    creationParams.DataFormatIn = MidiDataFormats::MidiDataFormats_ByteStream;
    creationParams.DataFormatOut = MidiDataFormats::MidiDataFormats_UMP;
    creationParams.UmpGroupIndex = groupIndex;

    RETURN_IF_FAILED(m_bs2UmpTransform->Initialize(endpointDeviceInterfaceId, &creationParams, mmcssTaskId, m_callback, 0, nullptr));

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidi2KSAggregateMidiInProxy::Callback(
    PVOID data, 
    UINT length,
    LONGLONG timestamp,
    LONGLONG /* context */
)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, data);
    RETURN_HR_IF_NULL(E_POINTER, m_callback);
    RETURN_HR_IF_NULL(E_POINTER, m_bs2UmpTransform);

    //TraceLoggingWrite(
    //    MidiKSAggregateAbstractionTelemetryProvider::Provider(),
    //    MIDI_TRACE_EVENT_VERBOSE,
    //    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
    //    TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
    //    TraceLoggingPointer(this, "this"),
    //    TraceLoggingWideString(L"Callback received from device. Sending data to next callback.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
    //    TraceLoggingUInt32(length, "data length"),
    //    TraceLoggingUInt64(timestamp, MIDI_TRACE_EVENT_MESSAGE_TIMESTAMP_FIELD)
    //);

    // the callback for the transform is wired up in initialize
    return m_bs2UmpTransform->SendMidiMessage(data, length, timestamp);
}

HRESULT
CMidi2KSAggregateMidiInProxy::Shutdown()
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

    m_bs2UmpTransform->Shutdown();
    m_bs2UmpTransform.reset();

    m_device->Shutdown();
    m_device.reset();

    return S_OK;
}
