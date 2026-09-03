// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

namespace midikeyboard
{
    struct CommandLineOptions
    {
        bool ShowHelp{ false };
        bool HasError{ false };

        // resource key for the parse failure, so the message stays localizable
        std::wstring ErrorResourceKey{};
        std::wstring ErrorArgument{};

        // when present the app plays this endpoint instead of creating a virtual device
        std::wstring EndpointDeviceId{};

        // user-facing numbers (1-16), not indexes
        std::optional<uint8_t> GroupNumber{};
        std::optional<uint8_t> ChannelNumber{};

        static CommandLineOptions Parse(std::vector<std::wstring> const& arguments) noexcept;
        static CommandLineOptions ParseProcessCommandLine() noexcept;
    };
}
