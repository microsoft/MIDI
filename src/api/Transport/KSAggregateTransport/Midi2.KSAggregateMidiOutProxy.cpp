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
    LONGLONG context,
    BYTE groupIndex
)
{
    TraceLoggingWrite(
        MidiKSAggregateTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(endpointDeviceInterfaceId, MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD),
        TraceLoggingUInt32(pinId, "Pin id"),
        TraceLoggingUInt8(groupIndex, "Group index")
    );

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
            MidiKSAggregateTransportTelemetryProvider::Provider(),
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

    // Confirm that this component is either signed, or we are in developer mode.
    // Else, do not use it.
    RETURN_IF_FAILED(internal::IsComponentPermitted(transformId));

    RETURN_IF_FAILED(CoCreateInstance(transformId, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&transformPlugin)));

    RETURN_IF_FAILED(transformPlugin->Activate(__uuidof(IMidiDataTransform), (void**)(&m_ump2BSTransform)));


    TRANSFORMCREATIONPARAMS creationParams{};
    creationParams.DataFormatIn = MidiDataFormats::MidiDataFormats_UMP;
    creationParams.DataFormatOut = MidiDataFormats::MidiDataFormats_ByteStream;
    creationParams.UmpGroupIndex = groupIndex;

    // the transform sends the results back here through the IMidiCallback interface so we can then send to the associated KS device
    RETURN_IF_FAILED(m_ump2BSTransform->Initialize(endpointDeviceInterfaceId, &creationParams, mmcssTaskId, this, m_context, nullptr));

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidi2KSAggregateMidiOutProxy::SendMidiMessage(
    MessageOptionFlags optionFlags,
    PVOID data,
    UINT length,
    LONGLONG timestamp
)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, data);
    RETURN_HR_IF_NULL(E_POINTER, m_ump2BSTransform);

#ifdef _DEBUG
    TraceLoggingWrite(
        MidiKSAggregateTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_VERBOSE,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Sending MIDI Message to UMP2BS translator", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingUInt32(static_cast<uint32_t>(optionFlags), "optionFlags"),
        TraceLoggingHexUInt32Array(static_cast<uint32_t*>(data), static_cast<uint16_t>(length/sizeof(uint32_t)), "data"),
        TraceLoggingUInt32(static_cast<uint32_t>(length), "length bytes"),
        TraceLoggingUInt64(static_cast<uint64_t>(timestamp), MIDI_TRACE_EVENT_MESSAGE_TIMESTAMP_FIELD)
    );
#else
    //TraceLoggingWrite(
    //    MidiKSAggregateTransportTelemetryProvider::Provider(),
    //    MIDI_TRACE_EVENT_VERBOSE,
    //    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
    //    TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
    //    TraceLoggingPointer(this, "this"),
    //    TraceLoggingWideString(L"Sending MIDI Message to UMP2BS translator", MIDI_TRACE_EVENT_MESSAGE_FIELD),
    //    TraceLoggingUInt32(static_cast<uint32_t>(optionFlags), "optionFlags"),
    //    TraceLoggingPointer(data, "data pointer"),
    //    TraceLoggingUInt32(static_cast<uint32_t>(length), "length bytes"),
    //    TraceLoggingUInt64(static_cast<uint64_t>(timestamp), MIDI_TRACE_EVENT_MESSAGE_TIMESTAMP_FIELD)
    //);
#endif

    // get the message translated. Comes back via the callback
    RETURN_IF_FAILED(m_ump2BSTransform->SendMidiMessage(optionFlags, data, length, timestamp));

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidi2KSAggregateMidiOutProxy::Callback(
    MessageOptionFlags optionFlags,
    PVOID data,
    UINT length,
    LONGLONG timestamp,
    LONGLONG /* context */
)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, data);
    RETURN_HR_IF_NULL(E_POINTER, m_device);

#ifdef _DEBUG
    TraceLoggingWrite(
        MidiKSAggregateTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_VERBOSE,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"MidiOut callback received from translation. Sending data to device.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingUInt32(static_cast<uint32_t>(optionFlags), "optionFlags"),
        TraceLoggingHexUInt32Array(static_cast<uint32_t*>(data), static_cast<uint16_t>(length / sizeof(uint32_t)), "data"),
        TraceLoggingUInt32(static_cast<uint32_t>(length), "length bytes"),
        TraceLoggingUInt64(static_cast<uint64_t>(timestamp), MIDI_TRACE_EVENT_MESSAGE_TIMESTAMP_FIELD)
    );
#else
    //TraceLoggingWrite(
    //    MidiKSAggregateTransportTelemetryProvider::Provider(),
    //    MIDI_TRACE_EVENT_VERBOSE,
    //    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
    //    TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
    //    TraceLoggingPointer(this, "this"),
    //    TraceLoggingWideString(L"MidiOut callback received from translation. Sending data to device.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
    //    TraceLoggingUInt32(static_cast<uint32_t>(optionFlags), "optionFlags"),
    //    TraceLoggingPointer(data, "data pointer"),
    //    TraceLoggingUInt32(static_cast<uint32_t>(length), "length bytes"),
    //    TraceLoggingUInt64(static_cast<uint64_t>(timestamp), MIDI_TRACE_EVENT_MESSAGE_TIMESTAMP_FIELD)
    //);
#endif

    // Send transformed message to device
    RETURN_IF_FAILED(m_device->SendMidiMessage(optionFlags, data, length, timestamp));

    return S_OK;
}




HRESULT
CMidi2KSAggregateMidiOutProxy::Shutdown()
{
    TraceLoggingWrite(
        MidiKSAggregateTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_METRICS,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Cleaning up", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD),
        TraceLoggingUInt64(m_countMidiMessageSent, "Count messages forwarded")
    );

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