// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "AppSettings.h"

namespace midiscratchpad
{
    struct CommandLineOptions
    {
        bool ShowHelp{ false };
        bool HasError{ false };

        // resource key for the parse failure, so the message stays localizable
        std::wstring ErrorResourceKey{};
        std::wstring ErrorArgument{};

        std::wstring EndpointDeviceId{};

        // user-facing number (1-16), not an index. MIDI 1.0 mode only: in UMP mode the group
        // travels inside the message.
        std::optional<uint8_t> GroupNumber{};

        std::optional<ScratchPadMode> Mode{};

        static CommandLineOptions Parse(std::vector<std::wstring> const& arguments) noexcept;
        static CommandLineOptions ParseProcessCommandLine() noexcept;
    };
}
