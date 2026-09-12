// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

namespace midiclock
{
    struct CommandLineOptions
    {
        bool ShowHelp{ false };
        bool HasError{ false };

        // resource key for the parse failure, so the message stays localizable
        std::wstring ErrorResourceKey{};
        std::wstring ErrorArgument{};

        // When present, the clock for this endpoint is brought to the front, and created first
        // if there is not one already. This is how MIDI Settings hands an endpoint over.
        std::wstring EndpointDeviceId{};

        // user-facing number (1-16), not an index
        std::optional<uint8_t> GroupNumber{};
        std::optional<double> BeatsPerMinute{};

        // start the clock for the endpoint above, or every clock when none was named
        bool Start{ false };

        static CommandLineOptions Parse(std::vector<std::wstring> const& arguments) noexcept;
        static CommandLineOptions ParseProcessCommandLine() noexcept;
    };
}
