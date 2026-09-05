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
}
