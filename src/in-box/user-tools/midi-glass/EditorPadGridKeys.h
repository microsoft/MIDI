// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "LayoutModel.h"

namespace winrt::midiglass::implementation
{
    // The names of what a pad grid row does, in the order of PadGridFunction, which is also the
    // order the inspector lists them.
    constexpr wchar_t const* PadGridFunctionKeys[]
    {
        L"PadFunctionOctaveUp",
        L"PadFunctionOctaveDown",
        L"PadFunctionRowUp",
        L"PadFunctionRowDown",
        L"PadFunctionShiftLeft",
        L"PadFunctionShiftRight",
        L"PadFunctionSemitoneUp",
        L"PadFunctionSemitoneDown",
        L"PadFunctionInKeyOnlyOn",
        L"PadFunctionInKeyOnlyOff",
        L"PadFunctionInKeyOnlyToggle",
        L"PadFunctionNextRoot",
        L"PadFunctionPreviousRoot",
        L"PadFunctionNextScale",
        L"PadFunctionPreviousScale",
        L"PadFunctionNextRowOffset",
        L"PadFunctionReset",
    };

    static_assert(std::size(PadGridFunctionKeys) == static_cast<size_t>(::glass::PadGridFunctionCount));
}
