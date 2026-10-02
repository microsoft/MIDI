// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

namespace midinetworksetup
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

        // Set when the app was started through its protocol, which asks for a page and nothing
        // more. See network_setup_protocol_defs.h for why a URI never carries an action.
        bool ShowPendingApprovals{ false };
        bool ShowLocalHosts{ false };
        bool ShowRtpLocalHosts{ false };

        static CommandLineOptions Parse(std::vector<std::wstring> const& arguments) noexcept;
        static CommandLineOptions ParseProcessCommandLine() noexcept;
    };
}
