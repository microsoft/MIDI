// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

namespace miditroubleshooter
{
    struct CommandLineOptions
    {
        bool ShowHelp{ false };

        // Suppresses the automatic elevation prompt at startup. The tool then runs read-only
        // until the customer elevates from the banner. Used for development and for a customer
        // who only wants to look at diagnostics.
        bool NoElevate{ false };

        // Set on the relaunched copy so it never tries to elevate a second time.
        bool Relaunched{ false };

        // Opens on a specific page, so MIDI Settings or a support engineer can send someone
        // straight to the right place.
        std::wstring StartPage{};

        static CommandLineOptions Parse(std::vector<std::wstring> const& arguments) noexcept;
        static CommandLineOptions ParseProcessCommandLine() noexcept;
    };
}
