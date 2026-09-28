// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// Which remotes may connect to each host on this PC.
// ============================================================================

#include "pch.h"

namespace
{
    bool SameName(_In_ std::wstring const& left, _In_ std::wstring const& right) noexcept
    {
        return CompareStringOrdinal(left.c_str(), static_cast<int>(left.size()), right.c_str(), static_cast<int>(right.size()), TRUE) == CSTR_EQUAL;
    }

    bool Contains(_In_ std::vector<std::wstring> const& names, _In_ std::wstring const& name) noexcept
    {
        return std::any_of(names.begin(), names.end(), [&](std::wstring const& candidate) { return SameName(candidate, name); });
    }

    void Erase(_Inout_ std::vector<std::wstring>& names, _In_ std::wstring const& name)
    {
        std::erase_if(names, [&](std::wstring const& candidate) { return SameName(candidate, name); });
    }

    bool Add(_Inout_ std::vector<std::wstring>& names, _In_ std::wstring const& name)
    {
        if (Contains(names, name)) return true;
        if (names.size() >= MIDI_RTP_MAX_REMOTE_CLIENT_DECISIONS_PER_HOST) return false;

        names.push_back(name);
        return true;
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


_Use_decl_annotations_
void
RtpMidiApprovals::SetPolicy(GUID const& hostId, RtpMidiRemoteClientPolicy const policy)
{
    bool signal{ false };

    {
        auto lock = std::scoped_lock{ m_lock };

        auto& host = m_hosts[hostId];
        host.Policy = policy;

        // nothing is waiting on a host which lets everyone in
        if (policy == RtpMidiRemoteClientPolicy::AllowAny && !host.Pending.empty())
        {
            host.Pending.clear();
            signal = true;
        }
    }

    if (signal) m_signal.SignalPendingApprovalChanged();
}


_Use_decl_annotations_
void
RtpMidiApprovals::SetRememberedDecisions(GUID const& hostId, std::vector<std::wstring> const& allowed, std::vector<std::wstring> const& denied)
{
    auto lock = std::scoped_lock{ m_lock };

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
        auto lock = std::scoped_lock{ m_lock };

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
        auto lock = std::scoped_lock{ m_lock };

        // a host on its way out lets nobody new in
        auto const it = m_hosts.find(hostId);
        if (it == m_hosts.end()) return RtpMidi::Admission::Reject;

        auto& host = it->second;
        auto const now = GetTickCount64();

        signal = ExpirePending(host, now);

        auto const pending = std::find_if(host.Pending.begin(), host.Pending.end(),
            [&](Pending const& candidate) { return SameName(candidate.RemoteName, remoteName); });

        if (Contains(host.Denied, remoteName) || Contains(host.DeniedUntilRestart, remoteName))
        {
            admission = RtpMidi::Admission::Reject;
        }
        else if (host.Policy == RtpMidiRemoteClientPolicy::AllowAny ||
            Contains(host.Allowed, remoteName) ||
            Contains(host.AllowedUntilRestart, remoteName))
        {
            admission = RtpMidi::Admission::Accept;
        }
        else if (pending != host.Pending.end())
        {
            switch (pending->State)
            {
            case PendingState::ApprovedOnce:
                host.Pending.erase(pending);
                admission = RtpMidi::Admission::Accept;
                signal = true;
                break;

            case PendingState::DeniedOnce:
                host.Pending.erase(pending);
                admission = RtpMidi::Admission::Reject;
                break;

            default:
                pending->RemoteAddress = remoteAddress;
                pending->ExpiresTick = now + MIDI_RTP_PENDING_REMOTE_CLIENT_LIFETIME_MS;
                admission = RtpMidi::Admission::Hold;
                break;
            }
        }
        else if (remoteName.empty() ||
            remoteName.size() > MIDI_RTP_REMOTE_CLIENT_NAME_MAX_CHARS ||
            host.Pending.size() >= MIDI_RTP_MAX_PENDING_REMOTE_CLIENTS_PER_HOST)
        {
            // a remote which cannot be listed for someone to decide about is refused
            admission = RtpMidi::Admission::Reject;
        }
        else
        {
            Pending added{};
            added.RemoteName = remoteName;
            added.RemoteAddress = remoteAddress;
            added.ExpiresTick = now + MIDI_RTP_PENDING_REMOTE_CLIENT_LIFETIME_MS;
            GetSystemTimePreciseAsFileTime(&added.FirstRequest);

            host.Pending.push_back(std::move(added));

            admission = RtpMidi::Admission::Hold;
            signal = true;
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
        auto lock = std::scoped_lock{ m_lock };

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
            [&](Pending const& candidate) { return SameName(candidate.RemoteName, remoteName); });

        bool const isPending = pending != host.Pending.end();

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
                // answered on its next ask, which is also when a refusal stops it asking
                pending->State = allow ? PendingState::ApprovedOnce : PendingState::DeniedOnce;
                pending->ExpiresTick = now + MIDI_RTP_PENDING_REMOTE_CLIENT_LIFETIME_MS;
                signal = true;
            }
        }
        else
        {
            auto& keep = allow ?
                (scope == RtpMidiApprovalScope::Always ? host.Allowed : host.AllowedUntilRestart) :
                (scope == RtpMidiApprovalScope::Always ? host.Denied : host.DeniedUntilRestart);

            if (!Add(keep, remoteName))
            {
                errorCode = RTP_MIDI_ERROR_CODE_TOO_MANY_REMOTE_CLIENT_DECISIONS;
                if (signal) m_signal.SignalPendingApprovalChanged();
                return E_BOUNDS;
            }

            // the newest decision is the one that holds
            if (allow)
            {
                Erase(host.Denied, remoteName);
                Erase(host.DeniedUntilRestart, remoteName);
                if (scope == RtpMidiApprovalScope::Always) Erase(host.AllowedUntilRestart, remoteName);
            }
            else
            {
                Erase(host.Allowed, remoteName);
                Erase(host.AllowedUntilRestart, remoteName);
                if (scope == RtpMidiApprovalScope::Always) Erase(host.DeniedUntilRestart, remoteName);
            }

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
    auto lock = std::scoped_lock{ m_lock };

    auto const it = m_hosts.find(hostId);
    RETURN_HR_IF(E_NOTFOUND, it == m_hosts.end());

    Erase(it->second.Allowed, remoteName);
    Erase(it->second.Denied, remoteName);
    Erase(it->second.AllowedUntilRestart, remoteName);
    Erase(it->second.DeniedUntilRestart, remoteName);

    return S_OK;
}


_Use_decl_annotations_
bool
RtpMidiApprovals::HasHost(GUID const& hostId)
{
    auto lock = std::scoped_lock{ m_lock };
    return m_hosts.find(hostId) != m_hosts.end();
}


_Use_decl_annotations_
RtpMidiRemoteClientPolicy
RtpMidiApprovals::Policy(GUID const& hostId)
{
    auto lock = std::scoped_lock{ m_lock };

    auto const it = m_hosts.find(hostId);
    return it == m_hosts.end() ? RtpMidiRemoteClientPolicy::AllowAny : it->second.Policy;
}


_Use_decl_annotations_
std::vector<RtpMidiRemoteClientDecision>
RtpMidiApprovals::Decisions(GUID const& hostId)
{
    std::vector<RtpMidiRemoteClientDecision> decisions;

    auto lock = std::scoped_lock{ m_lock };

    auto const it = m_hosts.find(hostId);
    if (it == m_hosts.end()) return decisions;

    auto const append = [&](std::vector<std::wstring> const& names, bool const allowed, bool const untilRestart)
        {
            for (auto const& name : names) decisions.push_back(RtpMidiRemoteClientDecision{ name, allowed, untilRestart });
        };

    append(it->second.Allowed, true, false);
    append(it->second.AllowedUntilRestart, true, true);
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
        auto lock = std::scoped_lock{ m_lock };
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
