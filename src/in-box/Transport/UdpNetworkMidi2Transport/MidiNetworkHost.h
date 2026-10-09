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

    // Only consulted when a specific port was configured.
    bool AllowPortFallback{ true };

    // The adapter the host is limited to, or a null GUID for every adapter. Found by its id, then
    // by its hardware address. The name is only shown while the adapter is missing.
    winrt::guid NetworkAdapterId{};
    winrt::hstring NetworkAdapterName;
    winrt::hstring NetworkAdapterPhysicalAddress;

    // When the adapter is missing: run on every adapter until it comes back, instead of waiting
    bool AllowNetworkAdapterFallback{ true };

    bool IsLimitedToNetworkAdapter() const noexcept
    {
        return NetworkAdapterId != winrt::guid{} || !NetworkAdapterPhysicalAddress.empty();
    }

    winrt::hstring UmpEndpointName;
    winrt::hstring ProductInstanceId;

    // What the user chose to call endpoints created for remote clients reaching this host.
    // Empty means use the name each remote client announces.
    winrt::hstring CustomEndpointName;

    //bool UmpOnly{ true };
    bool IsEnabled{ true };
    bool Advertise{ true };

    bool CreateMidi1Ports{ MIDI_NETWORK_MIDI_CREATE_MIDI1_PORTS_DEFAULT };

    // Only used when the remote client declares no function blocks. See the constant for why.
    uint8_t FallbackMidi1PortCount{ MIDI_NETWORK_MIDI_FALLBACK_MIDI1_PORT_COUNT_DEFAULT };

    // What this host sends to each remote client, as a multiple of MIDI 1.0 wire speed. 0 is no
    // limit. See MIDI_CONFIG_JSON_NETWORK_MIDI_SEND_SPEED_LIMIT_KEY.
    uint32_t SendSpeedLimit{ 0 };
    bool ReduceSendSpeedAutomatically{ false };

    // Remote clients with their own settings, used for them instead of the two above
    std::vector<MidiNetworkRemoteClientSettings> RemoteClientSettings{};

    // The entry for this remote client, or nullptr when it has none
    MidiNetworkRemoteClientSettings const* FindRemoteClientSettings(_In_ MidiNetworkRemoteClientIdentity const& identity) const
    {
        if (!identity.IsValid()) return nullptr;

        auto const key = identity.Key();

        for (auto const& settings : RemoteClientSettings)
        {
            if (settings.Identity.Key() == key) return &settings;
        }

        return nullptr;
    }

    // What this host sends to one remote client. One not identified yet gets the host's.
    MidiNetworkRemoteClientSendSpeed SendSpeedFor(_In_ MidiNetworkRemoteClientIdentity const& identity) const
    {
        if (auto const settings = FindRemoteClientSettings(identity); settings != nullptr)
        {
            return { settings->SendSpeedLimit, settings->ReduceSendSpeedAutomatically, true };
        }

        return { SendSpeedLimit, ReduceSendSpeedAutomatically, false };
    }

    // connection rules
    MidiNetworkRemoteClientPolicy RemoteClientPolicy{ MidiNetworkRemoteClientPolicy::PolicyAllowAny };

    // Identity keys, as produced by MidiNetworkRemoteClientIdentity::Key(). Populated from the
    // configuration and added to at runtime when a user approves or denies a client.
    std::vector<std::wstring> AllowedClientKeys{};
    std::vector<std::wstring> DeniedClientKeys{};

    // protocol
    MidiNetworkHostProtocol NetworkProtocol{ MidiNetworkHostProtocol::ProtocolDefault };

    // Anything other than none is refused by configuration validation. See MidiNetworkCredentials.h.
    MidiNetworkHostAuthentication Authentication{ MidiNetworkHostAuthentication::NoAuthentication };

    // generated properties
    winrt::hstring ServiceInstanceName;     // instance name for the PTR record
    winrt::hstring HostName;                // must include the .local domain

};



class MidiNetworkHost : public std::enable_shared_from_this<MidiNetworkHost>
{
public:
    HRESULT Initialize(_In_ MidiNetworkHostDefinition const& hostDefinition);

    // Does nothing when the host is already running
    HRESULT Start();
    HRESULT Stop();

    HRESULT Shutdown();

    bool HasStarted() { return m_started; }

    // True when the configured port was unavailable and the host started on an allocated one.
    bool PortFallbackUsed() { return m_portFallbackUsed; }

    // True when the host is limited to an adapter which is missing, and is running on every
    // adapter until it comes back.
    bool NetworkAdapterFallbackUsed() { return m_networkAdapterFallbackUsed; }

    // True when the host is limited to an adapter which is missing and may not fall back, so it
    // is not running. It starts by itself when the adapter comes back.
    bool IsWaitingForNetworkAdapter() { return m_waitingForNetworkAdapter; }

    // A configuration update. Takes effect at the next ReconcileNetworkAdapter.
    void SetNetworkAdapter(
        _In_ winrt::guid const& id,
        _In_ winrt::hstring const& name,
        _In_ winrt::hstring const& physicalAddress,
        _In_ bool const allowFallback);

    // Moves the host to where its adapter setting says it should be now: onto the adapter when
    // it is there, onto every adapter or into waiting when it is not. Leaves a host the customer
    // stopped alone.
    HRESULT ReconcileNetworkAdapter(_In_ std::vector<::WindowsMidiServicesInternal::MidiNetworkAdapterInfo> const& adapters);

    // True when a DNS-SD collision made the responder advertise this host under a different
    // instance label than the one it was configured with.
    bool ServiceInstanceNameWasChanged();

    // What this host is actually called on the network. The configured name when it is not
    // advertising, because then there is nothing on the wire to disagree with.
    winrt::hstring ActualServiceInstanceName();

    bool IsEnabled() { return m_enabled; }

    // Fixed once the host is initialized, so it is safe to read without copying the definition
    winrt::guid EntryIdentifier() const noexcept { return m_entryIdentifier; }

    MidiNetworkHostDefinition GetDefinition()
    {
        auto lock = m_remoteClientListsLock.lock();

        return m_hostDefinition;
    }

    // Used for the next remote client which connects to this host. The ones already connected are
    // updated in place by the configuration manager.
    void SetFallbackMidi1PortCount(_In_ uint8_t const value) noexcept { m_fallbackMidi1PortCount = value; }

    // Used for the next remote client which connects. An endpoint already up keeps what it has.
    void SetCreateMidi1Ports(_In_ bool const value) noexcept { m_createUmpEndpointsOnly = !value; }

    // Used for the next remote client which connects. The ones already connected are updated in
    // place by the configuration manager.
    void SetSendSpeedLimit(_In_ uint32_t const speedMultiple, _In_ bool const reduceAutomatically) noexcept
    {
        m_sendSpeedLimit = speedMultiple;
        m_reduceSendSpeedAutomatically = reduceAutomatically;
    }

    // Used for the remote clients which identify themselves from now on. The ones already
    // connected are updated in place by the configuration manager.
    void SetRemoteClientSettings(_In_ std::vector<MidiNetworkRemoteClientSettings> const& settings);

    // What this host sends to one remote client: its own settings when the host has them,
    // otherwise the host's
    MidiNetworkRemoteClientSendSpeed GetSendSpeedForRemoteClient(_In_ MidiNetworkRemoteClientIdentity const& identity) noexcept;

    winrt::hstring ActualPort() { auto socket = GetSocket(); return socket != nullptr ? socket.Information().LocalPort() : L""; }
    winrt::hstring ActualAddress() { auto socket = GetSocket(); return socket != nullptr ? socket.Information().LocalAddress().DisplayName() : L""; }

    // What should happen to an invitation from this client, given the policy and the lists.
    MidiNetworkRemoteClientDecision EvaluateRemoteClient(_In_ MidiNetworkRemoteClientIdentity const& identity);

    MidiNetworkRemoteClientPolicy GetRemoteClientPolicy() const { return m_hostDefinition.RemoteClientPolicy; }

    // A user decision arriving through the configuration manager. Persisting it is the caller's
    // job; these take effect immediately either way.
    HRESULT AddRemoteClientToAllowList(_In_ MidiNetworkRemoteClientIdentity const& identity);
    HRESULT AddRemoteClientToDenyList(_In_ MidiNetworkRemoteClientIdentity const& identity);

    // Drops whichever decision is held for this identity, so the next invitation from it is
    // judged on the host policy alone, as though it had never been seen.
    HRESULT ForgetRemoteClient(_In_ MidiNetworkRemoteClientIdentity const& identity);

private:
    bool m_enabled{ true };
    std::atomic<bool> m_portFallbackUsed{ false };
    std::atomic<bool> m_networkAdapterFallbackUsed{ false };
    std::atomic<bool> m_waitingForNetworkAdapter{ false };
    std::atomic<bool> m_started{ false };
    std::atomic<bool> m_createUmpEndpointsOnly{ true };
    std::atomic<uint8_t> m_fallbackMidi1PortCount{ MIDI_NETWORK_MIDI_FALLBACK_MIDI1_PORT_COUNT_DEFAULT };
    std::atomic<uint32_t> m_sendSpeedLimit{ 0 };
    std::atomic<bool> m_reduceSendSpeedAutomatically{ false };

    winrt::guid m_entryIdentifier{ };

    std::wstring m_hostEndpointName{ };
    std::wstring m_hostProductInstanceId{ };

    std::wstring m_parentDeviceInstanceId{ };

    winrt::event_token m_messageReceivedEventToken;

    void OnMessageReceived(
        _In_ DatagramSocket const& sender,
        _In_ DatagramSocketMessageReceivedEventArgs const& args);

    // The first datagram from a remote with no connection. Returns the connection to hand it to,
    // or nullptr when the remote was refused or ignored.
    std::shared_ptr<MidiNetworkConnection> AdmitNewRemote(
        _In_ DatagramSocketMessageReceivedEventArgs const& args,
        _In_ MidiNetworkCommandPacketHeader const& firstCommandHeader);

    // A remote reconnects from a new ephemeral port, so a finished session's entry is released
    // straight away rather than holding a connection slot until the idle reaper notices.
    void ReleaseConnectionIfSessionFinished(
        _In_ std::shared_ptr<MidiNetworkConnection> const& connection,
        _In_ DatagramSocketMessageReceivedEventArgs const& args);

    MidiNetworkHostDefinition m_hostDefinition{};

    // Start, Stop and Shutdown arrive from the endpoint creator worker and from configuration
    // calls, which the service does not serialize.
    wil::critical_section m_lifecycleLock;

    // Replaced only under m_lifecycleLock. Readers take a copy, because enumerateHosts polls it
    // while a user starts and stops the host.
    wil::critical_section m_advertiserLock;
    std::shared_ptr<MidiNetworkAdvertiser> m_advertiser{ nullptr };

    std::shared_ptr<MidiNetworkAdvertiser> GetAdvertiser()
    {
        auto lock = m_advertiserLock.lock();

        return m_advertiser;
    }

    // The label on the wire for this advertiser, or the configured one when it has none.
    winrt::hstring ActualServiceInstanceName(_In_ std::shared_ptr<MidiNetworkAdvertiser> const& advertiser);

    // Binds the configured port, or an automatic one when the configuration allows falling back.
    HRESULT BindSocket(_In_ DatagramSocket const& socket, _Out_ uint16_t& boundPort);

    // Registers the host with DNS-SD and publishes the advertiser only once that succeeded. A
    // non-zero interface index limits the advertisement to that adapter, and the follow-up
    // announcements to the adapter with that id.
    HRESULT StartAdvertising(
        _In_ uint16_t const boundPort,
        _In_ winrt::guid const& networkAdapterId,
        _In_ uint32_t const networkInterfaceIndex);

    // Where Start should run the host. False, with waiting set, when it has to wait for its
    // adapter. A null adapter id and a zero interface index mean every adapter.
    bool ChooseNetworkAdapter(
        _Out_ winrt::guid& adapterId,
        _Out_ uint32_t& interfaceIndex,
        _Out_ bool& fallbackUsed);

    // Records a change, and tells the notifications app so it can say why the host is not running
    void SetWaitingForNetworkAdapter(_In_ bool const waiting) noexcept;

    // False for a datagram which arrived on any adapter but the one the host is limited to
    bool ArrivedOnActiveNetworkAdapter(_In_ DatagramSocketMessageReceivedEventArgs const& args);

    DatagramSocket m_socket{ nullptr };

    // Stop() replaces this while receive and configuration threads are still reading it.
    wil::critical_section m_socketLock;

    // The adapter the running host is limited to, or a null GUID for every adapter. Guarded by
    // m_socketLock, because a receive handler from a socket being replaced can still read it.
    winrt::guid m_activeNetworkAdapterId{};

    DatagramSocket GetSocket()
    {
        auto lock = m_socketLock.lock();

        return m_socket;
    }

    HRESULT CreateNetworkConnection(
        _In_ winrt::Windows::Networking::HostName const& remoteHostName,
        _In_ winrt::hstring const& remotePort,
        _In_ winrt::Windows::Networking::HostName const& localHostName,
        _Out_ std::shared_ptr<MidiNetworkConnection>& connection);

    // Spec 6.4: the first command from a client which has no session must be an invitation.
    static bool IsSessionOpeningCommand(_In_ uint8_t const commandCode);

    // Commands which the spec says warrant a Bye when no session exists.
    static bool WarrantsSessionNotEstablishedBye(_In_ uint8_t const commandCode);

    // Replies to a remote we hold no connection for, so nothing is allocated on its behalf.
    HRESULT SendUnconnectedBye(
        _In_ winrt::Windows::Networking::HostName const& remoteHostName,
        _In_ winrt::hstring const& remotePort,
        _In_ winrt::Windows::Networking::HostName const& localHostName,
        _In_ MidiNetworkCommandByeReason const reason,
        _In_ std::wstring const& message);

    MidiNetworkReplyRateLimiter m_refusalRateLimiter;

    // Guards the allow and deny lists, which a user can change at any time through the
    // configuration manager while the receive path is reading them, and every copy of the
    // definition, which includes them.
    wil::critical_section m_remoteClientListsLock;

};
