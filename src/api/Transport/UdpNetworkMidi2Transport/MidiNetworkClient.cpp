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
    MidiNetworkClientDefinition& clientDefinition
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

    m_clientDefinition = clientDefinition;

    m_configIdentifier = clientDefinition.EntryIdentifier;


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

    try
    {
        auto reader = args.GetDataReader();

        uint32_t firstCommandHeaderWord{ 0 };

        auto prologue = ReadNetworkPacketPrologue(reader, firstCommandHeaderWord);

        if (prologue == MidiNetworkPacketPrologueResult::TooSmall)
        {
            TraceLoggingWrite(
                MidiNetworkMidiTransportTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_WARNING,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Undersized packet", MIDI_TRACE_EVENT_MESSAGE_FIELD)
            );

            return;
        }

        if (prologue != MidiNetworkPacketPrologueResult::Ok)
        {
            return;
        }

        auto connection = GetConnection();

        if (connection)
        {
            LOG_IF_FAILED(connection->ProcessIncomingMessage(reader, firstCommandHeaderWord));
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
    }
    CATCH_LOG();

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

    auto conn = std::make_shared<MidiNetworkClientConnection>();
    RETURN_IF_NULL_ALLOC(conn);

    DatagramSocket socket;
    socket.Control().QualityOfService(SocketQualityOfService::LowLatency);
    socket.Control().DontFragment(true);

    {
        auto lock = m_socketLock.lock();
        m_socket = socket;
    }

    // The delegate holds a weak reference, not a raw this. Revoking the token does not drain
    // handlers already dispatched, so the object has to be able to outlive the revoke.
    std::weak_ptr<MidiNetworkClient> weakThis{ weak_from_this() };
    RETURN_HR_IF_MSG(E_UNEXPECTED, weakThis.expired(), "Client must be owned by a shared_ptr before Start");

    auto messageReceivedHandler = winrt::Windows::Foundation::TypedEventHandler<DatagramSocket, DatagramSocketMessageReceivedEventArgs>(
        [weakThis](DatagramSocket const& sender, DatagramSocketMessageReceivedEventArgs const& args)
        {
            if (auto strongThis = weakThis.lock())
            {
                strongThis->OnMessageReceived(sender, args);
            }
        });

    m_messageReceivedEventToken = socket.MessageReceived(messageReceivedHandler);

    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Event handler hooked up. About to connect socket", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(remoteHostName.ToString().c_str(), "remote hostname"),
        TraceLoggingWideString(remotePort.c_str(), "remote port"));

    // establish the remote connection
    try
    {
        // this throws if the address can't be resolved or if
        // the connect otherwise fails

        auto connectOperation = socket.ConnectAsync(remoteHostName, remotePort);

        auto status = connectOperation.wait_for(
            std::chrono::milliseconds(MIDI_NETWORK_CLIENT_CONNECT_TIMEOUT_MILLISECONDS));

        if (status == winrt::Windows::Foundation::AsyncStatus::Started)
        {
            // still running, so it is not coming back in a useful timeframe
            connectOperation.Cancel();

            TraceLoggingWrite(
                MidiNetworkMidiTransportTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Timed out connecting to the remote host. Giving up on this attempt.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingWideString(remoteHostName.ToString().c_str(), "remote hostname"),
                TraceLoggingWideString(remotePort.c_str(), "remote port")
            );

            RETURN_IF_FAILED(HRESULT_FROM_WIN32(ERROR_TIMEOUT));
        }

        // rethrows the failure for the catch blocks below
        connectOperation.GetResults();
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
        m_configIdentifier,
        socket,
        remoteHostName,
        remotePort,
        m_thisEndpointName,
        m_thisProductInstanceId,
        TransportState::Current().TransportSettings.RetransmitBufferMaxCommandPacketCount,
        TransportState::Current().TransportSettings.ForwardErrorCorrectionMaxCommandPacketCount,
        m_createUmpEndpointsOnly
    ));

    TransportState::Current().AddNetworkConnection(remoteHostName, remotePort, conn);

    {
        auto lock = m_connectionLock.lock();
        m_networkConnection = conn;
    }

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
MidiNetworkClient::DisconnectByUser()
{
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    for (auto& connection : TransportState::Current().GetAllNetworkConnectionsForClient(m_clientDefinition.EntryIdentifier))
    {
        LOG_IF_FAILED(connection->SendUserTerminatedByeAndAwaitReply());
    }

    {
        auto lock = m_connectionLock.lock();

        if (m_networkConnection != nullptr)
        {
            LOG_IF_FAILED(m_networkConnection->SendUserTerminatedByeAndAwaitReply());
        }
    }

    return Shutdown();
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

    auto connections = TransportState::Current().GetAllNetworkConnectionsForClient(m_clientDefinition.EntryIdentifier);

    for (auto& connection : connections)
    {
        LOG_IF_FAILED(connection->SendShutdownBye());
    }

    for (auto& connection : connections)
    {
        LOG_IF_FAILED(connection->Shutdown());
    }

    // now remove all those connections
    RETURN_IF_FAILED(TransportState::Current().RemoveAllNetworkConnectionsForClient(m_clientDefinition.EntryIdentifier));

    // may not be in the list above if Start failed part way through. Shutdown is idempotent.
    std::shared_ptr<MidiNetworkClientConnection> connection{ nullptr };

    {
        auto lock = m_connectionLock.lock();
        connection.swap(m_networkConnection);
    }

    if (connection != nullptr)
    {
        LOG_IF_FAILED(connection->Shutdown());
    }

    winrt::Windows::Networking::Sockets::DatagramSocket socket{ nullptr };

    {
        auto lock = m_socketLock.lock();
        std::swap(socket, m_socket);
    }

    if (socket)
    {
        try
        {
            socket.MessageReceived(m_messageReceivedEventToken);
            socket.Close();
        }
        CATCH_LOG();
    }


    return S_OK;
}


