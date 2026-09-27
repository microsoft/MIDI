// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// PROTOTYPE. MIDI Glass tools: the control catalog, a preview picture of a layout described in
// plain terms, and a draft the customer reviews in the app. Built on MIDI Glass's own document
// layer, compiled unchanged, so what gets written is exactly what the app writes.

#pragma once

#include "McpServer.h"

namespace midimcp
{
    struct GlassToolOptions
    {
        // Where drafts are written and saved layouts are read. Empty means Documents\MIDI Layouts.
        std::wstring LayoutFolder{};

        // midiglass.exe, for its headless --thumbnail mode. Empty means look for it.
        std::wstring MidiGlassExe{};
    };

    std::vector<ToolDefinition> MakeGlassTools(_In_ GlassToolOptions const& options);
}
