// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// The document itself is pure and lives in PatchDocument.h. This adds what only the app has:
// resources for the words, and the live endpoint catalog.
#include "PatchDocument.h"
#include "MessageText.h"

// Endpoint identity, matching and the live catalog are shared with the other MIDI tools.
#include "EndpointCatalog.h"

namespace midipatchbay
{
    // The shared names, usable unqualified throughout this app the way they always were.
    using LiveEndpoint = midiapp::LiveEndpoint;
    using EndpointCatalog = midiapp::EndpointCatalog;

    // "3 - Iridium Aux", "Group 3" when the group has no name, or the all-groups caption. On
    // the In side all groups reads "Any group": each message keeps its own group there.
    winrt::hstring DescribeGroupIndex(
        _In_ int32_t groupIndex,
        _In_ std::wstring const& groupName,
        _In_ bool isInput = false) noexcept;

    // The shared catalog matches on criteria alone, so these are the one place that knows a
    // saved endpoint keeps its criteria, its mode and the name it last went by.
    std::optional<LiveEndpoint> ResolveEndpoint(_In_ PatchEndpoint const& endpoint) noexcept;
    std::optional<LiveEndpoint> SuggestReplacementFor(_In_ PatchEndpoint const& endpoint) noexcept;

    // Whether two endpoints, from one patch or two, stand for one device. A file written by hand
    // can match by name and hold no device ID, so an empty ID never matches another empty one.
    bool IsSameDevice(_In_ PatchEndpoint const& left, _In_ PatchEndpoint const& right) noexcept;

    // Whether an endpoint on a patch stands for the device that is connected with this ID.
    bool StandsFor(_In_ PatchEndpoint const& endpoint, _In_ std::wstring const& endpointDeviceId) noexcept;

    // The name the customer gave the block, or its kind's name until they give it one.
    winrt::hstring BlockDisplayName(_In_ PatchBlock const& block) noexcept;
}
