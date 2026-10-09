// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// PROTOTYPE. MIDI Patchbay tools: read the saved patches, preview a patch described in plain
// terms, and save it as a draft the customer reviews in the app before anything routes.

#pragma once

#include "McpServer.h"

namespace midimcp
{
    struct PatchbayToolOptions
    {
        // Where drafts are written and saved patches are read. Empty means Documents\MIDI Patches,
        // the app's own folder. The test harness points it somewhere disposable.
        std::wstring PatchFolder{};
    };

    std::vector<ToolDefinition> MakePatchbayTools(_In_ PatchbayToolOptions const& options);
}
