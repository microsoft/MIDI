// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Everything a customer reads about a filter, a transform or a block. Kept apart from the pure
// files because the words come from the app's resources.

#include "ProcessingBlock.h"
#include "CapabilityInquiry.h"

namespace midipatchbay
{
    // "C4", "F#-1" and so on, from the shipped SDK helper so this app names notes the same way
    // every other Windows MIDI Services tool does.
    winrt::hstring DescribeNote(_In_ uint8_t noteIndex) noexcept;

    // Localized names for the pickers.
    winrt::hstring DescribeMessageType(_In_ uint8_t messageType) noexcept;
    winrt::hstring DescribeChannelVoiceStatus(_In_ uint8_t status) noexcept;
    winrt::hstring DescribeSystemMessage(_In_ uint8_t status) noexcept;

    // One line for the inspector, for example "Channels 1, 2 - no clock - notes C2 to C6".
    winrt::hstring SummarizeFilter(_In_ MessageFilter const& filter) noexcept;

    // "72" or "56.69%", for a summary line.
    winrt::hstring DescribeScaledValue(_In_ int32_t hundredths, _In_ ValueScale scale) noexcept;

    winrt::hstring SummarizeTransform(_In_ MessageTransform const& transform) noexcept;

    // "1, 2, 5 - 8" rather than a list of sixteen numbers. Offset turns an index into what a
    // person counts from.
    std::wstring DescribeRuns(_In_reads_(count) bool const* values, _In_ size_t count, _In_ int offset) noexcept;

    // "Note filter".
    winrt::hstring BlockKindName(_In_ BlockKind kind) noexcept;

    // The short label on a palette tile, "Note".
    winrt::hstring BlockKindShortName(_In_ BlockKind kind) noexcept;

    // A few characters that stand for the kind on the canvas and in the palette, "CC#".
    winrt::hstring BlockKindBadge(_In_ BlockKind kind) noexcept;

    // What the kind is for, for a tooltip.
    winrt::hstring BlockKindHint(_In_ BlockKind kind) noexcept;

    winrt::hstring BlockCategoryName(_In_ BlockCategory category) noexcept;

    // What the block does right now, in one short line, for example "Lets C3 to A4 through".
    winrt::hstring DescribeBlock(_In_ BlockKind kind, _In_ BlockSettings const& settings) noexcept;

    // "Exactly 60", "36, 38 or 42", "0 to 63" in the number style the mask filter shows.
    winrt::hstring DescribeMaskValue(_In_ uint32_t value, _In_ bool hex) noexcept;

    // "MIDI 1.0 wire speed", "2× MIDI 1.0 wire speed" or "Unlimited".
    winrt::hstring DescribeSendSpeed(_In_ uint32_t speed) noexcept;

    // At most this many places, without trailing zeros: "2", "2.5".
    winrt::hstring DescribeNumber(_In_ double value, _In_ int places) noexcept;

    // "120" or "97.5": two places at most, without trailing zeros.
    winrt::hstring DescribeTempo(_In_ double beatsPerMinute) noexcept;

    // "30 fps" for a summary, or "30 frames per second" for a picker.
    winrt::hstring DescribeFrameRate(_In_ midiapp::MidiTimeCodeFrameRate rate, _In_ bool forPicker) noexcept;

    // The same names MIDI Glass uses.
    winrt::hstring DescribeLfoWave(_In_ midiapp::LfoWave wave) noexcept;

    // "1 bar" or "1/4" for the lengths the picker offers, and "2.5 beats" for anything else.
    winrt::hstring DescribeLfoLength(_In_ double beatsPerCycle) noexcept;

    // "Pitch bend", for a picker.
    winrt::hstring DescribeValueMessageKind(_In_ midiapp::ValueMessageKind kind) noexcept;

    // "RPN 0/0". A bank or index of -1 is "any".
    winrt::hstring DescribeParameter(_In_ ParameterKind kind, _In_ int32_t bank, _In_ int32_t index) noexcept;

    // "CC 64 at 64 or more", for a gate's summary.
    winrt::hstring DescribeGateTrigger(_In_ GateTrigger const& trigger) noexcept;

    // "Profiles, Property Exchange", from the CiCategory bits.
    winrt::hstring DescribeCiCategories(_In_ uint8_t categories) noexcept;

    // "Discovery", "Get property", for what a MIDI-CI step has been answering.
    winrt::hstring DescribeCiMessage(_In_ uint8_t messageType) noexcept;

    // One problem with a MIDI-CI file, in a sentence.
    winrt::hstring DescribeCiFileProblem(_In_ CiFileProblem const& problem) noexcept;
}
