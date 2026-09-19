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
        bool ShowHelp{ false };
        bool StartMinimized{ false };
        bool HasError{ false };

        // Opens a named patch at startup, so another tool or a shortcut can bring up the one
        // the customer cares about.
        std::wstring PatchName{};

        std::wstring ErrorText{};

        static CommandLineOptions ParseProcessCommandLine() noexcept;
    };
}
