// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Describes a Universal MIDI Packet in words a person reads: "Note 60 (C4)   Velocity 100".
// Windows MIDI Monitor's Details column and Windows MIDI Patchbay's connection monitor windows
// both use it, so the two apps describe a message the same way.
//
// The labels ("Note", "Velocity"...) are read by each app from its own resources and handed in,
// because shared code can't reach an app's resources. Note names, controller names and message
// names are MIDI terms and come from the SDK or from here, untranslated.

#include <array>
#include <cstdint>
#include <string>

namespace midiapp
{
    struct UmpDescriptionLabels
    {
        winrt::hstring Note;
        winrt::hstring Velocity;
        winrt::hstring Pressure;
        winrt::hstring Controller;
        winrt::hstring Value;
        winrt::hstring Program;
        winrt::hstring Bank;
        winrt::hstring Bend;
        winrt::hstring Index;
        winrt::hstring Bytes;
        winrt::hstring Status;
        winrt::hstring Song;
        winrt::hstring Position;
        winrt::hstring Frame;
        winrt::hstring Attribute;
        winrt::hstring Text;
        winrt::hstring Name;
        winrt::hstring Ticks;
        winrt::hstring Tempo;
        winrt::hstring StreamId;
        winrt::hstring FunctionBlock;
        winrt::hstring Form;
        winrt::hstring TimeSignature;
        winrt::hstring KeySignature;
        winrt::hstring PerNote;
        winrt::hstring Options;
        winrt::hstring NoteOffValue;

        // A time signature whose denominator isn't a power of two. {0} is the numerator.
        winrt::hstring TimeSignatureNonStandardFormat;
    };

    // The contents of one message, as labels and values kept apart, so a window can color the
    // values without parsing anything.
    struct UmpDescription
    {
        static constexpr size_t MaxFields = 4;

        std::array<winrt::hstring, MaxFields> Labels{};
        std::array<winrt::hstring, MaxFields> Values{};
        uint32_t Count{ 0 };

        void Add(_In_ winrt::hstring const& label, _In_ std::wstring const& value) noexcept
        {
            if (Count >= MaxFields)
            {
                return;
            }

            Labels[Count] = label;
            Values[Count] = winrt::hstring{ value };
            Count++;
        }

        // "Label value   Label value", for copying and saving to a file.
        winrt::hstring ToDisplayString() const;
    };

    // Empty when there is nothing more to say than the message's name.
    UmpDescription DescribeUmp(
        _In_ std::array<uint32_t, 4> const& words,
        _In_ uint8_t wordCount,
        _In_ UmpDescriptionLabels const& labels) noexcept;

    // The message's name from the SDK, such as "MIDI 1.0 Note On".
    winrt::hstring UmpDisplayName(_In_ uint32_t word0) noexcept;

    // 0 to 7, one for each family of message types, for picking the name chiclet's color.
    uint32_t UmpMessageTypeColorIndex(_In_ uint8_t messageType) noexcept;
}
