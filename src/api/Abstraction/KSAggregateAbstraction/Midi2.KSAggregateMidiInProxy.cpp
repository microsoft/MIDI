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


    wil::com_ptr_nothrow<IMidiTransform> transformLib;
    wil::com_ptr_nothrow<IMidiDataTransform> transform;

    auto iid = __uuidof(Midi2BS2UMPTransform);

    RETURN_IF_FAILED(CoCreateInstance(iid, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&transformLib)));

    RETURN_IF_FAILED(transformLib->Activate(__uuidof(IMidiDataTransform), (void**)(&m_bs2UmpTransform)));

    TRANSFORMCREATIONPARAMS creationParams{};
    creationParams.DataFormatIn = MidiDataFormat::MidiDataFormat_ByteStream;
    creationParams.DataFormatOut = MidiDataFormat::MidiDataFormat_UMP;
    creationParams.InstanceConfigurationJsonData = nullptr;
    creationParams.UmpGroupIndex = GroupIndex;

    DWORD mmcssTaskId{};

    RETURN_IF_FAILED(transform->Initialize(Device, &creationParams, &mmcssTaskId, m_callback, 0, nullptr));



    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidi2KSAggregateMidiInProxy::Callback(
    PVOID Data, 
    UINT Length,
    LONGLONG Timestamp,
    LONGLONG /* Context */
)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, Data);
    RETURN_HR_IF_NULL(E_POINTER, m_callback);
    RETURN_HR_IF_NULL(E_POINTER, m_bs2UmpTransform);

    //TraceLoggingWrite(
    //    MidiKSAggregateAbstractionTelemetryProvider::Provider(),
    //    MIDI_TRACE_EVENT_VERBOSE,
    //    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
    //    TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
    //    TraceLoggingPointer(this, "this"),
    //    TraceLoggingWideString(L"Callback received from device. Sending data to next callback.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
    //    TraceLoggingUInt32(Length, "data length"),
    //    TraceLoggingUInt64(Timestamp, MIDI_TRACE_EVENT_MESSAGE_TIMESTAMP_FIELD)
    //);

    // the callback for the transform is wired up in initialize
    return m_bs2UmpTransform->SendMidiMessage(Data, Length, Timestamp);
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

    m_bs2UmpTransform->Cleanup();
    m_bs2UmpTransform.reset();

    m_device->Cleanup();
    m_device.reset();

    return S_OK;
}
