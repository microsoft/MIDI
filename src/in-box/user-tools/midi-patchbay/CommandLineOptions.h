// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

namespace midipatchbay
{
    struct CommandLineOptions
    {
        bool StartMinimized{ false };

        // Opens a named patch at startup, so another tool or a shortcut can bring up the one
        // the customer cares about.
        std::wstring PatchName{};

        // Patch files to import, which is what a double-click in Explorer sends.
        std::vector<std::wstring> FilesToImport{};

        // Anything it doesn't recognize is ignored: the app has no console to report it on.
        static CommandLineOptions ParseProcessCommandLine() noexcept;
    };
}
