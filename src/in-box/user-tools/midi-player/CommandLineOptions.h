// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

namespace midiplayer
{
    struct CommandLineOptions
    {
        bool ShowHelp{ false };

        // Files to queue. Explorer passes one file per invocation, but a multiple selection
        // opened at once gives several, and a shortcut can name a list.
        std::vector<std::wstring> Files{};

        // Start playing as soon as the first file is ready. This is what a double click in
        // Explorer means.
        bool AutoPlay{ true };

        std::wstring EndpointDeviceId{};
        std::optional<uint8_t> GroupNumber{};

        static CommandLineOptions Parse(_In_ std::vector<std::wstring> const& arguments) noexcept;
        static CommandLineOptions ParseProcessCommandLine() noexcept;
    };
}
