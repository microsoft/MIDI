// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// PROTOTYPE. The endpoints on this PC, as the MIDI tools see them.

#pragma once

#include "McpServer.h"
#include "EndpointCatalog.h"

namespace midimcp
{
    // A snapshot from the same catalog MIDI Patchbay and MIDI Glass use, so a group is named here
    // exactly the way it is named on their canvases. Starts the catalog on first use, which also
    // starts the MIDI service if it was stopped, the same as opening any MIDI tool does.
    std::vector<midiapp::LiveEndpoint> LiveEndpoints() noexcept;

    // Finds an endpoint the way a person names one: an exact id, then an exact name, then a name
    // that contains what was typed. More than one match is not a match; it is a question for the
    // customer, so the candidates come back for the model to ask about.
    struct EndpointLookup
    {
        std::optional<midiapp::LiveEndpoint> Found{};
        std::vector<midiapp::LiveEndpoint> Candidates{};
    };

    EndpointLookup FindEndpoint(
        _In_ std::vector<midiapp::LiveEndpoint> const& endpoints,
        _In_ std::wstring const& nameOrId);

    // "Launchkey 49" or, when two endpoints share a name, "Launchkey 49 (KS, ...5a1c)".
    std::wstring DescribeEndpoint(
        _In_ midiapp::LiveEndpoint const& endpoint,
        _In_ std::vector<midiapp::LiveEndpoint> const& all);

    // The problem text for a lookup that did not find exactly one endpoint.
    std::wstring DescribeLookupProblem(
        _In_ std::wstring const& role,
        _In_ std::wstring const& asked,
        _In_ EndpointLookup const& lookup,
        _In_ std::vector<midiapp::LiveEndpoint> const& all);

    // "Group 3 \"Aux\"", with the block name when the device gives one.
    std::wstring DescribeGroup(
        _In_ midiapp::LiveEndpoint const& endpoint,
        _In_ int32_t groupIndex,
        _In_ bool isSource);

    ToolDefinition MakeListEndpointsTool();
}
