// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

namespace midisoundfontsynth
{
    struct CommandLineOptions
    {
        // Set by the entry that starts the app with Windows, so it comes up out of the way.
        bool StartMinimized{ false };

        static CommandLineOptions ParseProcessCommandLine() noexcept;
    };
}
