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
    // Parsing of the UMP word text used by send-message and send-message-file. Accepts 0x / 0b
    // prefixes and h / b / d suffixes, falling back to the format named on the command line.
    bool TryParseMidiWord(
        _In_ std::string_view text,
        _In_ std::string_view defaultFormat,
        _Out_ uint32_t& value);

    // Splits a line on the requested delimiter. "Auto" picks whichever of comma, pipe or tab is
    // present, otherwise whitespace.
    std::vector<std::string> SplitDelimitedLine(_In_ std::string_view line, _In_ std::string_view delimiter);

    std::string ExpandEnvironmentPath(_In_ std::string_view path);
}
