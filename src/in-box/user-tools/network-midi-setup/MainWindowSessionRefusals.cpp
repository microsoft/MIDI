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

        std::wstring_view RefusalFormatKey(_In_ midi2net::MidiNetworkRemoteClientRefusalReason const reason) noexcept
        {
            switch (reason)
            {
            case midi2net::MidiNetworkRemoteClientRefusalReason::AlreadyConnectedToThisHost:
                return L"NetworkHostRefusedSecondConnectionFormat";

            case midi2net::MidiNetworkRemoteClientRefusalReason::ThisPCAlreadyConnectedToRemote:
                return L"NetworkHostRefusedRemoteHostFormat";

            default:
                return L"NetworkHostRefusedUnknownReasonFormat";
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

            // The remote ended the session, saying its user did or it was powering down. The
            // service leaves a Failed entry with this code only then, and doesn't try again until
            // the device is back on the network or someone clicks Try again.
            if (client.EntryState() == midi2net::MidiNetworkClientEntryState::Failed &&
                client.LastErrorCode() == midi2net::MidiNetworkClientConnectErrorCode::InvitationEndedByHost)
            {
                return client.ConfiguredDirectAddress().empty() ?
                    res::FormatString(L"NetworkClientDisconnectedByRemoteFormat", name) :
                    res::FormatString(L"NetworkClientDisconnectedByRemoteDirectFormat", name);
            }

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
            std::wstring text{};

            // already newest first
            auto const refusals = host.RecentlyRefusedRemoteClients();

            if (refusals == nullptr)
            {
                return winrt::hstring{};
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

                if (!text.empty())
                {
                    text += L"\n";
                }

                text += res::FormatString(RefusalFormatKey(refusal.Reason()), name, RefusalTimeText(refusal.RefusedTime()));
            }

            return winrt::hstring{ text };
        }
        catch (...)
        {
            return winrt::hstring{};
        }
    }

    _Use_decl_annotations_
    winrt::hstring MainWindow::HostConnectionCountText(uint32_t const count) noexcept
    {
        if (count == 0) return res::GetString(L"HostNoConnections");
        if (count == 1) return res::GetString(L"HostConnectionCountOne");

        return res::FormatString(L"HostConnectionCountFormat", count);
    }

    _Use_decl_annotations_
    uint32_t MainWindow::CountActiveSessions(collections::IVectorView<midi2net::MidiNetworkHostConnection> const& connections) noexcept
    {
        uint32_t count{ 0 };

        try
        {
            if (connections == nullptr) return 0;

            for (auto const& connection : connections)
            {
                if (connection != nullptr && connection.IsSessionActive())
                {
                    count++;
                }
            }
        }
        catch (...)
        {
        }

        return count;
    }
}