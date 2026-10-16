// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// The message converter: turns one kind of message into another, such as the note a DAW
// controller's pad sends into the control change a DAW listens for. Pure: no XAML, no resources
// and no precompiled header, so the unit tests compile this exactly as it ships. Names and
// summaries for people are in MessageText.h.

#include "ControllerSteps.h"

#include <array>
#include <atomic>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace midipatchbay
{
    // What a row of a converter looks for.
    enum class ConverterInput : int32_t
    {
        // A note on is pressed, and its note off lets go.
        Note = 0,

        ControlChange = 1,

        // Pressed each time. It never lets go.
        ProgramChange = 2,

        PitchBend = 3,
        ChannelPressure = 4,

        // Pressure on one note. The number is the note.
        PolyPressure = 5,
    };

    constexpr size_t ConverterInputCount = 6;

    // The names a patch file uses, the same as a controller button's messages: "note",
    // "controlChange" and so on.
    std::wstring_view ConverterInputKey(_In_ ConverterInput input) noexcept;
    std::optional<ConverterInput> ConverterInputFromKey(_In_ std::wstring_view key) noexcept;

    // Carries a note, a controller or a program number.
    bool ConverterInputHasNumber(_In_ ConverterInput input) noexcept;

    // Carries a velocity, a controller's value, a pressure or a bend.
    bool ConverterInputHasValue(_In_ ConverterInput input) noexcept;

    // A value that moves, which a row can also read as a button.
    bool ConverterInputIsValue(_In_ ConverterInput input) noexcept;

    // What letting go does.
    enum class ConverterRelease : int32_t
    {
        // A note off for a note, or the release value for a value.
        Send = 0,

        Nothing = 1,

        // Each press switches between the message and what letting go would send. Letting go
        // itself sends nothing.
        Toggle = 2,
    };

    // 64 out of 127: where a MIDI 1.0 switch, such as the sustain pedal, counts as down.
    constexpr int32_t DefaultConverterPressHundredths = 5039;

    struct ConverterRow
    {
        ConverterInput From{ ConverterInput::Note };

        // Counted from 0. -1 is any channel.
        int8_t FromChannel{ -1 };

        // The notes, controllers or programs the row looks for, both ends included.
        uint8_t Lowest{ 0 };
        uint8_t Highest{ 127 };

        // A value read as a button: pressed from PressHundredths up, and let go below it. Off,
        // every value is converted as it comes.
        bool AsButton{ false };
        int32_t PressHundredths{ DefaultConverterPressHundredths };

        // What the message becomes. Its group isn't used: a message always keeps its own.
        ControllerMessage To{ ControllerMessageFor(ControllerMessageKind::ControlChange) };

        // Each takes what the message had instead of what To says.
        bool SameChannel{ true };
        bool SameNumber{ true };
        bool SameValue{ true };
        bool SameProtocol{ true };

        ConverterRelease Release{ ConverterRelease::Send };
    };

    // Middle C on any channel, as the control change with the same number.
    ConverterRow DefaultConverterRow() noexcept;

    // Whether the row sees its input let go: a note off, or a value falling below the press point.
    bool ConverterRowCanLetGo(_In_ ConverterRow const& row) noexcept;

    // Whether a message of this kind has something to send when it's let go: a note off, or a value.
    bool ConverterOutputCanLetGo(_In_ ControllerMessageKind kind) noexcept;

    constexpr size_t MaximumConverterRows = 64;

    struct MessageConverterSettings
    {
        // The first row that matches a message is the one used.
        std::vector<ConverterRow> Rows{};

        // Display only.
        ValueScale Scale{ ValueScale::SevenBit };
    };

    // What a converter remembers between messages: which inputs read as buttons are down, which
    // toggles are on, and where next and previous program are. Made when the step starts
    // routing, never on the way of a message.
    struct ConverterMemory
    {
        // One bit for each kind of input, group, channel and number.
        static constexpr size_t SlotWords = ConverterInputCount * 16 * 16 * 128 / 64;

        std::array<std::atomic<uint64_t>, SlotWords> Down{};
        std::array<std::atomic<uint64_t>, SlotWords> On{};

        // By group and channel: the program last sent, plus one. 0 is none yet.
        std::array<std::atomic<uint32_t>, 256> Programs{};
    };

    constexpr uint8_t MaximumConvertedMessages = 2;

    // What a converter sends instead of the message it was given.
    struct ConvertedMessages
    {
        std::array<std::array<uint32_t, MaximumControllerWords>, MaximumConvertedMessages> Words{};
        std::array<uint8_t, MaximumConvertedMessages> Counts{};
        uint8_t Count{ 0 };
    };

    // Runs one message through the rows. False when no row matches, and the message goes on as
    // it is. True when one does, and then what is in converted goes instead, which can be
    // nothing. Called on the service callback thread for every message: no allocation, no locks,
    // nothing that throws.
    bool ConvertMessage(
        _In_ MessageConverterSettings const& settings,
        _Inout_ ConverterMemory& memory,
        _In_reads_(wordCount) uint32_t const* words,
        _In_ uint8_t wordCount,
        _Out_ ConvertedMessages& converted) noexcept;

    // Only the parts a row uses are written. Leaving out the channel, number, value or protocol
    // of what a message becomes keeps what the message had. Anything missing or out of range
    // takes the default when read.
    void MessageConverterToJson(_In_ MessageConverterSettings const& settings, _In_ json::JsonObject const& object);
    MessageConverterSettings MessageConverterFromJson(_In_ json::JsonObject const& object) noexcept;

    std::wstring MessageConverterSignature(_In_ MessageConverterSettings const& settings);
}
