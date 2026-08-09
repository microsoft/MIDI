// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

using namespace winrt::Windows::Networking;
using namespace winrt::Windows::Networking::Sockets;
using namespace winrt::Windows::Networking::ServiceDiscovery::Dnssd;

#include <queue>

enum MidiNetworkHostConnectionPolicy
{
    PolicyAllowAllConnections = 0,
    PolicyAllowFromIpList,
    PolicyAllowFromIpRange,
};

enum MidiNetworkHostAuthentication
{
    NoAuthentication = 0,
    PasswordAuthentication,
    UserAuthentication,
};

enum MidiNetworkHostProtocol
{
    ProtocolDefault = 0,
    ProtocolUdp,
};

struct MidiNetworkHostDefinition
{
    bool Created{ false };

    winrt::hstring EntryIdentifier;         // internal 

    bool UseAutomaticPortAllocation{ true };
    winrt::hstring Port;

    winrt::hstring UmpEndpointName;
    winrt::hstring ProductInstanceId;

    //bool UmpOnly{ true };
    bool IsEnabled{ true };
    bool Advertise{ true };

    bool CreateMidi1Ports{ MIDI_NETWORK_MIDI_CREATE_MIDI1_PORTS_DEFAULT };

    // connection rules
    MidiNetworkHostConnectionPolicy ConnectionPolicy{ MidiNetworkHostConnectionPolicy::PolicyAllowAllConnections };

    // For PolicyAllowFromIpList these are the permitted addresses. For PolicyAllowFromIpRange
    // there are exactly two, being the inclusive start and end of the range.
    std::vector<winrt::hstring> ConnectionPolicyAddresses{};

    // protocol
    MidiNetworkHostProtocol NetworkProtocol{ MidiNetworkHostProtocol::ProtocolDefault };

    // authentication
    MidiNetworkHostAuthentication Authentication{ MidiNetworkHostAuthentication::NoAuthentication };

    // Names the secret in whatever store we settle on. Never the secret itself, and never
    // logged as anything other than an opaque identifier. See MidiNetworkCredentials.h.
    winrt::hstring AuthenticationCredentialIdentifier;

    // auth lookup key


    // generated properties
    winrt::hstring ServiceInstanceName;     // instance name for the PTR record
    winrt::hstring HostName;                // must include the .local domain

};



class MidiNetworkHost : public std::enable_shared_from_this<MidiNetworkHost>
{
public:
    HRESULT Initialize(_In_ MidiNetworkHostDefinition& hostDefinition);
    
    HRESULT Start();
    HRESULT Stop();

    HRESULT Shutdown();

    bool HasStarted() { return m_started; }

    bool IsEnabled() { return m_enabled; }

    MidiNetworkHostDefinition GetDefinition() { return m_hostDefinition; }

    winrt::hstring ActualPort() { auto socket = GetSocket(); return socket != nullptr ? socket.Information().LocalPort() : L""; }
    winrt::hstring ActualAddress() { auto socket = GetSocket(); return socket != nullptr ? socket.Information().LocalAddress().DisplayName() : L""; }

private:
//    winrt::hstring m_configIdentifier{};

    bool m_enabled{ true };
    std::atomic<bool> m_started{ false };
    bool m_createUmpEndpointsOnly{ true };

    std::wstring m_hostEndpointName{ };
    std::wstring m_hostProductInstanceId{ };

    std::wstring m_parentDeviceInstanceId{ };

    winrt::event_token m_messageReceivedEventToken;

    void OnMessageReceived(
        _In_ DatagramSocket const& sender,
        _In_ DatagramSocketMessageReceivedEventArgs const& args);

    MidiNetworkHostDefinition m_hostDefinition{};

    std::shared_ptr<MidiNetworkAdvertiser> m_advertiser{ nullptr };


    DatagramSocket m_socket{ nullptr };

    // Stop() replaces this while receive and configuration threads are still reading it.
    wil::critical_section m_socketLock;

    DatagramSocket GetSocket()
    {
        auto lock = m_socketLock.lock();

        return m_socket;
    }

    HRESULT CreateNetworkConnection(
        _In_ winrt::Windows::Networking::HostName const& remoteHostName,
        _In_ winrt::hstring const& remotePort,
        _Out_ std::shared_ptr<MidiNetworkConnection>& connection);

    // Spec 6.4: the first command from a client which has no session must be an invitation.
    static bool IsSessionOpeningCommand(_In_ uint8_t const commandCode);

    // Commands which the spec says warrant a Bye when no session exists.
    static bool WarrantsSessionNotEstablishedBye(_In_ uint8_t const commandCode);

    // Replies to a remote we hold no connection for, so nothing is allocated on its behalf.
    HRESULT SendUnconnectedBye(
        _In_ winrt::Windows::Networking::HostName const& remoteHostName,
        _In_ winrt::hstring const& remotePort,
        _In_ MidiNetworkCommandByeReason const reason,
        _In_ std::wstring const& message);

    bool IsRemoteAllowedByConnectionPolicy(_In_ winrt::Windows::Networking::HostName const& remoteHostName);

    MidiNetworkReplyRateLimiter m_refusalRateLimiter;

};
