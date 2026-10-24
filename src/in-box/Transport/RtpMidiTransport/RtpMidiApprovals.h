// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// Which remotes may connect to each host on this PC, and which are waiting for a decision.
//
// AppleMIDI has no way to ask a remote to wait, so a remote that needs a decision gets no answer
// at all. It keeps asking about once a second for about twelve seconds, and its first ask after
// an approval gets in. A remote is known by the name it sends, the way macOS remembers it: an
// invitation carries nothing else that stays the same from one connection to the next.
//
// RTP-MIDI has no authentication, so anyone can send any name. The only other thing a remote
// can't choose freely is the address it connects from, so a decision about one request holds for
// the address shown in it, and the endpoint a host keeps for a remote while it is away goes back
// only to an address that remote connected from. A remote using that name from anywhere else is
// asked about again, showing the new address.
// ============================================================================

#pragma once

enum class RtpMidiApprovalScope
{
    Once,
    UntilRestart,
    Always,
};

struct RtpMidiPendingRemoteClient
{
    GUID HostId{};
    std::wstring RemoteName;

    // where the first ask came from. A decision about the request holds for this address.
    std::wstring RemoteAddress;

    FILETIME FirstRequest{};

    // approved for one connection, and waiting for the remote to ask again
    bool Approved{ false };
};

struct RtpMidiRemoteClientDecision
{
    std::wstring RemoteName;
    bool Allowed{ false };
    bool UntilRestart{ false };
};

// Thread safe. Admit is called on socket receive threads, and nothing here calls out except the
// notification signal, which only queues work. Its lock is taken after any other lock in the
// transport, never before one.
class RtpMidiApprovals
{
public:
    // Creates the host's state if it is new. Decisions already made for it are kept.
    void SetPolicy(_In_ GUID const& hostId, _In_ RtpMidiRemoteClientPolicy const policy);

    // What the configuration file remembers. Replaces the lists it held before, and leaves the
    // decisions made until restart alone.
    void SetRememberedDecisions(_In_ GUID const& hostId, _In_ std::vector<std::wstring> const& allowed, _In_ std::vector<std::wstring> const& denied);

    void RemoveHost(_In_ GUID const& hostId);

    RtpMidi::Admission Admit(_In_ GUID const& hostId, _In_ std::wstring const& remoteName, _In_ std::wstring const& remoteAddress);

    // E_NOTFOUND for an unknown host. A decision for this request only needs a remote waiting;
    // RTP_MIDI_ERROR_CODE_PENDING_REMOTE_CLIENT_NOT_FOUND tells the caller which it was.
    HRESULT Decide(_In_ GUID const& hostId, _In_ std::wstring const& remoteName, _In_ bool const allow, _In_ RtpMidiApprovalScope const scope, _Out_ uint32_t& errorCode);

    // Nothing to forget is not an error
    HRESULT Forget(_In_ GUID const& hostId, _In_ std::wstring const& remoteName);

    RtpMidiRemoteClientPolicy Policy(_In_ GUID const& hostId);
    std::vector<RtpMidiRemoteClientDecision> Decisions(_In_ GUID const& hostId);
    std::vector<RtpMidiPendingRemoteClient> PendingRemoteClients();

    // The endpoint a host makes for a remote, found again by the remote's name, and the addresses
    // that remote connected from in this run of the service. While the endpoint is kept for a
    // remote which is away, a remote of that name from any other address is asked about first. A
    // kept endpoint made before its remote ever connected goes to the first one that does.
    void RemoteEndpointConnected(_In_ GUID const& hostId, _In_ std::wstring const& remoteName, _In_ std::wstring const& remoteAddress);
    void RemoteEndpointAway(_In_ GUID const& hostId, _In_ std::wstring const& remoteName);
    void RemoteEndpointRemoved(_In_ GUID const& hostId, _In_ std::wstring const& remoteName);
    bool MayTakeRemoteEndpoint(_In_ GUID const& hostId, _In_ std::wstring const& remoteName, _In_ std::wstring const& remoteAddress);

    // The endpoint manager stopped, and the service removes its endpoints
    void ForgetRemoteEndpoints();

    // Lets the deferred notification finish, so nothing is left to run when the DLL unloads
    void Shutdown() noexcept;

private:
    enum class PendingState { Waiting, ApprovedOnce, DeniedOnce };

    struct Pending
    {
        std::wstring RemoteName;
        std::wstring RemoteAddress;
        FILETIME FirstRequest{};
        uint64_t LastAskTick{ 0 };
        uint64_t ExpiresTick{ 0 };
        PendingState State{ PendingState::Waiting };
    };

    // A name and the addresses it holds for. No addresses means the first one to use it.
    struct AddressedName
    {
        std::wstring Name;
        std::vector<std::wstring> Addresses;
    };

    struct RemoteEndpoint
    {
        std::wstring Name;
        std::vector<std::wstring> Addresses;
        bool Away{ false };
    };

    struct HostState
    {
        RtpMidiRemoteClientPolicy Policy{ RtpMidiRemoteClientPolicy::AllowAny };
        std::vector<std::wstring> Allowed;
        std::vector<std::wstring> Denied;
        std::vector<AddressedName> AllowedUntilRestart;
        std::vector<std::wstring> DeniedUntilRestart;
        std::vector<Pending> Pending;
        std::vector<RemoteEndpoint> RemoteEndpoints;
    };

    // Expired requests go whenever the state is looked at, so nothing needs a timer. True when
    // a request which was listed went away.
    static bool ExpirePending(_Inout_ HostState& host, _In_ uint64_t const now);

    // Room for a new request from this address. False when there is none.
    static bool MakeRoomForPending(_Inout_ HostState& host, _In_ std::wstring const& remoteAddress, _In_ uint64_t const now, _Out_ bool& listedOneWent);

    static RemoteEndpoint* FindRemoteEndpoint(_Inout_ HostState& host, _In_ std::wstring const& remoteName);

    wil::srwlock m_lock;
    std::map<GUID, HostState, GuidLess> m_hosts;

    RtpMidiNotificationSignal m_signal;
};
