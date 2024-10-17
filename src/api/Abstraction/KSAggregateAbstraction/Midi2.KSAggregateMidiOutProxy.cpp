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
CMidi2KSAggregateMidiOutProxy::Initialize(
    LPCWSTR endpointDeviceInterfaceId,
    HANDLE filter,
    UINT pinId,
    ULONG bufferSize,
    DWORD* mmcssTaskId,
    IMidiCallback* callback,        // callback here is the actual MIDI output
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
    m_device = std::make_unique<KSMidiOutDevice>();
    RETURN_IF_NULL_ALLOC(m_device);

    auto initResult =
        m_device->Initialize(
            endpointDeviceInterfaceId,
            filter,
            pinId,
            MidiTransport::MidiTransport_StandardByteStream,
            bufferSize,
            mmcssTaskId
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

    auto transformId = __uuidof(Midi2UMP2BSTransform);

    RETURN_IF_FAILED(CoCreateInstance(transformId, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&transformPlugin)));

    RETURN_IF_FAILED(transformPlugin->Activate(__uuidof(IMidiDataTransform), (void**)(&m_ump2BSTransform)));


    TRANSFORMCREATIONPARAMS creationParams{};
    creationParams.DataFormatIn = MidiDataFormats::MidiDataFormats_UMP;
    creationParams.DataFormatOut = MidiDataFormats::MidiDataFormats_ByteStream;
    creationParams.UmpGroupIndex = groupIndex;

    // the transform sends the results directly to the specified callback
    RETURN_IF_FAILED(m_ump2BSTransform->Initialize(endpointDeviceInterfaceId, &creationParams, mmcssTaskId, m_callback, m_context, nullptr));

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidi2KSAggregateMidiOutProxy::SendMidiMessage(
    PVOID data,
    UINT length,
    LONGLONG timestamp
)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, data);
    //RETURN_HR_IF_NULL(E_POINTER, m_callback);
    RETURN_HR_IF_NULL(E_POINTER, m_ump2BSTransform);

    return m_ump2BSTransform->SendMidiMessage(data, length, timestamp);
}


HRESULT
CMidi2KSAggregateMidiOutProxy::Shutdown()
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

    if (m_ump2BSTransform)
    {
        m_ump2BSTransform->Shutdown();
        m_ump2BSTransform.reset();
    }

    if (m_device)
    {
        m_device->Shutdown();
        m_device.reset();
    }

    return S_OK;
}