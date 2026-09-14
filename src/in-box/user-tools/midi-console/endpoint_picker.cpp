// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"

#include "console_output.h"
#include "endpoint_picker.h"
#include "endpoint_utility.h"
#include "midi_formatting.h"
#include "pickers.h"
#include "strings.h"

namespace midi2console
{
    EndpointPickerResult PickEndpoint(_In_ std::string_view prompt)
    {
        EndpointPickerResult result;

        auto const endpoints = EnumerateEndpoints(BuildEndpointFilters(true, false));

        if (endpoints == nullptr || endpoints.Size() == 0)
        {
            WriteErrorLine(ResourceString(IDS_ERROR_NO_ENDPOINTS_FOUND));
            return result;
        }

        std::vector<PickerEntry> entries;

        const std::string supportsMidi2Indicator{ ResourceString(IDS_LABEL_MIDI2_PROTOCOL) };

        bool anyMidi2{ false };

        for (auto const& endpoint : endpoints)
        {
            PickerEntry entry;

            entry.Icon = GetEndpointIcon(endpoint);
            entry.PrimaryText = ToUtf8(endpoint.Name());
            entry.Value = ToUtf8(endpoint.EndpointDeviceId());

            auto const transportInfo = endpoint.GetTransportSuppliedInfo();

            entry.SecondaryText = ToUtf8(transportInfo.TransportCode());

            auto const manufacturerName = ToUtf8(transportInfo.ManufacturerName());

            if (manufacturerName != "Microsoft" && !manufacturerName.empty())
            {
                entry.QuaternaryText = manufacturerName;
            }

            if (endpoint.GetDeclaredEndpointInfo().SupportsMidi20Protocol())
            {
                entry.TertiaryText = supportsMidi2Indicator;
                anyMidi2 = true;
            }

            entries.push_back(std::move(entry));
        }

        if (!anyMidi2)
        {
            for (auto& entry : entries)
            {
                entry.TertiaryText.clear();
            }
        }

        FinalizePickerEntries(entries);

        auto const picked = ShowPicker(prompt, std::move(entries));

        result.Canceled = picked.Canceled;
        result.EndpointDeviceId = picked.Value;
        result.EndpointName = picked.DisplayText;

        return result;
    }

    bool ResolveEndpointDeviceId(_Inout_ std::string& endpointDeviceId, _Out_ std::string& endpointName)
    {
        endpointName.clear();

        if (!endpointDeviceId.empty())
        {
            endpointName = GetEndpointNameFromEndpointDeviceId(endpointDeviceId);
            return true;
        }

        if (!CanShowInteractiveUI())
        {
            WriteErrorLine(ResourceString(IDS_ERROR_NO_INTERACTIVE_CONSOLE));
            return false;
        }

        auto const picked = PickEndpoint(ResourceString(IDS_PROMPT_SELECT_ENDPOINT));

        if (picked.Canceled)
        {
            WriteWarningLine(ResourceString(IDS_STATUS_CANCELED));
            return false;
        }

        endpointDeviceId = picked.EndpointDeviceId;
        endpointName = picked.EndpointName;

        return true;
    }

    namespace
    {
        constexpr uint8_t GroupCount = 16;

        // What each group is called, indexed by group index. An empty entry means the endpoint
        // never claimed that group for this direction.
        using GroupLabels = std::array<std::string, GroupCount>;

        void ClaimGroups(
            _Inout_ GroupLabels& labels,
            _In_ uint8_t firstGroupIndex,
            _In_ uint8_t groupCount,
            _In_ std::string const& name)
        {
            for (uint8_t offset = 0; offset < groupCount; offset++)
            {
                auto const index = static_cast<uint8_t>(firstGroupIndex + offset);

                if (index < GroupCount && labels[index].empty())
                {
                    labels[index] = name;
                }
            }
        }

        bool TryBuildDeclaredGroupLabels(
            _In_ midi2enum::MidiEndpointDeviceInformation const& device,
            _In_ bool wantMessageSource,
            _Out_ GroupLabels& labels)
        {
            labels = {};

            bool anyClaimed{ false };

            auto const functionBlocks = device.GetDeclaredFunctionBlocks();

            if (functionBlocks != nullptr && functionBlocks.Size() > 0)
            {
                for (auto const& block : functionBlocks)
                {
                    if (!block.IsActive())
                    {
                        continue;
                    }

                    // A block input receives from us, so it is a destination for what we send.
                    auto const direction = block.Direction();

                    auto const usable = direction == midi2enum::MidiFunctionBlockDirection::Bidirectional ||
                        (wantMessageSource
                            ? direction == midi2enum::MidiFunctionBlockDirection::BlockOutput
                            : direction == midi2enum::MidiFunctionBlockDirection::BlockInput);

                    if (!usable)
                    {
                        continue;
                    }

                    ClaimGroups(labels, block.FirstGroup().Index(), block.GroupCount(), ToUtf8(block.Name()));
                    anyClaimed = true;
                }

                if (anyClaimed)
                {
                    return true;
                }
            }

            auto const groupTerminalBlocks = device.GetGroupTerminalBlocks();

            if (groupTerminalBlocks != nullptr && groupTerminalBlocks.Size() > 0)
            {
                for (auto const& block : groupTerminalBlocks)
                {
                    auto const direction = block.Direction();

                    auto const usable = direction == midi2enum::MidiGroupTerminalBlockDirection::Bidirectional ||
                        (wantMessageSource
                            ? direction == midi2enum::MidiGroupTerminalBlockDirection::BlockOutput
                            : direction == midi2enum::MidiGroupTerminalBlockDirection::BlockInput);

                    if (!usable)
                    {
                        continue;
                    }

                    ClaimGroups(labels, block.FirstGroup().Index(), block.GroupCount(), ToUtf8(block.Name()));
                    anyClaimed = true;
                }
            }

            return anyClaimed;
        }
    }

    GroupPickerResult PickGroup(
        _In_ std::string_view prompt,
        _In_ std::string const& endpointDeviceId,
        _In_ bool wantMessageSource)
    {
        GroupPickerResult result;

        if (!CanShowInteractiveUI())
        {
            WriteErrorLine(ResourceString(IDS_ERROR_NO_INTERACTIVE_CONSOLE));
            return result;
        }

        auto const device = midi2enum::MidiEndpointDeviceInformation::CreateFromEndpointDeviceId(
            winrt::hstring{ FromUtf8(endpointDeviceId) });

        GroupLabels labels{};

        // An endpoint which declares nothing, or nothing in this direction, still has sixteen
        // groups on the wire. Offering them all beats refusing to forward.
        auto const declared = device != nullptr && TryBuildDeclaredGroupLabels(device, wantMessageSource, labels);

        std::vector<PickerEntry> entries;
        std::vector<uint8_t> groupIndexes;

        auto const groupLabel = ToUtf8(midi2::MidiGroup::LongLabel());

        for (uint8_t index = 0; index < GroupCount; index++)
        {
            if (declared && labels[index].empty())
            {
                continue;
            }

            PickerEntry entry;

            entry.PrimaryText = fmt::format("{} {}", groupLabel, index + 1);
            entry.SecondaryText = labels[index];
            entry.Value = fmt::format("{}", index);

            entries.push_back(std::move(entry));
            groupIndexes.push_back(index);
        }

        FinalizePickerEntries(entries);

        auto const picked = ShowPicker(prompt, std::move(entries));

        if (picked.Canceled || picked.SelectedIndex < 0 ||
            static_cast<size_t>(picked.SelectedIndex) >= groupIndexes.size())
        {
            return result;
        }

        result.Canceled = false;
        result.GroupIndex = groupIndexes[static_cast<size_t>(picked.SelectedIndex)];

        return result;
    }
}
