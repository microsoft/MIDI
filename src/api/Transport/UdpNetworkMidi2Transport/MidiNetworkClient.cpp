// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"


_Use_decl_annotations_
HRESULT 
MidiNetworkClient::Initialize(
    MidiNetworkClientDefinition& clientDefinition, 
    enumeration::DeviceInformation advertisedHost
)
{
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    UNREFERENCED_PARAMETER(advertisedHost);

    m_createUmpEndpointsOnly = !clientDefinition.CreateMidi1Ports;

    m_thisEndpointName = clientDefinition.LocalEndpointName;
    m_thisProductInstanceId = clientDefinition.LocalProductInstanceId;

    return S_OK;
}


_Use_decl_annotations_
void MidiNetworkClient::OnMessageReceived(
    _In_ winrt::Windows::Networking::Sockets::DatagramSocket const& sender,
    _In_ winrt::Windows::Networking::Sockets::DatagramSocketMessageReceivedEventArgs const& args)
{
    UNREFERENCED_PARAMETER(sender);

    auto reader = args.GetDataReader();

    if (reader != nullptr && reader.UnconsumedBufferLength() < MINIMUM_VALID_UDP_PACKET_SIZE)
    {
        // not a message we understand. Needs to be at least the size of the 
        // MIDI header plus a command packet header. Really it needs to be larger, but
        // just trying to weed out blips

        TraceLoggingWrite(
            MidiNetworkMidiTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_WARNING,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Undersized packet", MIDI_TRACE_EVENT_MESSAGE_FIELD)
        );

        // todo: does the reader need to be cleared?

        return;
    }

    uint32_t udpHeader = reader.ReadUInt32();

    if (udpHeader != MIDI_UDP_PAYLOAD_HEADER)
    {
        // not a message we understand

        return;
    }


    if (m_networkConnection)
    {
        m_networkConnection->ProcessIncomingMessage(reader);
    }
    else
    {
        TraceLoggingWrite(
            MidiNetworkMidiTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_ERROR,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Message received from remote client, connection is nullptr", MIDI_TRACE_EVENT_MESSAGE_FIELD)
        );
    }


    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Exit", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

}


_Use_decl_annotations_
HRESULT
MidiNetworkClient::Start(
    HostName const& remoteHostName, 
    winrt::hstring const& remotePort
)
{
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(remoteHostName.ToString().c_str(), "remote hostname"),
        TraceLoggingWideString(remotePort.c_str(), "remote port"));

    auto conn = std::make_shared<MidiNetworkConnection>();
    RETURN_IF_NULL_ALLOC(conn);

    DatagramSocket socket;
    m_socket = std::move(socket);

    m_socket.Control().QualityOfService(SocketQualityOfService::LowLatency);
    m_socket.Control().DontFragment(true);

    auto messageReceivedHandler = winrt::Windows::Foundation::TypedEventHandler<DatagramSocket, DatagramSocketMessageReceivedEventArgs>(this, &MidiNetworkClient::OnMessageReceived);
    m_messageReceivedEventToken = m_socket.MessageReceived(messageReceivedHandler);

    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Event handler hooked up. About to connect socket", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(remoteHostName.ToString().c_str(), "remote hostname"),
        TraceLoggingWideString(remotePort.c_str(), "remote port"));


    //EndpointPair pair(
    //    nullptr, 
    //    L"",
    //    remoteHostName, 
    //    remotePort);

    // establish the remote connection
    try
    {
        // this throws if the address can't be resolved or if 
        // the connect otherwise fails

        //m_socket.ConnectAsync(pair).get();
        m_socket.ConnectAsync(remoteHostName, remotePort);
    }
    catch (winrt::hresult_error err)
    {
        TraceLoggingWrite(
            MidiNetworkMidiTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_ERROR,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"HRESULT Exception connecting to socket", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(remoteHostName.ToString().c_str(), "remote hostname"),
            TraceLoggingWideString(remotePort.c_str(), "remote port"),
            TraceLoggingHResult(err.code(), "hresult"),
            TraceLoggingWideString(err.message().c_str(), "error message")
            );

        RETURN_IF_FAILED(E_NOTFOUND);
    }
    catch (...)
    {
        TraceLoggingWrite(
            MidiNetworkMidiTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_ERROR,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Exception connecting to socket", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(remoteHostName.ToString().c_str(), "remote hostname"),
            TraceLoggingWideString(remotePort.c_str(), "remote port"));

        RETURN_IF_FAILED(E_NOTFOUND);
    }
   

    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"About to initialize connection", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(remoteHostName.ToString().c_str(), "remote hostname"),
        TraceLoggingWideString(remotePort.c_str(), "remote port"));

    RETURN_IF_FAILED(conn->Initialize(
        MidiNetworkConnectionRole::ConnectionWindowsIsClient,
        m_socket,
        remoteHostName,
        remotePort,
        m_thisEndpointName,
        m_thisProductInstanceId,
        TransportState::Current().TransportSettings.RetransmitBufferMaxCommandPacketCount,
        TransportState::Current().TransportSettings.ForwardErrorCorrectionMaxCommandPacketCount,
        m_createUmpEndpointsOnly
    ));

    TransportState::Current().AddNetworkConnection(remoteHostName, remotePort, conn);

    m_networkConnection = conn;

    // try to establish connection in-protocol

    // TODO: Need to wire up other security approaches here
    // TODO: The invitation send should be in a loop so it's repeated if
    //       there's no response
    RETURN_IF_FAILED(conn->SendInvitation());

    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Invitation sent", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(remoteHostName.ToString().c_str(), "remote hostname"),
        TraceLoggingWideString(remotePort.c_str(), "remote port"));


    // todo: associate connection with the endpoint id


    // todo: initiate discovery


    return S_OK;
}





HRESULT 
MidiNetworkClient::Shutdown()
{
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );


    if (m_socket)
    {
        m_socket.MessageReceived(m_messageReceivedEventToken);
        m_socket.Close();
        m_socket = nullptr;
    }


    return S_OK;
}


