// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// Which remotes may connect to each host on this PC, and which are waiting for a decision.
//
// AppleMIDI has no way to ask a remote to wait, so a remote that needs a decision gets no answer
// at all. It keeps asking about once a second for about twelve seconds, and its first ask after
// an approval gets in. A remote is known by the name it sends, the way macOS remembers it: an
// invitation carries nothing else that stays the same from one connection to the next.
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

    // where the last ask came from, for display only
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
// notification signal, which only queues work.
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

    bool HasHost(_In_ GUID const& hostId);
    RtpMidiRemoteClientPolicy Policy(_In_ GUID const& hostId);
    std::vector<RtpMidiRemoteClientDecision> Decisions(_In_ GUID const& hostId);
    std::vector<RtpMidiPendingRemoteClient> PendingRemoteClients();

private:
    enum class PendingState { Waiting, ApprovedOnce, DeniedOnce };

    struct Pending
    {
        std::wstring RemoteName;
        std::wstring RemoteAddress;
        FILETIME FirstRequest{};
        uint64_t ExpiresTick{ 0 };
        PendingState State{ PendingState::Waiting };
    };

    struct HostState
    {
        RtpMidiRemoteClientPolicy Policy{ RtpMidiRemoteClientPolicy::AllowAny };
        std::vector<std::wstring> Allowed;
        std::vector<std::wstring> Denied;
        std::vector<std::wstring> AllowedUntilRestart;
        std::vector<std::wstring> DeniedUntilRestart;
        std::vector<Pending> Pending;
    };

    // Expired requests go whenever the state is looked at, so nothing needs a timer. True when
    // a request which was listed went away.
    static bool ExpirePending(_Inout_ HostState& host, _In_ uint64_t const now);

    std::mutex m_lock;
    std::map<GUID, HostState, GuidLess> m_hosts;

    RtpMidiNotificationSignal m_signal;
};
