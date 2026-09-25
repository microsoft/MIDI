// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Deliberately free of pch.h and XAML, so the unit tests compile it unchanged.

#include <sal.h>
#include <cstdint>
#include <string>
#include <vector>

#include "LayoutModel.h"

namespace glass
{
    enum class RepeatDirection
    {
        Right = 0,
        Left = 1,
        Down = 2,
        Up = 3,
    };

    // What steps on each copy. Building a sixteen channel mixer by hand means sixteen copies and
    // sixteen hand-edited channel numbers, and one of them will be wrong.
    enum class RepeatField
    {
        Nothing = 0,
        Channel = 1,
        ControllerNumber = 2,
        NoteNumber = 3,
        Group = 4,
    };

    constexpr int32_t MaximumRepeatCopies = 256;

    struct RepeatOptions
    {
        int32_t Copies{ 1 };
        RepeatDirection Direction{ RepeatDirection::Right };

        // Between the edge of one copy and the start of the next, in page pixels.
        double Gap{ 16.0 };

        RepeatField Field{ RepeatField::Nothing };
        int32_t Step{ 1 };

        // "Ch {n}" gives Ch 1, Ch 2 and so on, with the source counting as 1. Empty leaves every
        // label as it is.
        std::wstring LabelPattern{};
    };

    // Every {n} replaced by the number, and nothing else touched. A pattern with no {n} is used
    // as it stands, which is how somebody renames a whole bank to the same word.
    std::wstring FormatRepeatLabel(_In_ std::wstring const& pattern, _In_ int32_t number);

    struct RepeatResult
    {
        // The controls that were selected, with their labels renumbered if a pattern was given.
        // Same ids, so the caller writes them back over the originals.
        std::vector<Control> UpdatedSource{};

        // New controls, with new ids, offset and stepped. They go on the page in this order.
        std::vector<Control> Copies{};
    };

    // Takes a selection and builds a bank from it.
    //
    // The whole selection moves as a unit, so repeating a channel strip repeats the strip rather
    // than each control separately. Numbers wrap: channel and group at sixteen, note and
    // controller at a hundred and twenty eight, because a bank that runs off the end of the range
    // and silently stops stepping is worse than one that comes round again.
    RepeatResult BuildRepeat(_In_ std::vector<Control> const& source, _In_ RepeatOptions const& options);
}
