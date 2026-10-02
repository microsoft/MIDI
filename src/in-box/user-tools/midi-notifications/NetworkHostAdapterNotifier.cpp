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
    namespace midi2rtp = ::winrt::Windows::Devices::Midi2::Transports::Rtp;

    std::wstring LoadAppString(_In_ UINT const id) noexcept
    {
        wchar_t buffer[512]{ };

        auto const length = ::LoadStringW(::GetModuleHandleW(nullptr), id, buffer, ARRAYSIZE(buffer));

        return length > 0 ? std::wstring{ buffer, static_cast<size_t>(length) } : std::wstring{ };
    }

    std::wstring FormatAppString(
        _In_ UINT const id,
        _In_ std::wstring const& first,
        _In_ std::wstring const& second) noexcept
    {
        auto const format = LoadAppString(id);

        if (format.empty())
        {
            return first;
        }

        wchar_t buffer[2048]{ };

        // The format strings are ours, from the resource file. The arguments have already been
        // sanitized, which also caps their length well below the size of the buffer.
        if (::swprintf_s(buffer, ARRAYSIZE(buffer), format.c_str(), first.c_str(), second.c_str()) < 0)
        {
            return first;
        }

        return std::wstring{ buffer };
    }

    std::wstring SanitizedOr(_In_ winrt::hstring const& value, _In_ UINT const fallbackId) noexcept
    {
        return value.empty() ?
            LoadAppString(fallbackId) :
            ToastSender::SanitizeForToast(std::wstring{ value });
    }

    // one banner per transport, so each can be replaced or withdrawn on its own
    std::wstring ToastTagFor(_In_ NetworkHostAdapterNotifier::Transport const transport) noexcept
    {
        return transport == NetworkHostAdapterNotifier::Transport::RtpMidi ?
            L"rtp-host-adapter" :
            L"network-host-adapter";
    }
}

void NetworkHostAdapterNotifier::Evaluate() noexcept
{
    if (!AppSettings::NotificationsEnabled() || !AppSettings::NetworkHostAdapterNotificationsEnabled())
    {
        // Turned off while a banner was up. Withdrawing it is part of honoring the setting.
        m_toast.Remove(ToastTagFor(m_transport));
        m_reportedIdentities.clear();

        return;
    }

    auto const waiting = ReadWaitingHosts();

    if (waiting.empty())
    {
        // The adapter is back, or another one was chosen
        m_toast.Remove(ToastTagFor(m_transport));
        m_reportedIdentities.clear();

        return;
    }

    // A host which stopped waiting and then waits again is news
    std::erase_if(m_reportedIdentities, [&waiting](std::wstring const& identity)
        {
            return std::none_of(waiting.begin(), waiting.end(),
                [&identity](WaitingHost const& host) { return host.Identity == identity; });
        });

    Notify(waiting);
}

_Use_decl_annotations_
void NetworkHostAdapterNotifier::Notify(std::vector<WaitingHost> const& waiting) noexcept
{
    auto const anythingNew = std::any_of(waiting.begin(), waiting.end(),
        [this](WaitingHost const& host) { return !m_reportedIdentities.contains(host.Identity); });

    if (!anythingNew)
    {
        return;
    }

    auto const now = std::chrono::steady_clock::now();

    if (m_lastToast.time_since_epoch().count() != 0 && now - m_lastToast < MinimumIntervalBetweenToasts)
    {
        // Not queued for later. The next change looks again.
        return;
    }

    auto const isRtp = m_transport == Transport::RtpMidi;
    auto const one = waiting.size() == 1;

    auto const body = one ?
        FormatAppString(IDS_NOTIFICATION_HOST_ADAPTER_ONE, waiting.front().DisplayName, waiting.front().AdapterName) :
        FormatAppString(IDS_NOTIFICATION_HOST_ADAPTER_MANY, std::to_wstring(waiting.size()), std::wstring{ });

    auto const title = isRtp ?
        (one ? IDS_NOTIFICATION_RTP_HOST_ADAPTER_TITLE : IDS_NOTIFICATION_RTP_HOST_ADAPTER_TITLE_MANY) :
        (one ? IDS_NOTIFICATION_NETWORK_HOST_ADAPTER_TITLE : IDS_NOTIFICATION_NETWORK_HOST_ADAPTER_TITLE_MANY);

    auto const result = m_toast.ShowOrReplace(
        ToastTagFor(m_transport),
        LoadAppString(title),
        body,
        LoadAppString(isRtp ? IDS_NOTIFICATION_RTP_ATTRIBUTION : IDS_NOTIFICATION_NETWORK_ATTRIBUTION),
        LoadAppString(IDS_NOTIFICATION_REVIEW_BUTTON),
        isRtp ? MIDI_NETWORK_SETUP_PROTOCOL_URI_RTP_HOSTS : MIDI_NETWORK_SETUP_PROTOCOL_URI_HOSTS);

    if (FAILED(result))
    {
        LOG_IF_FAILED(result);
        return;
    }

    m_lastToast = now;

    for (auto const& host : waiting)
    {
        m_reportedIdentities.insert(host.Identity);
    }
}

std::vector<NetworkHostAdapterNotifier::WaitingHost> NetworkHostAdapterNotifier::ReadWaitingHosts() const noexcept
{
    std::vector<WaitingHost> result{ };

    try
    {
        // Missing and not started is waiting. Missing and started is falling back, which works.
        if (m_transport == Transport::NetworkMidi2)
        {
            if (!midi2net::MidiNetworkTransportManager::IsTransportAvailable())
            {
                return result;
            }

            for (auto const& host : midi2net::MidiNetworkTransportManager::GetConfiguredHosts())
            {
                if (host == nullptr || host.HasStarted() || !host.IsNetworkAdapterMissing())
                {
                    continue;
                }

                result.push_back(WaitingHost{
                    std::wstring{ winrt::to_hstring(host.HostId()) },
                    SanitizedOr(host.UmpEndpointName().empty() ? host.ServiceInstanceName() : host.UmpEndpointName(), IDS_NOTIFICATION_HOST_UNNAMED),
                    SanitizedOr(host.NetworkAdapterName(), IDS_NOTIFICATION_ADAPTER_UNNAMED) });
            }
        }
        else
        {
            // An SDK from before RTP-MIDI throws here, which is the same as nothing waiting
            if (!midi2rtp::MidiRtpTransportManager::IsTransportAvailable())
            {
                return result;
            }

            for (auto const& host : midi2rtp::MidiRtpTransportManager::GetConfiguredHosts())
            {
                if (host == nullptr || host.HasStarted() || !host.IsNetworkAdapterMissing())
                {
                    continue;
                }

                auto const name = !host.Name().empty() ? host.Name() :
                    !host.ActualServiceInstanceName().empty() ? host.ActualServiceInstanceName() :
                    host.ServiceInstanceName();

                result.push_back(WaitingHost{
                    std::wstring{ winrt::to_hstring(host.HostId()) },
                    SanitizedOr(name, IDS_NOTIFICATION_HOST_UNNAMED),
                    SanitizedOr(host.NetworkAdapterName(), IDS_NOTIFICATION_ADAPTER_UNNAMED) });
            }
        }
    }
    catch (...)
    {
        // The service may have stopped between the hint and this call. Reporting nothing waiting
        // is the honest answer.
        LOG_CAUGHT_EXCEPTION();

        result.clear();
    }

    return result;
}
