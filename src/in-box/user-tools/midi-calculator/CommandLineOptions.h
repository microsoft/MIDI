// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "AppSettings.h"

namespace midicalculator
{
    // midicalculator [page] [value]
    //
    //   page   tempo, pitch, midi2, time or history. Opens that page instead of the last one.
    //   value  filled in on that page: a tempo, a note or frequency, UMP words, or a timestamp.
    //          "midicalculator time 2892714813833" opens Windows timestamps with that timestamp.
    //
    // Anything not understood is ignored, so a launch never fails over its command line.
    struct CommandLineOptions
    {
        std::optional<CalculatorPage> Page{};
        std::wstring Value{};

        static CommandLineOptions Parse(_In_ std::vector<std::wstring> const& arguments) noexcept;
        static CommandLineOptions ParseProcessCommandLine() noexcept;
    };
}
