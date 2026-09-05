// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"

#include "ImportCandidates.h"

namespace midiloopbacksetup
{
    namespace
    {
        // Driver services which ship with Windows. A MIDI port behind anything else was put
        // there by software the customer installed, and that is what this tool offers to
        // replace. An empty service name means a software device Windows MIDI Services created
        // itself, so those are skipped as well.
        constexpr std::wstring_view InBoxDriverServices[]
        {
            L"usbaudio",        // USB Audio 1.0 class driver, which carries USB MIDI 1.0
            L"usbaudio2",       // USB Audio 2.0 class driver
            L"usbmidi2",        // the Windows USB MIDI 2.0 class driver
            L"mskssrv",         // Microsoft streaming service proxy
        };

        std::wstring Lowered(_In_ winrt::hstring const& value) noexcept
        {
            std::wstring result{ value };

            std::transform(result.begin(), result.end(), result.begin(),
                [](wchar_t const c) { return static_cast<wchar_t>(::towlower(c)); });

            return result;
        }

        bool IsInBoxDriverService(_In_ std::wstring const& serviceName) noexcept
        {
            if (serviceName.empty())
            {
                return true;
            }

            return std::any_of(
                std::begin(InBoxDriverServices),
                std::end(InBoxDriverServices),
                [&serviceName](std::wstring_view const known) { return known == serviceName; });
        }

        // Both directions of one group on one endpoint, gathered before deciding whether the
        // pair is complete. A source with no matching destination cannot be a loopback.
        struct PortPairBuilder
        {
            std::wstring SourceName{};
            std::wstring DestinationName{};
            bool HasSource{ false };
            bool HasDestination{ false };
        };

        struct DeviceBuilder
        {
            std::wstring DeviceName{};
            std::wstring DeviceInstanceId{};
            std::wstring ServiceName{};

            // keyed on endpoint id then group index, because a device split across several
            // endpoints starts counting groups again in each one
            std::map<std::wstring, std::map<uint8_t, PortPairBuilder>> Pairs{};
        };
    }

    std::vector<ImportCandidateDevice> FindImportCandidates() noexcept
    {
        std::vector<ImportCandidateDevice> results{};

        try
        {
            auto const ports = midi2legacy::MidiLegacyPortDeviceInformation::FindAll();

            if (ports == nullptr)
            {
                return results;
            }

            std::map<std::wstring, DeviceBuilder> devices{};

            for (auto const& port : ports)
            {
                if (port == nullptr)
                {
                    continue;
                }

                // The enumerator also returns ports Windows MIDI Services does not own - the GS
                // Wavetable Synth and the older Bluetooth MIDI ports. Those are in-box Windows
                // features rather than something a customer installed, and they have no
                // endpoint behind them to read a group from.
                auto const endpointDeviceId = std::wstring{ port.AssociatedEndpointDeviceId() };

                if (endpointDeviceId.empty())
                {
                    continue;
                }

                auto const parent = port.GetParentDeviceInformation();

                if (parent == nullptr)
                {
                    continue;
                }

                auto const serviceName = Lowered(parent.ServiceName());

                if (IsInBoxDriverService(serviceName))
                {
                    continue;
                }

                auto const parentId = std::wstring{ parent.Id() };

                if (parentId.empty())
                {
                    continue;
                }

                auto& device = devices[parentId];

                if (device.DeviceInstanceId.empty())
                {
                    device.DeviceInstanceId = parentId;
                    device.DeviceName = std::wstring{ parent.Name() };
                    device.ServiceName = std::wstring{ parent.ServiceName() };
                }

                auto const group = port.Group();

                if (group == nullptr)
                {
                    continue;
                }

                auto& pair = device.Pairs[endpointDeviceId][group.Index()];

                if (port.Flow() == midi2enum::Midi1PortFlow::MidiMessageSource)
                {
                    pair.SourceName = std::wstring{ port.Name() };
                    pair.HasSource = true;
                }
                else
                {
                    pair.DestinationName = std::wstring{ port.Name() };
                    pair.HasDestination = true;
                }
            }

            for (auto const& [parentId, device] : devices)
            {
                ImportCandidateDevice candidate{};

                candidate.DeviceName = device.DeviceName;
                candidate.DeviceInstanceId = device.DeviceInstanceId;
                candidate.ServiceName = device.ServiceName;

                for (auto const& [endpointDeviceId, groups] : device.Pairs)
                {
                    for (auto const& [groupIndex, pair] : groups)
                    {
                        if (!pair.HasSource || !pair.HasDestination)
                        {
                            continue;
                        }

                        ImportCandidatePort port{};

                        port.SourceName = pair.SourceName;
                        port.DestinationName = pair.DestinationName;
                        port.GroupNumber = static_cast<uint8_t>(groupIndex + 1);
                        port.EndpointDeviceId = endpointDeviceId;

                        candidate.Ports.push_back(port);
                    }
                }

                if (!candidate.Ports.empty())
                {
                    results.push_back(candidate);
                }
            }

            std::sort(results.begin(), results.end(),
                [](ImportCandidateDevice const& left, ImportCandidateDevice const& right)
                {
                    return ::CompareStringOrdinal(
                        left.DeviceName.c_str(), -1, right.DeviceName.c_str(), -1, TRUE) == CSTR_LESS_THAN;
                });
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
        }

        return results;
    }
}
