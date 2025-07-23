// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "stdafx.h"

using namespace winrt::Windows::Devices::Enumeration;

_Use_decl_annotations_
HRESULT
CMidiDevicePipe::Initialize(
    LPCWSTR device,
    PMIDISRV_DEVICECREATION_PARAMS creationParams,
    DWORD* mmcssTaskId
)
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(device, MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
    );


    auto deviceLock = m_DevicePipeLock.lock_exclusive();

    TRANSPORTCREATIONPARAMS transportCreationParams;

    RETURN_IF_FAILED(CMidiPipe::Initialize(device, creationParams->Flow));
    transportCreationParams.MessageOptions = MessageOptionFlags_None;
    transportCreationParams.DataFormat = creationParams->DataFormat;
    transportCreationParams.CallingComponent = MIDISRV_APIID;

    // retrieve the transport layer GUID for this peripheral
    auto additionalProperties = winrt::single_threaded_vector<winrt::hstring>();
    additionalProperties.Append(STRING_PKEY_MIDI_TransportLayer);
    additionalProperties.Append(STRING_PKEY_MIDI_SupportsMulticlient);

    auto deviceInfo = DeviceInformation::CreateFromIdAsync(
        device, 
        additionalProperties, 
        winrt::Windows::Devices::Enumeration::DeviceInformationKind::DeviceInterface).get();

    auto prop = deviceInfo.Properties().Lookup(STRING_PKEY_MIDI_TransportLayer);
    RETURN_HR_IF_NULL(E_INVALIDARG, prop);
    m_TransportGuid = winrt::unbox_value<winrt::guid>(prop);

    // Confirm that this component is either signed, or we are in developer mode.
    // Else, do not use it.
    RETURN_IF_FAILED(internal::IsComponentPermitted(m_TransportGuid));

    GUID dummySessionId{};

    if (MidiFlowBidirectional == creationParams->Flow)
    {
        TraceLoggingWrite(
            MidiSrvTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Creating bidirectional flow", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(device, MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
        );

        wil::com_ptr_nothrow<IMidiTransport> midiTransport;

        RETURN_IF_FAILED(CoCreateInstance(m_TransportGuid, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&midiTransport)));
        RETURN_IF_FAILED(midiTransport->Activate(__uuidof(IMidiBidirectional), (void**)&m_MidiBidiDevice));
        RETURN_IF_FAILED(m_MidiBidiDevice->Initialize(device, &transportCreationParams, mmcssTaskId, this, INVALID_GROUP_INDEX, dummySessionId));
    }
    else if (MidiFlowIn == creationParams->Flow)
    {
        TraceLoggingWrite(
            MidiSrvTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Creating input flow", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(device, MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
        );


        wil::com_ptr_nothrow<IMidiTransport> midiTransport;

        RETURN_IF_FAILED(CoCreateInstance(m_TransportGuid, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&midiTransport)));
        RETURN_IF_FAILED(midiTransport->Activate(__uuidof(IMidiIn), (void**)&m_MidiInDevice));
        RETURN_IF_FAILED(m_MidiInDevice->Initialize(device, &transportCreationParams, mmcssTaskId, this, INVALID_GROUP_INDEX, dummySessionId));
    }
    else if (MidiFlowOut == creationParams->Flow)
    {
        TraceLoggingWrite(
            MidiSrvTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Creating output flow", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(device, MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
        );

        wil::com_ptr_nothrow<IMidiTransport> midiTransport;

        RETURN_IF_FAILED(CoCreateInstance(m_TransportGuid, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&midiTransport)));
        RETURN_IF_FAILED(midiTransport->Activate(__uuidof(IMidiOut), (void**)&m_MidiOutDevice));
        RETURN_IF_FAILED(m_MidiOutDevice->Initialize(device, &transportCreationParams, mmcssTaskId, dummySessionId));
    }
    else
    {
        RETURN_IF_FAILED(E_INVALIDARG);
    }

    // save the format that was actually used for creation.
    // For the device pipe, the format in and out are always the
    // same.
    if (IsFlowSupported(MidiFlowIn))
    {
        RETURN_IF_FAILED(SetDataFormatIn(transportCreationParams.DataFormat));
    }

    if (IsFlowSupported(MidiFlowOut))
    {
        RETURN_IF_FAILED(SetDataFormatOut(transportCreationParams.DataFormat));
    }

    // Check to see if the device supports multi-client. This value is used
    // when trying to add a new client pipe

    try
    {
        // we don't watch this property for updates. It's reasonable for a change to this
        // property to require disconnecting any clients and reconnecting to pick them up. 
        // It's necessary, even

        // default to true for multiclient support
        m_endpointSupportsMulticlient = true;

        if (deviceInfo.Properties().HasKey(STRING_PKEY_MIDI_SupportsMulticlient))
        {
            auto propMultiClient = deviceInfo.Properties().Lookup(STRING_PKEY_MIDI_SupportsMulticlient);

            if (propMultiClient != nullptr)
            {
                m_endpointSupportsMulticlient = winrt::unbox_value<bool>(propMultiClient);
            }
        }

    }
    CATCH_LOG();

    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Exit Success", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(device, MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
    );

    return S_OK;
}

HRESULT
CMidiDevicePipe::Shutdown()
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    {
        auto lock = m_DevicePipeLock.lock_exclusive();

        if (m_MidiBidiDevice)
        {
            LOG_IF_FAILED(m_MidiBidiDevice->Shutdown());
        }
        if (m_MidiInDevice)
        {
            LOG_IF_FAILED(m_MidiInDevice->Shutdown());
        }
        if (m_MidiOutDevice)
        {
            LOG_IF_FAILED(m_MidiOutDevice->Shutdown());
        }

        m_MidiBidiDevice.reset();
        m_MidiInDevice.reset();
        m_MidiOutDevice.reset();
    }

    RETURN_IF_FAILED(CMidiPipe::Shutdown());

    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Exit success", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidiDevicePipe::SendMidiMessage(
    MessageOptionFlags optionFlags,
    PVOID data,
    UINT length,
    LONGLONG timestamp
)
{
    // TODO: What happens with outgoing JR clock/timestamp messages? They can't take a different path
    // from the message they are attached to. If they follow the same path, aren't excepted, and have
    // the right timestamp order, they should be ok, but that's a "should" not a "will".
    // 
    // In theory, JR-timestamped messages can't be scheduled. They have to just go right through.
    // But there are real issues with that, and with keeping the JR timestamp and its attached
    // message as an atomic unit. There are too many ways they could get interleaved in a busy
    // system with multiple clients.
    // 
    // Maybe the DevicePipe is what is responsible for adding JR Timestamps as last step, and 
    // stripping them out as first step on incoming messages. We can then use that information
    // for jitter calculation. The DevicePipe is the single connection to the device, so
    // we can ensure order there before it goes to the transport.

    // multiple clients may send at the same time, contention is handled in the transport layer
    auto lock = m_DevicePipeLock.lock_shared();

    if (m_MidiBidiDevice)
    {
        auto hr = m_MidiBidiDevice->SendMidiMessage(optionFlags, data, length, timestamp);
        RETURN_IF_FAILED(hr);

        return S_OK;
    }
    else if (m_MidiOutDevice)
    {
        auto hr = m_MidiOutDevice->SendMidiMessage(optionFlags, data, length, timestamp);
        RETURN_IF_FAILED(hr);

        return S_OK;
    }

    return E_ABORT;
}

