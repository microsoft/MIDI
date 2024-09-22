// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"

#include <chrono>

_Use_decl_annotations_
HRESULT 
MidiNetworkHost::Initialize(
    MidiNetworkUdpHostDefinition& hostDefinition
)
{
    TraceLoggingWrite(
        MidiNetworkMidiAbstractionTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    RETURN_HR_IF(E_INVALIDARG, hostDefinition.HostName.empty());
    RETURN_HR_IF(E_INVALIDARG, hostDefinition.ProductInstanceId.empty());
    RETURN_HR_IF(E_INVALIDARG, hostDefinition.ServiceInstanceName.empty());
    RETURN_HR_IF(E_INVALIDARG, hostDefinition.UmpEndpointName.empty());

    RETURN_HR_IF(E_INVALIDARG, hostDefinition.UmpEndpointName.size() > MAX_UMP_ENDPOINT_NAME_LENGTH);
    RETURN_HR_IF(E_INVALIDARG, hostDefinition.ProductInstanceId.size() > MAX_UMP_PRODUCT_INSTANCE_ID_LENGTH);

    m_hostDefinition = hostDefinition;

    m_advertiser = std::make_shared<MidiNetworkAdvertiser>();

    RETURN_HR_IF_NULL(E_POINTER, m_advertiser);

    RETURN_IF_FAILED(m_advertiser->Initialize());

    return S_OK;
}

HRESULT
MidiNetworkHost::Start()
{
    winrt::Windows::Foundation::TimeSpan timeout = std::chrono::seconds(5);

    auto status = m_socket.BindServiceNameAsync(winrt::to_hstring(m_hostDefinition.Port)).wait_for(timeout);

    if (status != winrt::Windows::Foundation::AsyncStatus::Completed)
    {
        TraceLoggingWrite(
            MidiNetworkMidiAbstractionTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_ERROR,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Timed out trying to bind service to socket.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingUInt16(m_hostDefinition.Port, "port")
        );

        RETURN_IF_FAILED(E_FAIL);
    }

    HostName hostName(m_hostDefinition.HostName);

    RETURN_IF_FAILED(m_advertiser->Advertise(
        m_hostDefinition.ServiceInstanceName,
        hostName,
        m_socket,
        m_hostDefinition.Port,
        m_hostDefinition.UmpEndpointName,
        m_hostDefinition.ProductInstanceId
    ));


    // start listening on thread.



    // when done, stop advertising, and unbind the socket


    return S_OK;
}

HRESULT 
MidiNetworkHost::ProcessIncomingPacket()
{
    // if the packet is a session packet, route to the appropriate session
    // otherwise, handle it here



    return S_OK;
}


HRESULT 
MidiNetworkHost::EstablishNewSession()
{
    TraceLoggingWrite(
        MidiNetworkMidiAbstractionTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );


    // need to add a new session to the session list


    return S_OK;
}


HRESULT 
MidiNetworkHost::Cleanup()
{
    TraceLoggingWrite(
        MidiNetworkMidiAbstractionTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );


    if (m_advertiser)
    {
        m_advertiser->Cleanup();
    }


    return S_OK;
}