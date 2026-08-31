// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

namespace midibluetoothsetup
{
    struct CommandLineOptions
    {
        bool ShowHelp{ false };
        bool HasError{ false };

        // resource key for the parse failure, so the message stays localizable
        std::wstring ErrorResourceKey{};
        std::wstring ErrorArgument{};

        // Redirects every configuration file read and write to another file. Intended for
        // working against a copy rather than the machine's live configuration.
        std::wstring ConfigFilePath{};

        static CommandLineOptions Parse(std::vector<std::wstring> const& arguments) noexcept;
        static CommandLineOptions ParseProcessCommandLine() noexcept;
    };
}
