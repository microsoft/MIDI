// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"

namespace
{
    namespace midi2net = ::winrt::Windows::Devices::Midi2::Transports::Network;

    std::wstring LoadAppString(_In_ UINT const id) noexcept
    {
        wchar_t buffer[512]{ };

        auto const length = ::LoadStringW(::GetModuleHandleW(nullptr), id, buffer, ARRAYSIZE(buffer));

        return length > 0 ? std::wstring{ buffer, static_cast<size_t>(length) } : std::wstring{ };
    }

    std::wstring FormatAppString(_In_ UINT const id, _In_ std::wstring const& argument) noexcept
    {
        auto const format = LoadAppString(id);

        if (format.empty())
        {
            return argument;
        }

        wchar_t buffer[1024]{ };

        // The format strings are ours, from the resource file. The argument has already been
        // sanitized, and swprintf_s truncates rather than overflowing.
        if (::swprintf_s(buffer, ARRAYSIZE(buffer), format.c_str(), argument.c_str()) < 0)
        {
            return argument;
        }

        return std::wstring{ buffer };
    }

    std::wstring LowerCopy(_In_ std::wstring const& value) noexcept
    {
        std::wstring result{ value };

        for (auto& c : result)
        {
            c = static_cast<wchar_t>(::towlower(c));
        }

        return result;
    }
}

void NetworkApprovalNotifier::Evaluate() noexcept
{
    if (!AppSettings::NotificationsEnabled() || !AppSettings::NetworkApprovalNotificationsEnabled())
    {
        // Turned off while a banner was up. Withdrawing it is part of honoring the setting.
        m_toast.Remove(ToastTag);
        m_reportedIdentities.clear();

        return;
    }

    auto const pending = ReadPendingClients();

    if (pending.empty())
    {
        // Everything was decided, withdrawn, or the remotes gave up. Nothing is waiting, so a
        // banner saying otherwise would be a lie by the time they read it.
        m_toast.Remove(ToastTag);
        m_reportedIdentities.clear();

        return;
    }

    Notify(pending);
}

void NetworkApprovalNotifier::Notify(std::vector<PendingClient> const& pending) noexcept
{
    bool anythingNew{ false };

    for (auto const& client : pending)
    {
        if (!m_reportedIdentities.contains(client.Identity))
        {
            anythingNew = true;
            break;
        }
    }

    if (!anythingNew)
    {
        return;
    }

    auto const now = std::chrono::steady_clock::now();

    if (m_lastToast.time_since_epoch().count() != 0 && now - m_lastToast < MinimumIntervalBetweenToasts)
    {
        // Deliberately not queued for later. The next change re-evaluates, and the banner which
        // is already up says something is waiting, which is the whole of the message.
        return;
    }

    std::wstring body{ };

    if (pending.size() == 1)
    {
        body = FormatAppString(IDS_NOTIFICATION_NETWORK_APPROVAL_ONE, pending.front().DisplayName);
    }
    else
    {
        body = FormatAppString(IDS_NOTIFICATION_NETWORK_APPROVAL_MANY, std::to_wstring(pending.size()));
    }

    auto const result = m_toast.ShowOrReplace(
        ToastTag,
        LoadAppString(IDS_NOTIFICATION_TITLE),
        body,
        LoadAppString(IDS_NOTIFICATION_NETWORK_ATTRIBUTION),
        LoadAppString(IDS_NOTIFICATION_REVIEW_BUTTON),
        MIDI_NETWORK_SETUP_PROTOCOL_URI_PENDING);

    if (FAILED(result))
    {
        LOG_IF_FAILED(result);
        return;
    }

    m_lastToast = now;

    m_reportedIdentities.clear();

    for (auto const& client : pending)
    {
        m_reportedIdentities.insert(client.Identity);
    }
}

std::vector<NetworkApprovalNotifier::PendingClient> NetworkApprovalNotifier::ReadPendingClients() const noexcept
{
    std::vector<PendingClient> result{ };

    try
    {
        if (!midi2net::MidiNetworkTransportManager::IsTransportAvailable())
        {
            return result;
        }

        auto const pending = midi2net::MidiNetworkTransportManager::GetPendingRemoteClients();

        if (pending == nullptr)
        {
            return result;
        }

        for (auto const& client : pending)
        {
            if (client == nullptr)
            {
                continue;
            }

            std::wstring const name{ client.UmpEndpointName() };
            std::wstring const productInstanceId{ client.ProductInstanceId() };

            if (name.empty() && productInstanceId.empty())
            {
                continue;
            }

            PendingClient entry{ };

            // The service keys a remote on the pair, compared without case, so this has to agree
            // with it or the same client would look new every time it re-invited.
            entry.Identity = LowerCopy(name) + L"\u0001" + LowerCopy(productInstanceId);

            entry.DisplayName = ToastSender::SanitizeForToast(
                name.empty() ? productInstanceId : name);

            result.push_back(std::move(entry));
        }
    }
    catch (...)
    {
        // The service may have stopped between the hint and this call, which is ordinary rather
        // than exceptional. Reporting nothing pending is the honest answer.
        LOG_CAUGHT_EXCEPTION();

        result.clear();
    }

    return result;
}
