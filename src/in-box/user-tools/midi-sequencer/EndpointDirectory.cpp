// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "EndpointDirectory.h"

namespace midisequencer
{
    namespace
    {
        bool SameId(std::wstring const& left, std::wstring const& right) noexcept
        {
            return midiapp::EndpointIdsMatch(winrt::hstring{ left }, winrt::hstring{ right });
        }

        // What a destination group speaks, from what the endpoint says about itself.
        std::array<bool, 16> GroupProtocols(midi2enum::MidiEndpointDeviceInformation const& device)
        {
            std::array<bool, 16> midi2{};
            midi2.fill(true);

            try
            {
                // A MIDI 1.0 byte stream device gets MIDI 1.0 whatever else it says.
                auto const transport = device.GetTransportSuppliedInfo();

                if (transport != nullptr && transport.NativeDataFormat() == midi2enum::MidiEndpointNativeDataFormat::Midi1ByteFormat)
                {
                    midi2.fill(false);
                    return midi2;
                }

                // An endpoint that settled on MIDI 1.0 gets MIDI 1.0 on every group.
                auto const stream = device.GetDeclaredStreamConfiguration();

                if (stream != nullptr && stream.Protocol() == midi2enum::MidiProtocol::Midi1)
                {
                    midi2.fill(false);
                }

                auto const blocks = device.GetDeclaredFunctionBlocks();

                if (blocks != nullptr && blocks.Size() > 0)
                {
                    for (auto const& block : blocks)
                    {
                        if (!block.IsActive() || block.Direction() == midi2enum::MidiFunctionBlockDirection::BlockOutput)
                        {
                            continue;
                        }

                        auto const midi1 = block.RepresentsMidi10Connection() != midi2enum::MidiFunctionBlockRepresentsMidi10Connection::Not10;

                        for (uint8_t group = 0; group < 16; ++group)
                        {
                            if (block.IncludesGroup(midi2::MidiGroup{ group }) && midi1)
                            {
                                midi2[group] = false;
                            }
                        }
                    }

                    return midi2;
                }

                // No function blocks: the group terminal blocks say it.
                auto const terminals = device.GetGroupTerminalBlocks();

                if (terminals != nullptr)
                {
                    for (auto const& terminal : terminals)
                    {
                        if (terminal.Direction() == midi2enum::MidiGroupTerminalBlockDirection::BlockOutput)
                        {
                            continue;
                        }

                        auto const protocol = terminal.Protocol();
                        auto const midi1 =
                            protocol == midi2enum::MidiGroupTerminalBlockProtocol::Midi1Message64 ||
                            protocol == midi2enum::MidiGroupTerminalBlockProtocol::Midi1Message64WithJitterReduction ||
                            protocol == midi2enum::MidiGroupTerminalBlockProtocol::Midi1Message128 ||
                            protocol == midi2enum::MidiGroupTerminalBlockProtocol::Midi1Message128WithJitterReduction;

                        for (uint8_t group = 0; group < 16; ++group)
                        {
                            if (terminal.IncludesGroup(midi2::MidiGroup{ group }) && midi1)
                            {
                                midi2[group] = false;
                            }
                        }
                    }
                }
            }
            catch (...)
            {
            }

            return midi2;
        }
    }

    _Use_decl_annotations_
    EndpointRef EndpointDirectory::MakeRef(midiapp::LiveEndpoint const& endpoint)
    {
        return EndpointRef{ endpoint.Name, endpoint.EndpointDeviceId };
    }

    _Use_decl_annotations_
    void EndpointDirectory::Refresh(std::vector<std::wstring> const& detail)
    {
        auto const live = midiapp::EndpointCatalog::Current().Snapshot();

        std::vector<EndpointSummary> endpoints{};
        endpoints.reserve(live.size());

        std::unordered_map<std::wstring, std::array<bool, 16>> known{};

        {
            std::scoped_lock guard{ m_lock };

            for (auto const& endpoint : m_endpoints)
            {
                if (endpoint.Detailed)
                {
                    known.emplace(endpoint.Live.EndpointDeviceId, endpoint.GroupSpeaksMidi2);
                }
            }
        }

        for (auto const& endpoint : live)
        {
            EndpointSummary summary{};
            summary.Live = endpoint;
            summary.GroupSpeaksMidi2.fill(endpoint.SupportsMidi2Protocol);

            auto const wanted = std::find_if(detail.begin(), detail.end(), [&](std::wstring const& id)
            {
                return SameId(id, endpoint.EndpointDeviceId);
            }) != detail.end();

            if (wanted)
            {
                try
                {
                    auto const device = midi2enum::MidiEndpointDeviceInformation::CreateFromEndpointDeviceId(endpoint.EndpointDeviceId);

                    if (device != nullptr)
                    {
                        summary.GroupSpeaksMidi2 = GroupProtocols(device);
                        summary.Detailed = true;
                    }
                }
                catch (...)
                {
                }
            }
            else if (auto found = known.find(endpoint.EndpointDeviceId); found != known.end())
            {
                summary.GroupSpeaksMidi2 = found->second;
                summary.Detailed = true;
            }

            endpoints.push_back(std::move(summary));
        }

        std::wstring synthId{};

        try
        {
            if (midi2synth::MidiSynthManager::IsTransportAvailable())
            {
                synthId = std::wstring{ midi2synth::MidiSynthManager::EndpointDeviceId() };
            }
        }
        catch (...)
        {
        }

        std::scoped_lock guard{ m_lock };
        m_endpoints = std::move(endpoints);
        m_synthId = std::move(synthId);
    }

    std::vector<EndpointSummary> EndpointDirectory::Snapshot() const
    {
        std::scoped_lock guard{ m_lock };
        return m_endpoints;
    }

    _Use_decl_annotations_
    std::optional<EndpointSummary> EndpointDirectory::Resolve(EndpointRef const& endpoint) const
    {
        if (endpoint.IsEmpty())
        {
            return std::nullopt;
        }

        std::scoped_lock guard{ m_lock };

        if (!endpoint.Id.empty())
        {
            for (auto const& summary : m_endpoints)
            {
                if (SameId(summary.Live.EndpointDeviceId, endpoint.Id))
                {
                    return summary;
                }
            }
        }

        if (!endpoint.Name.empty())
        {
            for (auto const& summary : m_endpoints)
            {
                if (summary.Live.Name == endpoint.Name)
                {
                    return summary;
                }
            }
        }

        return std::nullopt;
    }

    _Use_decl_annotations_
    std::wstring EndpointDirectory::ResolveId(EndpointRef const& endpoint) const
    {
        auto const found = Resolve(endpoint);
        return found.has_value() ? found->Live.EndpointDeviceId : std::wstring{};
    }

    _Use_decl_annotations_
    DestinationInfo EndpointDirectory::Lookup(EndpointRef const& endpoint, uint8_t group) const
    {
        DestinationInfo info{};
        auto const found = Resolve(endpoint);

        if (found.has_value())
        {
            info.SpeaksMidi2 = found->GroupSpeaksMidi2[group & 0x0F];
        }

        // How early each device is sent to is left to the service (design section 8).
        info.OffsetMicroseconds = 0;
        return info;
    }

    std::wstring EndpointDirectory::SynthEndpointId() const
    {
        std::scoped_lock guard{ m_lock };
        return m_synthId;
    }
}
