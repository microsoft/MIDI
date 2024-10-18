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


    auto deviceLock = m_DevicePipeLock.lock();

    TRANSPORTCREATIONPARAMS abstractionCreationParams;

    RETURN_IF_FAILED(CMidiPipe::Initialize(device, creationParams->Flow));

    abstractionCreationParams.DataFormat = creationParams->DataFormat;

    // retrieve the abstraction layer GUID for this peripheral
    auto additionalProperties = winrt::single_threaded_vector<winrt::hstring>();
    additionalProperties.Append(winrt::to_hstring(STRING_PKEY_MIDI_AbstractionLayer));
    additionalProperties.Append(winrt::to_hstring(STRING_PKEY_MIDI_SupportsMulticlient));

    auto deviceInfo = DeviceInformation::CreateFromIdAsync(
        device, 
        additionalProperties, 
        winrt::Windows::Devices::Enumeration::DeviceInformationKind::DeviceInterface).get();

    auto prop = deviceInfo.Properties().Lookup(winrt::to_hstring(STRING_PKEY_MIDI_AbstractionLayer));
    RETURN_HR_IF_NULL(E_INVALIDARG, prop);
    m_AbstractionGuid = winrt::unbox_value<winrt::guid>(prop);

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

        wil::com_ptr_nothrow<IMidiTransport> midiAbstraction;

        RETURN_IF_FAILED(CoCreateInstance(m_AbstractionGuid, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&midiAbstraction)));
        RETURN_IF_FAILED(midiAbstraction->Activate(__uuidof(IMidiBiDi), (void**)&m_MidiBiDiDevice));
        RETURN_IF_FAILED(m_MidiBiDiDevice->Initialize(device, &abstractionCreationParams, mmcssTaskId, this, INVALID_GROUP_INDEX, dummySessionId));
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


        wil::com_ptr_nothrow<IMidiTransport> midiAbstraction;

        RETURN_IF_FAILED(CoCreateInstance(m_AbstractionGuid, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&midiAbstraction)));
        RETURN_IF_FAILED(midiAbstraction->Activate(__uuidof(IMidiIn), (void**)&m_MidiInDevice));
        RETURN_IF_FAILED(m_MidiInDevice->Initialize(device, &abstractionCreationParams, mmcssTaskId, this, INVALID_GROUP_INDEX, dummySessionId));
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

        wil::com_ptr_nothrow<IMidiTransport> midiAbstraction;

        RETURN_IF_FAILED(CoCreateInstance(m_AbstractionGuid, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&midiAbstraction)));
        RETURN_IF_FAILED(midiAbstraction->Activate(__uuidof(IMidiOut), (void**)&m_MidiOutDevice));
        RETURN_IF_FAILED(m_MidiOutDevice->Initialize(device, &abstractionCreationParams, mmcssTaskId, dummySessionId));
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
        RETURN_IF_FAILED(SetDataFormatIn(abstractionCreationParams.DataFormat));
    }

    if (IsFlowSupported(MidiFlowOut))
    {
        RETURN_IF_FAILED(SetDataFormatOut(abstractionCreationParams.DataFormat));
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

        if (deviceInfo.Properties().HasKey(winrt::to_hstring(STRING_PKEY_MIDI_SupportsMulticlient)))
        {
            auto propMultiClient = deviceInfo.Properties().Lookup(winrt::to_hstring(STRING_PKEY_MIDI_SupportsMulticlient));

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
        TraceLoggingPointer(this, "this")
    );

    {
        auto lock = m_DevicePipeLock.lock();

        if (m_MidiBiDiDevice)
        {
            LOG_IF_FAILED(m_MidiBiDiDevice->Shutdown());
        }
        if (m_MidiInDevice)
        {
            LOG_IF_FAILED(m_MidiInDevice->Shutdown());
        }
        if (m_MidiOutDevice)
        {
            LOG_IF_FAILED(m_MidiOutDevice->Shutdown());
        }

        m_MidiBiDiDevice.reset();
        m_MidiInDevice.reset();
        m_MidiOutDevice.reset();
    }

    RETURN_IF_FAILED(CMidiPipe::Shutdown());

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidiDevicePipe::SendMidiMessage(
    PVOID data,
    UINT length,
    LONGLONG timestamp
)
{
    // TODO: 
    // Figure out how to most efficiently handle real-time messages
    // - Should we allow them to be scheduled?
    // - Should they bypass plugins?
    // Run message through plugins
    // - The plugins may produce additional messages, or delete the message
    // - If device is currently in the middle of SysEx7, we will want to park messages that aren't allowed to interrupt that
    // - After plugin processing, it goes to the scheduler

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

    auto hr = SendMidiMessageNow(data, length, timestamp);
    LOG_IF_FAILED(hr);

    return hr;
}

_Use_decl_annotations_
HRESULT
CMidiDevicePipe::SendMidiMessageNow(
    PVOID data,
    UINT length,
    LONGLONG timestamp
)
{
    // TODO: This function is where we'll check to see if we're in SysEx or not. If we are
    // then we need to add the message to the SysEx set-aside scheduler, or maybe just add
    // it to the regular scheduler queue?

    // only one client may send a message to the device at a time
    auto lock = m_DevicePipeLock.lock();

    if (m_MidiBiDiDevice)
    {
        auto hr = m_MidiBiDiDevice->SendMidiMessage(data, length, timestamp);
        RETURN_IF_FAILED(hr);

        return S_OK;
    }
    else if (m_MidiOutDevice)
    {
        auto hr = m_MidiOutDevice->SendMidiMessage(data, length, timestamp);
        RETURN_IF_FAILED(hr);

        return S_OK;
    }

    return E_ABORT;
}

