// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

namespace midi2console
{
    // Modal single-line text entry. Returns nullopt when the user pressed Escape.
    std::optional<std::string> PromptForText(
        _In_ std::string_view title,
        _In_ std::string_view hint,
        _In_ std::string_view placeholder);
}
