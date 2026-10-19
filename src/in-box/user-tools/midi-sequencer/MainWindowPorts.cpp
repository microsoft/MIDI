// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Groups turned off in Studio setup, and what that stops. The sequencer sends nothing to a group
// it doesn't play to, and ignores what arrives on a group it doesn't record from. A device whose
// every group in use is turned off isn't even opened. That's for a device that stops working when
// an app talks to it: it can stay plugged in (design section 25).

#include "pch.h"
#include "MainWindow.xaml.h"

#include "StudioSetup.h"

namespace winrt::midisequencer::implementation
{
    std::vector<seq::PortSwitch> MainWindow::SavedPortSwitches() const
    {
        std::vector<seq::PortSwitch> ports{};

        for (auto const& port : seq::AppSettings::Current().PortsOff())
        {
            ports.push_back(seq::PortSwitch{ seq::EndpointRef{ port.EndpointName, port.EndpointId }, port.Group, port.PlayTo, port.RecordFrom });
        }

        return ports;
    }

    _Use_decl_annotations_
    uint16_t MainWindow::GroupsTurnedOff(seq::EndpointRef const& endpoint, bool playTo) const
    {
        if (endpoint.IsEmpty() || seq::AppSettings::Current().PortsOff().empty())
        {
            return 0;
        }

        return seq::GroupsOff(SavedPortSwitches(), endpoint, playTo, [this](seq::EndpointRef const& other) { return EndpointKeyOf(other); });
    }

    _Use_decl_annotations_
    uint16_t MainWindow::SourceGroupMask(seq::EndpointRef const& endpoint) const
    {
        uint16_t groups{ 0 };

        if (endpoint.IsEmpty())
        {
            return groups;
        }

        if (auto const summary = m_directory->Resolve(endpoint); summary.has_value())
        {
            for (size_t group = 0; group < 16 && group < summary->Live.SourceGroups.size(); ++group)
            {
                if (summary->Live.SourceGroups[group])
                {
                    groups = static_cast<uint16_t>(groups | (1u << group));
                }
            }
        }

        return groups;
    }

    _Use_decl_annotations_
    uint16_t MainWindow::AnySourceGroups(seq::EndpointRef const& endpoint) const
    {
        // Each group the device sends on, or the first when that isn't known, the way Studio
        // setup lists them.
        auto const groups = SourceGroupMask(endpoint);
        return groups != 0 ? groups : uint16_t{ 1 };
    }

    _Use_decl_annotations_
    bool MainWindow::DestinationTurnedOff(seq::TrackDestination const& destination) const
    {
        return !destination.Endpoint.IsEmpty() &&
            (GroupsTurnedOff(destination.Endpoint, true) & (1u << (destination.Group & 0x0F))) != 0;
    }

    _Use_decl_annotations_
    bool MainWindow::SourceTurnedOff(seq::TrackSource const& source) const
    {
        if (source.Endpoint.IsEmpty())
        {
            return false;
        }

        auto const off = GroupsTurnedOff(source.Endpoint, false);

        if (off == 0)
        {
            return false;
        }

        // A track that records from any group still hears the groups left on.
        auto const groups = source.Group >= 0 ? static_cast<uint16_t>(1u << (source.Group & 0x0F)) : AnySourceGroups(source.Endpoint);
        return (groups & ~off & 0xFFFF) == 0;
    }

    _Use_decl_annotations_
    void MainWindow::LeaveOutGroupsOff(
        std::vector<seq::EndpointRef>& destinations,
        std::map<std::pair<std::wstring, std::wstring>, uint16_t> const& sendsTo,
        std::vector<std::wstring>& sources,
        std::map<std::wstring, uint16_t> const& listensTo)
    {
        std::map<std::pair<std::wstring, std::wstring>, uint16_t> destinationsOff{};
        std::map<std::wstring, uint16_t> sourcesOff{};
        auto const ports = SavedPortSwitches();

        if (!ports.empty())
        {
            auto const key = [this](seq::EndpointRef const& endpoint) { return EndpointKeyOf(endpoint); };

            // A device isn't opened at all when every group it's used on is turned off.
            std::erase_if(destinations, [&](seq::EndpointRef const& endpoint)
            {
                auto const off = seq::GroupsOff(ports, endpoint, true, key);

                if (off == 0)
                {
                    return false;
                }

                auto const pair = std::make_pair(endpoint.Name, endpoint.Id);
                destinationsOff.emplace(pair, off);

                auto const used = sendsTo.find(pair);
                return used != sendsTo.end() && (used->second & ~off & 0xFFFF) == 0;
            });

            std::erase_if(sources, [&](std::wstring const& id)
            {
                auto const off = seq::GroupsOff(ports, seq::EndpointRef{ std::wstring{}, id }, false, key);

                if (off == 0)
                {
                    return false;
                }

                sourcesOff.emplace(id, off);

                auto const used = listensTo.find(id);
                return used != listensTo.end() && (used->second & ~off & 0xFFFF) == 0;
            });
        }

        // Before the connections open, so nothing slips through while they do.
        if (m_output != nullptr)
        {
            m_output->SetGroupsOff(std::move(destinationsOff));
        }

        if (m_sources != nullptr)
        {
            m_sources->SetGroupsOff(std::move(sourcesOff));
        }
    }
}
