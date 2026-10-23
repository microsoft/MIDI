// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================
//
// A device has one Network MIDI 2.0 session with this PC at a time, so the service turns away a
// second one. Without saying why, a client entry just reads "Trying to connect..." and a host shows
// nothing at all. These say which session is in the way.

#include "pch.h"
#include "MainWindow.xaml.h"

#include "StringResources.h"

#include <winrt/Windows.Globalization.DateTimeFormatting.h>

namespace res = ::midinetworksetup::resources;

namespace winrt::midinetworksetup::implementation
{
    namespace
    {
        winrt::hstring HostDisplayName(_In_ midi2net::MidiNetworkConfiguredHost const& host) noexcept
        {
            try
            {
                return host.UmpEndpointName().empty() ? host.ServiceInstanceName() : host.UmpEndpointName();
            }
            catch (...)
            {
                return winrt::hstring{};
            }
        }

        // the name the customer knows the other entry by, as on its card
        winrt::hstring ClientEntryDisplayName(
            _In_ std::unordered_map<std::wstring, midi2net::MidiNetworkSavedClient> const& savedClients,
            _In_ midi2net::MidiNetworkConfiguredClient const& client) noexcept
        {
            try
            {
                for (auto const& [key, saved] : savedClients)
                {
                    if (saved == nullptr || saved.ClientId() != client.ClientId())
                    {
                        continue;
                    }

                    if (!saved.CustomEndpointName().empty())
                    {
                        return saved.CustomEndpointName();
                    }

                    if (auto const criteria = saved.MatchCriteria(); criteria != nullptr && !criteria.UmpEndpointName().empty())
                    {
                        return criteria.UmpEndpointName();
                    }
                }

                if (!client.ConfiguredDirectAddress().empty())
                {
                    return res::FormatString(
                        L"HostAddressValueFormat",
                        client.ConfiguredDirectAddress(),
                        client.ConfiguredDirectPort());
                }
            }
            catch (...)
            {
            }

            return winrt::hstring{};
        }

        winrt::hstring RefusalTimeText(_In_ foundation::DateTime const& time) noexcept
        {
            try
            {
                winrt::Windows::Globalization::DateTimeFormatting::DateTimeFormatter formatter{ L"shorttime" };

                return formatter.Format(time);
            }
            catch (...)
            {
                return winrt::hstring{};
            }
        }

        void AppendRefusals(
            _Inout_ std::vector<std::pair<int64_t, winrt::hstring>>& lines,
            _In_ collections::IVectorView<midi2net::MidiNetworkPendingRemoteClient> const& refusals,
            _In_ std::wstring_view formatKey) noexcept
        {
            try
            {
                if (refusals == nullptr)
                {
                    return;
                }

                for (auto const& refusal : refusals)
                {
                    if (refusal == nullptr)
                    {
                        continue;
                    }

                    auto const name = refusal.UmpEndpointName().empty() ?
                        res::GetString(L"UnnamedDevice") :
                        refusal.UmpEndpointName();

                    lines.emplace_back(
                        refusal.RequestTime().time_since_epoch().count(),
                        res::FormatString(formatKey, name, RefusalTimeText(refusal.RequestTime())));
                }
            }
            catch (...)
            {
            }
        }
    }

    _Use_decl_annotations_
    winrt::hstring MainWindow::DescribeNetworkClientWaiting(
        ServiceSnapshot const& snapshot,
        midi2net::MidiNetworkConfiguredClient const& client,
        winrt::hstring const& remoteName) noexcept
    {
        try
        {
            auto const name = remoteName.empty() ? res::GetString(L"UnnamedDevice") : remoteName;

            if (auto const hostId = client.WaitingForHostId(); hostId != winrt::guid{})
            {
                winrt::hstring hostName{};

                if (snapshot.ConfiguredHosts != nullptr)
                {
                    for (auto const& host : snapshot.ConfiguredHosts)
                    {
                        if (host != nullptr && host.HostId() == hostId)
                        {
                            hostName = HostDisplayName(host);
                            break;
                        }
                    }
                }

                return hostName.empty() ?
                    res::FormatString(L"NetworkClientWaitingForUnnamedHostFormat", name) :
                    res::FormatString(L"NetworkClientWaitingForHostFormat", name, hostName);
            }

            if (auto const clientId = client.WaitingForClientId(); clientId != winrt::guid{})
            {
                winrt::hstring otherName{};

                if (snapshot.ConfiguredClients != nullptr)
                {
                    for (auto const& other : snapshot.ConfiguredClients)
                    {
                        if (other != nullptr && other.ClientId() == clientId)
                        {
                            otherName = ClientEntryDisplayName(snapshot.SavedClients, other);
                            break;
                        }
                    }
                }

                return otherName.empty() ?
                    res::FormatString(L"NetworkClientWaitingForUnnamedClientFormat", name) :
                    res::FormatString(L"NetworkClientWaitingForClientFormat", name, otherName);
            }
        }
        catch (...)
        {
        }

        return winrt::hstring{};
    }

    _Use_decl_annotations_
    winrt::hstring MainWindow::DescribeNetworkHostRefusals(midi2net::MidiNetworkConfiguredHost const& host) noexcept
    {
        try
        {
            std::vector<std::pair<int64_t, winrt::hstring>> lines{};

            AppendRefusals(lines, host.RecentlyRefusedSecondConnections(), L"NetworkHostRefusedSecondConnectionFormat");
            AppendRefusals(lines, host.RecentlyRefusedRemoteHosts(), L"NetworkHostRefusedRemoteHostFormat");

            // newest first, across both lists
            std::stable_sort(lines.begin(), lines.end(),
                [](auto const& left, auto const& right) { return left.first > right.first; });

            std::wstring text{};

            for (auto const& line : lines)
            {
                if (!text.empty())
                {
                    text += L"\n";
                }

                text += line.second;
            }

            return winrt::hstring{ text };
        }
        catch (...)
        {
            return winrt::hstring{};
        }
    }
}
