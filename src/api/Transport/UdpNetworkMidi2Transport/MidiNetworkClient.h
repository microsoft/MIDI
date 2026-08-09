// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

struct MidiNetworkClientDefinition
{
    bool Created{ false };

    winrt::hstring EntryIdentifier;         // internal 
    bool Enabled{ true };

    winrt::hstring Name;                    // the name of the endpoint before we do discovery

    winrt::hstring LocalEndpointName;       // this is required by protocol
    winrt::hstring LocalProductInstanceId;  // also required by protocol


    bool CreateMidi1Ports{ MIDI_NETWORK_MIDI_CREATE_MIDI1_PORTS_DEFAULT };


    // protocol
//    MidiNetworkHostProtocol NetworkProtocol{ MidiNetworkHostProtocol::ProtocolDefault };


    // all match criteria follows

    winrt::hstring MatchId{};

    // these are direct connections. HostName or IP are required, plus the port
    winrt::hstring MatchDirectHostNameOrIPAddress{};
    winrt::hstring MatchDirectPort{};
};



class MidiNetworkClient : public std::enable_shared_from_this<MidiNetworkClient>
{
public:
    // will need some different versions of Initialize for the different ways of connecting
    HRESULT Initialize(
        _In_ MidiNetworkClientDefinition& clientDefinition
    );

    HRESULT Start(
        _In_ winrt::Windows::Networking::HostName const& remoteHostName,
        _In_ winrt::hstring const& remotePort
    );

    HRESULT Shutdown();

    // Says goodbye properly first, per spec 6.16, then shuts down. Only for a disconnect the
    // user asked for; every other path uses Shutdown() directly so nothing blocks.
    HRESULT DisconnectByUser();

    MidiNetworkClientDefinition GetDefinition() { return m_clientDefinition; }

    winrt::hstring RemoteAddress() { auto socket = GetSocket(); return socket != nullptr ? socket.Information().RemoteAddress().DisplayName() : L""; }
    winrt::hstring RemotePort() { auto socket = GetSocket(); return socket != nullptr ? socket.Information().RemotePort() : L""; }

    winrt::hstring LocalAddress() { auto socket = GetSocket(); return socket != nullptr ? socket.Information().LocalAddress().DisplayName() : L""; }
    winrt::hstring LocalPort() { auto socket = GetSocket(); return socket != nullptr ? socket.Information().LocalPort() : L""; }

    bool IsSessionActive() { auto conn = GetConnection(); return conn != nullptr ? conn->IsSessionActive() : false; }

    uint32_t GetRetransmitCount() { return m_retransmitCount; }
    uint32_t GetRetransmitRequestCount() { return m_retransmitRequestCount; }
    uint64_t GetAndResetAverageLatencyTicks()
    { 
        auto conn = GetConnection();

        if (conn != nullptr) 
        {
            return conn->GetAndResetAverageLatencyTicks();
        }
        else
        {
            return 0; 
        }
    }

    uint64_t GetTotalNetworkPacketsSent() { auto conn = GetConnection(); return conn ? conn->GetTotalNetworkPacketsSent() : 0; }
    uint64_t GetTotalNetworkPacketsReceived() { auto conn = GetConnection(); return conn ? conn->GetTotalNetworkPacketsReceived() : 0; }

    std::wstring GetEndpointDeviceId() { auto conn = GetConnection(); return conn ? conn->GetEndpointDeviceId() : L""; }

private:
    MidiNetworkClientDefinition m_clientDefinition;

    uint32_t m_retransmitCount{ 0 };
    uint32_t m_retransmitRequestCount{ 0 };

    winrt::hstring m_configIdentifier{};

    bool m_createUmpEndpointsOnly{ true };

    wil::critical_section m_connectionLock;
    std::shared_ptr<MidiNetworkConnection> m_networkConnection{ nullptr };

    std::shared_ptr<MidiNetworkConnection> GetConnection()
    {
        auto lock = m_connectionLock.lock();

        return m_networkConnection;
    }

    winrt::Windows::Networking::Sockets::DatagramSocket m_socket{ nullptr };

    // Shutdown() replaces this while receive and configuration threads are still reading it.
    wil::critical_section m_socketLock;

    winrt::Windows::Networking::Sockets::DatagramSocket GetSocket()
    {
        auto lock = m_socketLock.lock();

        return m_socket;
    }

    std::wstring m_thisEndpointName{ };
    std::wstring m_thisProductInstanceId{ };

    winrt::event_token m_messageReceivedEventToken;

    void OnMessageReceived(
        _In_ winrt::Windows::Networking::Sockets::DatagramSocket const& sender,
        _In_ winrt::Windows::Networking::Sockets::DatagramSocketMessageReceivedEventArgs const& args);


};