// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

namespace midiloopbacksetup
{
    struct CommandLineOptions
    {
        bool ShowHelp{ false };
        bool HasError{ false };

        // resource key for the parse failure, so the message stays localizable
        std::wstring ErrorResourceKey{};
        std::wstring ErrorArgument{};

        // Debug builds only: the SDK reads and saves this file instead of the machine's live
        // configuration, so a copy can be worked on. Release builds accept the switch and ignore it.
        std::wstring ConfigFilePath{};

        // Opened from a notification about feedback, so start on the page that has the muted loopback.
        bool ShowFeedback{ false };

        static CommandLineOptions Parse(std::vector<std::wstring> const& arguments) noexcept;
        static CommandLineOptions ParseProcessCommandLine() noexcept;
    };
}
