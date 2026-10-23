// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// Which remotes may connect to each host on this PC.
// ============================================================================

#include "pch.h"

namespace
{
    bool SameText(_In_ std::wstring const& left, _In_ std::wstring const& right) noexcept
    {
        return CompareStringOrdinal(left.c_str(), static_cast<int>(left.size()), right.c_str(), static_cast<int>(right.size()), TRUE) == CSTR_EQUAL;
    }

    bool Contains(_In_ std::vector<std::wstring> const& texts, _In_ std::wstring const& text) noexcept
    {
        return std::any_of(texts.begin(), texts.end(), [&](std::wstring const& candidate) { return SameText(candidate, text); });
    }

    void Erase(_Inout_ std::vector<std::wstring>& names, _In_ std::wstring const& name)
    {
        std::erase_if(names, [&](std::wstring const& candidate) { return SameText(candidate, name); });
    }

    bool Add(_Inout_ std::vector<std::wstring>& names, _In_ std::wstring const& name)
    {
        if (Contains(names, name)) return true;
        if (names.size() >= MIDI_RTP_MAX_REMOTE_CLIENT_DECISIONS_PER_HOST) return false;

        names.push_back(name);
        return true;
    }

    // the newest address last, so the oldest goes first when there are too many
    void AddAddress(_Inout_ std::vector<std::wstring>& addresses, _In_ std::wstring const& address)
    {
        if (address.empty() || Contains(addresses, address)) return;

        if (addresses.size() >= MIDI_RTP_MAX_REMOTE_ENDPOINT_ADDRESSES) addresses.erase(addresses.begin());
        addresses.push_back(address);
    }

    std::vector<std::wstring> Deduplicated(_In_ std::vector<std::wstring> const& names)
    {
        std::vector<std::wstring> result;

        for (auto const& name : names)
        {
            if (name.empty() || name.size() > MIDI_RTP_REMOTE_CLIENT_NAME_MAX_CHARS) continue;
            if (!Add(result, name)) break;
        }

        return result;
    }

    template <typename Entry>
    Entry* FindNamed(_Inout_ std::vector<Entry>& entries, _In_ std::wstring const& name)
    {
        auto const found = std::find_if(entries.begin(), entries.end(), [&](Entry const& entry) { return SameText(entry.Name, name); });
        return found == entries.end() ? nullptr : &*found;
    }

    template <typename Entry>
    void EraseNamed(_Inout_ std::vector<Entry>& entries, _In_ std::wstring const& name)
    {
        std::erase_if(entries, [&](Entry const& entry) { return SameText(entry.Name, name); });
    }
}


_Use_decl_annotations_
bool
RtpMidiApprovals::ExpirePending(HostState& host, uint64_t const now)
{
    bool listedOneWent{ false };

    std::erase_if(host.Pending, [&](Pending const& pending)
        {
            if (pending.ExpiresTick > now) return false;

            if (pending.State != PendingState::DeniedOnce) listedOneWent = true;
            return true;
        });

    return listedOneWent;
}


// Anyone can send an invitation from any name, so one address only gets a couple of places in the
// list. When the list is full, the request whose remote stopped asking longest ago makes room, so
// a burst of made-up names can't keep a real remote from being listed for long.
_Use_decl_annotations_
bool
RtpMidiApprovals::MakeRoomForPending(HostState& host, std::wstring const& remoteAddress, uint64_t const now, bool& listedOneWent)
{
    listedOneWent = false;

    auto const fromAddress = std::count_if(host.Pending.begin(), host.Pending.end(),
        [&](Pending const& pending) { return pending.State == PendingState::Waiting && SameText(pending.RemoteAddress, remoteAddress); });

    if (static_cast<size_t>(fromAddress) >= MIDI_RTP_MAX_PENDING_REMOTE_CLIENTS_PER_ADDRESS) return false;
    if (host.Pending.size() < MIDI_RTP_MAX_PENDING_REMOTE_CLIENTS_PER_HOST) return true;

    auto idlest = host.Pending.end();

    for (auto it = host.Pending.begin(); it != host.Pending.end(); ++it)
    {
        if (it->State != PendingState::Waiting || now - it->LastAskTick < MIDI_RTP_PENDING_REMOTE_CLIENT_IDLE_MS) continue;
        if (idlest == host.Pending.end() || it->LastAskTick < idlest->LastAskTick) idlest = it;
    }

    if (idlest == host.Pending.end()) return false;

    host.Pending.erase(idlest);
    listedOneWent = true;

    return true;
}


_Use_decl_annotations_
RtpMidiApprovals::RemoteEndpoint*
RtpMidiApprovals::FindRemoteEndpoint(HostState& host, std::wstring const& remoteName)
{
    return FindNamed(host.RemoteEndpoints, remoteName);
}


_Use_decl_annotations_
void
RtpMidiApprovals::SetPolicy(GUID const& hostId, RtpMidiRemoteClientPolicy const policy)
{
    bool signal{ false };

    {
        auto lock = m_lock.lock_exclusive();

        auto& host = m_hosts[hostId];
        host.Policy = policy;

        // Nothing waits on a host which lets everyone in, except a remote asking for an endpoint
        // kept for a name from an address that name never used
        if (policy == RtpMidiRemoteClientPolicy::AllowAny && !host.Pending.empty())
        {
            std::erase_if(host.Pending, [&](Pending const& pending)
                {
                    auto const endpoint = FindRemoteEndpoint(host, pending.RemoteName);
                    if (endpoint != nullptr && endpoint->Away && !endpoint->Addresses.empty()) return false;

                    if (pending.State != PendingState::DeniedOnce) signal = true;
                    return true;
                });
        }
    }

    if (signal) m_signal.SignalPendingApprovalChanged();
}


_Use_decl_annotations_
void
RtpMidiApprovals::SetRememberedDecisions(GUID const& hostId, std::vector<std::wstring> const& allowed, std::vector<std::wstring> const& denied)
{
    auto lock = m_lock.lock_exclusive();

    auto& host = m_hosts[hostId];
    host.Allowed = Deduplicated(allowed);
    host.Denied = Deduplicated(denied);
}


_Use_decl_annotations_
void
RtpMidiApprovals::RemoveHost(GUID const& hostId)
{
    bool signal{ false };

    {
        auto lock = m_lock.lock_exclusive();

        auto const it = m_hosts.find(hostId);
        if (it == m_hosts.end()) return;

        signal = std::any_of(it->second.Pending.begin(), it->second.Pending.end(),
            [](Pending const& pending) { return pending.State != PendingState::DeniedOnce; });

        m_hosts.erase(it);
    }

    if (signal) m_signal.SignalPendingApprovalChanged();
}


_Use_decl_annotations_
RtpMidi::Admission
RtpMidiApprovals::Admit(GUID const& hostId, std::wstring const& remoteName, std::wstring const& remoteAddress)
{
    bool signal{ false };
    RtpMidi::Admission admission{ RtpMidi::Admission::Reject };

    {
        auto lock = m_lock.lock_exclusive();

        // a host on its way out lets nobody new in
        auto const it = m_hosts.find(hostId);
        if (it == m_hosts.end()) return RtpMidi::Admission::Reject;

        auto& host = it->second;
        auto const now = GetTickCount64();

        signal = ExpirePending(host, now);

        auto const pending = std::find_if(host.Pending.begin(), host.Pending.end(),
            [&](Pending const& candidate) { return SameText(candidate.RemoteName, remoteName); });

        auto const endpoint = FindRemoteEndpoint(host, remoteName);

        // An endpoint kept while its remote is away goes back only to an address that remote used
        bool const endpointClaimed = endpoint != nullptr && endpoint->Away && !endpoint->Addresses.empty() && !Contains(endpoint->Addresses, remoteAddress);

        auto const untilRestart = FindNamed(host.AllowedUntilRestart, remoteName);

        if (Contains(host.Denied, remoteName) || Contains(host.DeniedUntilRestart, remoteName))
        {
            admission = RtpMidi::Admission::Reject;
        }
        else if (pending != host.Pending.end() && pending->State != PendingState::Waiting && SameText(pending->RemoteAddress, remoteAddress))
        {
            // a decision about one request, answered on the next ask from the address it showed
            bool const approved = pending->State == PendingState::ApprovedOnce;

            host.Pending.erase(pending);

            if (approved)
            {
                if (endpoint != nullptr) AddAddress(endpoint->Addresses, remoteAddress);

                admission = RtpMidi::Admission::Accept;
                signal = true;
            }
            else
            {
                admission = RtpMidi::Admission::Reject;
            }
        }
        else if (untilRestart != nullptr && Contains(untilRestart->Addresses, remoteAddress))
        {
            if (endpoint != nullptr) AddAddress(endpoint->Addresses, remoteAddress);

            admission = RtpMidi::Admission::Accept;
        }
        else if (!endpointClaimed && untilRestart != nullptr && untilRestart->Addresses.empty())
        {
            // decided before the remote ever asked, so it holds for the first address it asks from
            AddAddress(untilRestart->Addresses, remoteAddress);

            admission = RtpMidi::Admission::Accept;
        }
        else if (!endpointClaimed && (host.Policy == RtpMidiRemoteClientPolicy::AllowAny || Contains(host.Allowed, remoteName)))
        {
            admission = RtpMidi::Admission::Accept;
        }
        else if (pending != host.Pending.end())
        {
            // One request per name, for the address it first came from. An ask from anywhere else
            // waits for that one to be decided or to expire, so the address the customer is shown
            // is the one their decision holds for.
            if (pending->State == PendingState::Waiting && SameText(pending->RemoteAddress, remoteAddress))
            {
                pending->LastAskTick = now;
                pending->ExpiresTick = now + MIDI_RTP_PENDING_REMOTE_CLIENT_LIFETIME_MS;
            }

            admission = RtpMidi::Admission::Hold;
        }
        else if (remoteName.empty() || remoteName.size() > MIDI_RTP_REMOTE_CLIENT_NAME_MAX_CHARS)
        {
            // a remote which cannot be listed for someone to decide about is refused
            admission = RtpMidi::Admission::Reject;
        }
        else
        {
            bool madeRoom{ false };

            if (MakeRoomForPending(host, remoteAddress, now, madeRoom))
            {
                Pending added{};
                added.RemoteName = remoteName;
                added.RemoteAddress = remoteAddress;
                added.LastAskTick = now;
                added.ExpiresTick = now + MIDI_RTP_PENDING_REMOTE_CLIENT_LIFETIME_MS;
                GetSystemTimePreciseAsFileTime(&added.FirstRequest);

                host.Pending.push_back(std::move(added));

                admission = RtpMidi::Admission::Hold;
                signal = true;
            }
            else
            {
                admission = RtpMidi::Admission::Reject;
            }

            if (madeRoom) signal = true;
        }
    }

    if (signal) m_signal.SignalPendingApprovalChanged();

    return admission;
}


_Use_decl_annotations_
HRESULT
RtpMidiApprovals::Decide(GUID const& hostId, std::wstring const& remoteName, bool const allow, RtpMidiApprovalScope const scope, uint32_t& errorCode)
{
    errorCode = 0;
    bool signal{ false };

    {
        auto lock = m_lock.lock_exclusive();

        auto const it = m_hosts.find(hostId);
        if (it == m_hosts.end())
        {
            errorCode = RTP_MIDI_ERROR_CODE_ENTRY_NOT_FOUND;
            return E_NOTFOUND;
        }

        auto& host = it->second;
        auto const now = GetTickCount64();

        signal = ExpirePending(host, now);

        auto const pending = std::find_if(host.Pending.begin(), host.Pending.end(),
            [&](Pending const& candidate) { return SameText(candidate.RemoteName, remoteName); });

        bool const isPending = pending != host.Pending.end();
        std::wstring const requestAddress = isPending ? pending->RemoteAddress : std::wstring{};

        // Approving a request approves the address it showed, for the endpoint kept for that name too
        auto const approveRequestAddress = [&]()
            {
                if (!allow || !isPending) return;

                if (auto const endpoint = FindRemoteEndpoint(host, remoteName)) AddAddress(endpoint->Addresses, requestAddress);
            };

        if (scope == RtpMidiApprovalScope::Once)
        {
            if (allow && !isPending)
            {
                errorCode = RTP_MIDI_ERROR_CODE_PENDING_REMOTE_CLIENT_NOT_FOUND;
                if (signal) m_signal.SignalPendingApprovalChanged();
                return E_NOTFOUND;
            }

            if (isPending)
            {
                approveRequestAddress();

                // answered on its next ask, which is also when a refusal stops it asking
                pending->State = allow ? PendingState::ApprovedOnce : PendingState::DeniedOnce;
                pending->ExpiresTick = now + MIDI_RTP_PENDING_REMOTE_CLIENT_LIFETIME_MS;
                signal = true;
            }
        }
        else
        {
            if (allow && scope == RtpMidiApprovalScope::UntilRestart)
            {
                auto entry = FindNamed(host.AllowedUntilRestart, remoteName);

                if (entry == nullptr)
                {
                    if (host.AllowedUntilRestart.size() >= MIDI_RTP_MAX_REMOTE_CLIENT_DECISIONS_PER_HOST)
                    {
                        errorCode = RTP_MIDI_ERROR_CODE_TOO_MANY_REMOTE_CLIENT_DECISIONS;
                        if (signal) m_signal.SignalPendingApprovalChanged();
                        return E_BOUNDS;
                    }

                    host.AllowedUntilRestart.push_back(AddressedName{ remoteName, {} });
                    entry = &host.AllowedUntilRestart.back();
                }

                // For the request's address. Made before any request, it holds for the first
                // address the name asks from.
                if (isPending) AddAddress(entry->Addresses, requestAddress);
            }
            else
            {
                auto& keep = allow ?
                    host.Allowed :
                    (scope == RtpMidiApprovalScope::Always ? host.Denied : host.DeniedUntilRestart);

                if (!Add(keep, remoteName))
                {
                    errorCode = RTP_MIDI_ERROR_CODE_TOO_MANY_REMOTE_CLIENT_DECISIONS;
                    if (signal) m_signal.SignalPendingApprovalChanged();
                    return E_BOUNDS;
                }
            }

            // the newest decision is the one that holds
            if (allow)
            {
                Erase(host.Denied, remoteName);
                Erase(host.DeniedUntilRestart, remoteName);
                if (scope == RtpMidiApprovalScope::Always) EraseNamed(host.AllowedUntilRestart, remoteName);
            }
            else
            {
                Erase(host.Allowed, remoteName);
                EraseNamed(host.AllowedUntilRestart, remoteName);
                if (scope == RtpMidiApprovalScope::Always) Erase(host.DeniedUntilRestart, remoteName);
            }

            approveRequestAddress();

            // the lists answer its next ask from here on
            if (isPending)
            {
                host.Pending.erase(pending);
                signal = true;
            }
        }
    }

    if (signal) m_signal.SignalPendingApprovalChanged();

    return S_OK;
}


_Use_decl_annotations_
HRESULT
RtpMidiApprovals::Forget(GUID const& hostId, std::wstring const& remoteName)
{
    auto lock = m_lock.lock_exclusive();

    auto const it = m_hosts.find(hostId);
    RETURN_HR_IF(E_NOTFOUND, it == m_hosts.end());

    Erase(it->second.Allowed, remoteName);
    Erase(it->second.Denied, remoteName);
    EraseNamed(it->second.AllowedUntilRestart, remoteName);
    Erase(it->second.DeniedUntilRestart, remoteName);

    return S_OK;
}


_Use_decl_annotations_
RtpMidiRemoteClientPolicy
RtpMidiApprovals::Policy(GUID const& hostId)
{
    auto lock = m_lock.lock_shared();

    auto const it = m_hosts.find(hostId);
    return it == m_hosts.end() ? RtpMidiRemoteClientPolicy::AllowAny : it->second.Policy;
}


_Use_decl_annotations_
std::vector<RtpMidiRemoteClientDecision>
RtpMidiApprovals::Decisions(GUID const& hostId)
{
    std::vector<RtpMidiRemoteClientDecision> decisions;

    auto lock = m_lock.lock_shared();

    auto const it = m_hosts.find(hostId);
    if (it == m_hosts.end()) return decisions;

    auto const append = [&](std::vector<std::wstring> const& names, bool const allowed, bool const untilRestart)
        {
            for (auto const& name : names) decisions.push_back(RtpMidiRemoteClientDecision{ name, allowed, untilRestart });
        };

    append(it->second.Allowed, true, false);

    for (auto const& entry : it->second.AllowedUntilRestart) decisions.push_back(RtpMidiRemoteClientDecision{ entry.Name, true, true });

    append(it->second.Denied, false, false);
    append(it->second.DeniedUntilRestart, false, true);

    return decisions;
}


std::vector<RtpMidiPendingRemoteClient>
RtpMidiApprovals::PendingRemoteClients()
{
    std::vector<RtpMidiPendingRemoteClient> result;
    bool signal{ false };

    {
        auto lock = m_lock.lock_exclusive();
        auto const now = GetTickCount64();

        for (auto& [hostId, host] : m_hosts)
        {
            if (ExpirePending(host, now)) signal = true;

            for (auto const& pending : host.Pending)
            {
                if (pending.State == PendingState::DeniedOnce) continue;

                result.push_back(RtpMidiPendingRemoteClient{ hostId, pending.RemoteName, pending.RemoteAddress, pending.FirstRequest, pending.State == PendingState::ApprovedOnce });
            }
        }
    }

    if (signal) m_signal.SignalPendingApprovalChanged();

    return result;
}


_Use_decl_annotations_
void
RtpMidiApprovals::RemoteEndpointConnected(GUID const& hostId, std::wstring const& remoteName, std::wstring const& remoteAddress)
{
    if (remoteName.empty()) return;

    auto lock = m_lock.lock_exclusive();

    auto const it = m_hosts.find(hostId);
    if (it == m_hosts.end()) return;

    auto endpoint = FindRemoteEndpoint(it->second, remoteName);

    if (endpoint == nullptr)
    {
        if (it->second.RemoteEndpoints.size() >= MIDI_RTP_MAX_REMOTE_CLIENT_DECISIONS_PER_HOST) return;

        it->second.RemoteEndpoints.push_back(RemoteEndpoint{ remoteName, {}, false });
        endpoint = &it->second.RemoteEndpoints.back();
    }

    AddAddress(endpoint->Addresses, remoteAddress);
    endpoint->Away = false;
}

_Use_decl_annotations_
void
RtpMidiApprovals::RemoteEndpointAway(GUID const& hostId, std::wstring const& remoteName)
{
    if (remoteName.empty()) return;

    auto lock = m_lock.lock_exclusive();

    auto const it = m_hosts.find(hostId);
    if (it == m_hosts.end()) return;

    auto endpoint = FindRemoteEndpoint(it->second, remoteName);

    if (endpoint == nullptr)
    {
        if (it->second.RemoteEndpoints.size() >= MIDI_RTP_MAX_REMOTE_CLIENT_DECISIONS_PER_HOST) return;

        it->second.RemoteEndpoints.push_back(RemoteEndpoint{ remoteName, {}, true });
        return;
    }

    endpoint->Away = true;
}

_Use_decl_annotations_
void
RtpMidiApprovals::RemoteEndpointRemoved(GUID const& hostId, std::wstring const& remoteName)
{
    auto lock = m_lock.lock_exclusive();

    auto const it = m_hosts.find(hostId);
    if (it == m_hosts.end()) return;

    EraseNamed(it->second.RemoteEndpoints, remoteName);
}

_Use_decl_annotations_
bool
RtpMidiApprovals::MayTakeRemoteEndpoint(GUID const& hostId, std::wstring const& remoteName, std::wstring const& remoteAddress)
{
    auto lock = m_lock.lock_exclusive();

    auto const it = m_hosts.find(hostId);
    if (it == m_hosts.end()) return true;

    auto const endpoint = FindRemoteEndpoint(it->second, remoteName);

    return endpoint == nullptr || endpoint->Addresses.empty() || Contains(endpoint->Addresses, remoteAddress);
}


void
RtpMidiApprovals::ForgetRemoteEndpoints()
{
    auto lock = m_lock.lock_exclusive();

    for (auto& entry : m_hosts) entry.second.RemoteEndpoints.clear();
}


void
RtpMidiApprovals::Shutdown() noexcept
{
    m_signal.Shutdown();
}
