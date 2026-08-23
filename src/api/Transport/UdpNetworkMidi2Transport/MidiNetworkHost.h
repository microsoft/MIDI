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
    MidiNetworkEntryState State{ MidiNetworkEntryState::Pending };

    winrt::guid EntryIdentifier;            // internal

    bool UseAutomaticPortAllocation{ true };
    winrt::hstring Port;

    winrt::hstring UmpEndpointName;
    winrt::hstring ProductInstanceId;

    // What the user chose to call endpoints created for remote clients reaching this host.
    // Empty means use the name each remote client announces.
    winrt::hstring CustomEndpointName;

    //bool UmpOnly{ true };
    bool IsEnabled{ true };
    bool Advertise{ true };

    bool CreateMidi1Ports{ MIDI_NETWORK_MIDI_CREATE_MIDI1_PORTS_DEFAULT };

    // connection rules
    MidiNetworkRemoteClientPolicy RemoteClientPolicy{ MidiNetworkRemoteClientPolicy::PolicyAllowAny };

    // Identity keys, as produced by MidiNetworkRemoteClientIdentity::Key(). Populated from the
    // configuration and added to at runtime when a user approves or denies a client.
    std::vector<std::wstring> AllowedClientKeys{};
    std::vector<std::wstring> DeniedClientKeys{};

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

    // What should happen to an invitation from this client, given the policy and the lists.
    MidiNetworkRemoteClientDecision EvaluateRemoteClient(_In_ MidiNetworkRemoteClientIdentity const& identity);

    MidiNetworkRemoteClientPolicy GetRemoteClientPolicy() const { return m_hostDefinition.RemoteClientPolicy; }

    // A user decision arriving through the configuration manager. Persisting it is the caller's
    // job; these take effect immediately either way.
    HRESULT AddRemoteClientToAllowList(_In_ MidiNetworkRemoteClientIdentity const& identity);
    HRESULT AddRemoteClientToDenyList(_In_ MidiNetworkRemoteClientIdentity const& identity);

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

    MidiNetworkReplyRateLimiter m_refusalRateLimiter;

    // Guards the allow and deny lists, which a user can change at any time through the
    // configuration manager while the receive path is reading them.
    wil::critical_section m_remoteClientListsLock;

};
