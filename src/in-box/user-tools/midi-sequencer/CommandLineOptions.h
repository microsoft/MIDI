// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

namespace midisequencer
{
    // midisequencer [file] [--sample]
    //
    // A .midisequence opens. A Standard MIDI File or a MIDI 2.0 clip file is imported into a new
    // sequence, so a double-click on one in Explorer does something useful. --sample opens the
    // sample sequence, which is how a window that's already in use opens it in a new one.
    struct CommandLineOptions
    {
        std::wstring File{};
        bool Sample{ false };

        static CommandLineOptions Parse(_In_ std::vector<std::wstring> const& arguments) noexcept;
        static CommandLineOptions ParseProcessCommandLine() noexcept;
    };
}
