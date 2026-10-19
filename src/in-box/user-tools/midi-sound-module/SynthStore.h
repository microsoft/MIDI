// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "SynthList.h"

namespace midisoundmodule
{
    // The synths, kept in a JSON file in the customer's local app data. The file is the
    // customer's to edit, so it is read as untrusted input.
    class SynthStore
    {
    public:
        // A file that is missing is an empty list. One that can't be read, or holds something that
        // can't be used as it is, comes back with a problem and no synths, and is left untouched.
        static SynthList Load() noexcept;
        static bool Save(_In_ std::vector<SynthDefinition> const& synths) noexcept;

        static std::wstring FilePath() noexcept;

        static constexpr size_t MaximumSynths = ::midisoundmodule::MaximumSynths;
        static constexpr size_t MaximumNameLength = MaximumSynthNameLength;

        static std::wstring NewId();
        static std::string NewProductInstanceId();
    };
}
