// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// The processing steps a patch is built from. Pure: no XAML, no resources and no precompiled
// header, so the unit tests compile this exactly as it ships. Names and summaries for people are
// in MessageText.h.

#include "MessageTransform.h"

// Shared with MIDI Glass and MIDI Clock, in midi-app-shared. All three are pure.
#include "ChannelVoiceWords.h"
#include "LfoWave.h"
#include "MidiTimeCode.h"

#include <atomic>
#include <optional>
#include <vector>

namespace midipatchbay
{
    // Every kind of block. The numbers are never stored: the patch file uses BlockKindKey.
    enum class BlockKind : int32_t
    {
        MessageTypeFilter = 0,
        GroupFilter = 1,
        ChannelFilter = 2,
        NoteFilter = 3,
        ControlChangeFilter = 4,
        VelocityFilter = 5,
        MessageMaskFilter = 6,

        ChannelMap = 7,
        GroupMap = 8,
        NoteMap = 9,
        Transpose = 10,
        Velocity = 11,
        Aftertouch = 12,
        ControlChangeMap = 13,
        ControlChangeValue = 14,
        ProgramMap = 15,

        Throttle = 16,

        ClockGenerator = 17,
        TimeCodeGenerator = 18,
        LfoGenerator = 19,

        ClockDivider = 20,
    };

    constexpr size_t BlockKindCount = 21;

    // The order the palette shows them in.
    constexpr BlockKind AllBlockKinds[BlockKindCount] =
    {
        BlockKind::MessageTypeFilter, BlockKind::GroupFilter, BlockKind::ChannelFilter,
        BlockKind::NoteFilter, BlockKind::ControlChangeFilter, BlockKind::VelocityFilter,
        BlockKind::MessageMaskFilter,
        BlockKind::ChannelMap, BlockKind::GroupMap, BlockKind::NoteMap, BlockKind::Transpose,
        BlockKind::Velocity, BlockKind::Aftertouch, BlockKind::ControlChangeMap,
        BlockKind::ControlChangeValue, BlockKind::ProgramMap, BlockKind::ClockDivider,
        BlockKind::Throttle,
        BlockKind::ClockGenerator, BlockKind::TimeCodeGenerator, BlockKind::LfoGenerator,
    };

    enum class BlockCategory : int32_t
    {
        Filter = 0,
        Transform = 1,
        Sending = 2,

        // Makes messages of its own, so it has a way out and no way in.
        Generator = 3,
    };

    BlockCategory CategoryOf(_In_ BlockKind kind) noexcept;

    bool IsGenerator(_In_ BlockKind kind) noexcept;

    // Every kind but MIDI clock and MIDI Time Code has an In. An LFO's takes the clock it follows.
    bool HasInput(_In_ BlockKind kind) noexcept;

    // The name a patch file uses for the kind, for example "noteFilter".
    std::wstring_view BlockKindKey(_In_ BlockKind kind) noexcept;
    std::optional<BlockKind> BlockKindFromKey(_In_ std::wstring_view key) noexcept;

    // Which values a note or control change filter looks at.
    enum class ValueSetMode : int32_t
    {
        Range = 0,
        One = 1,
        List = 2,
    };

    // What happens to the messages a filter picks out.
    enum class FilterAction : int32_t
    {
        LetThrough = 0,
        KeepOut = 1,
    };

    constexpr size_t SevenBitValueCount = 128;

    // A set of 0 to 127 values: notes for the note filter, controller numbers for the control
    // change filter.
    struct ValueSetFilter
    {
        ValueSetMode Mode{ ValueSetMode::Range };
        FilterAction Action{ FilterAction::LetThrough };

        uint8_t Lowest{ 0 };
        uint8_t Highest{ 127 };
        uint8_t One{ 60 };
        std::array<bool, SevenBitValueCount> List{};

        bool Contains(_In_ uint8_t value) const noexcept;
        bool Passes(_In_ uint8_t value) const noexcept;
        bool PassesEverything() const noexcept;
    };

    // Note on velocities, in hundredths of a percent like every other value in this app.
    struct VelocityRange
    {
        FilterAction Action{ FilterAction::LetThrough };
        ValueScale Scale{ ValueScale::Percent };

        int32_t LowestHundredths{ 0 };
        int32_t HighestHundredths{ FullScaleHundredths };

        bool Contains(_In_ int32_t hundredths) const noexcept;
        bool PassesEverything() const noexcept;
    };

    enum class MaskMatch : int32_t
    {
        Exactly = 0,
        AnyOf = 1,
        Between = 2,
    };

    constexpr size_t MaximumMaskConditions = 4;
    constexpr size_t MaximumMaskValues = 128;
    constexpr uint8_t MaximumUmpWords = 4;

    // One place in a message, and the values that count as a match there.
    struct MaskCondition
    {
        // Counted from 0 in the file and from 1 on screen.
        uint8_t Word{ 0 };

        // Both ends included. Bit 31 is the top bit of the word.
        uint8_t HighBit{ 31 };
        uint8_t LowBit{ 0 };

        MaskMatch Match{ MaskMatch::Exactly };

        uint32_t Value{ 0 };
        std::vector<uint32_t> Values{};
        uint32_t Lowest{ 0 };
        uint32_t Highest{ 0 };

        uint8_t BitCount() const noexcept;
        uint32_t FieldMaximum() const noexcept;
        uint32_t FieldOf(_In_reads_(wordCount) uint32_t const* words, _In_ uint8_t wordCount) const noexcept;
        bool Matches(_In_reads_(wordCount) uint32_t const* words, _In_ uint8_t wordCount) const noexcept;
    };

    // The way out for any message the other filters do not cover: look at bits in messages of one
    // size, and let the ones that match through or keep them out.
    struct MessageMask
    {
        uint8_t WordCount{ 2 };
        FilterAction Action{ FilterAction::KeepOut };

        // Display only.
        bool ShowHex{ false };

        // All of them have to match. With none, the block does nothing.
        std::vector<MaskCondition> Conditions{};

        bool Passes(_In_reads_(wordCount) uint32_t const* words, _In_ uint8_t wordCount) const noexcept;
    };

    // Unchanged where -1.
    using GroupMapTable = std::array<int8_t, 16>;

    constexpr uint32_t DefaultThrottleSpeed = 1;

    // The tempo a clock or an LFO runs at. The same range as MIDI Clock.
    constexpr double MinimumGeneratorBeatsPerMinute = 20.0;
    constexpr double MaximumGeneratorBeatsPerMinute = 300.0;
    constexpr double DefaultGeneratorBeatsPerMinute = 120.0;

    // MIDI clock, 24 pulses a quarter note, from the moment the patch starts routing.
    struct ClockGeneratorSettings
    {
        double BeatsPerMinute{ DefaultGeneratorBeatsPerMinute };

        // Start as routing starts and stop as it stops, so a sequencer follows the patch.
        bool SendStartStop{ true };

        // 50 is straight. Up to 75, on eighth notes (2) or sixteenth notes (4).
        double SwingPercent{ 50.0 };
        int32_t SwingSubdivision{ 2 };

        // Counted from 0.
        uint8_t Group{ 0 };
    };

    struct TimeCodeGeneratorSettings
    {
        midiapp::MidiTimeCodeFrameRate FrameRate{ midiapp::MidiTimeCodeFrameRate::Frames30 };
        midiapp::MidiTimeCodePosition Start{};

        // A full frame message as it starts and as it stops, so a receiver finds its place at
        // once instead of reading it from the next eight quarter frames.
        bool SendFullFrame{ true };

        uint8_t Group{ 0 };
    };

    struct LfoGeneratorSettings
    {
        midiapp::LfoWave Wave{ midiapp::LfoWave::Sine };

        // How long one pass takes, in quarter notes at BeatsPerMinute. Four is one bar.
        double BeatsPerCycle{ 4.0 };
        double BeatsPerMinute{ DefaultGeneratorBeatsPerMinute };

        // The two ends of the sweep, in hundredths of a percent of the message's whole range.
        // Lowest above highest turns the wave upside down.
        int32_t LowestHundredths{ 0 };
        int32_t HighestHundredths{ FullScaleHundredths };

        int32_t IntervalMilliseconds{ midiapp::DefaultLfoIntervalMilliseconds };

        // The mod wheel on channel 1, group 1, unless the step says otherwise.
        midiapp::ValueMessageTarget Target{};

        // Sends the middle of the range when the patch stops routing, so a pitch bend is not
        // left bent.
        bool ReturnsToMiddle{ true };
    };

    // What an LFO step's number starts as for each kind of message: the mod wheel, middle C, or
    // the first RPN or NRPN.
    uint32_t DefaultLfoNumber(_In_ midiapp::ValueMessageKind kind) noexcept;

    // Clock divider: one timing clock in this many goes through. 1 lets every one through.
    constexpr uint32_t DefaultClockDivision = 2;
    constexpr uint32_t MaximumClockDivision = 96;

    // Everything any kind of block can hold. Each kind uses only its own part, so one shape
    // serves every kind and a block can be copied, compared and undone as a plain value.
    struct BlockSettings
    {
        // Message type filter and channel filter.
        MessageFilter Filter{};

        // Every transform kind uses the part of this it is named for.
        MessageTransform Transform{};

        // Note filter and control change filter.
        ValueSetFilter Values{};

        VelocityRange Velocities{};

        MessageMask Mask{};

        // Group filter: the groups let through.
        std::array<bool, 16> Groups{};

        GroupMapTable GroupMap{};

        // Throttle: a multiple of MIDI 1.0 wire speed, 0 for no limit.
        uint32_t SendSpeedLimit{ DefaultThrottleSpeed };

        ClockGeneratorSettings Clock{};
        TimeCodeGeneratorSettings TimeCode{};
        LfoGeneratorSettings Lfo{};

        uint32_t ClockDivision{ DefaultClockDivision };
    };

    // What a block dropped on the canvas starts as. Apart from the throttle, every kind starts
    // out changing nothing, so adding one never surprises a running patch.
    BlockSettings DefaultBlockSettings(_In_ BlockKind kind) noexcept;

    // Runs one block on one message. False means the message is kept out. Transforms rewrite the
    // words in place, so each path through a patch works on its own copy. Called on the service
    // callback thread for every message: no allocation, no locks, nothing that can throw.
    bool ProcessBlock(
        _In_ BlockKind kind,
        _In_ BlockSettings const& settings,
        _Inout_updates_(wordCount) uint32_t* words,
        _In_ uint8_t wordCount) noexcept;

    // True when the block, as set, leaves every message as it is.
    bool BlockChangesNothing(_In_ BlockKind kind, _In_ BlockSettings const& settings) noexcept;

    // Only the part the kind uses is written, so a block in a patch file reads like what it does.
    json::JsonObject BlockSettingsToJson(_In_ BlockKind kind, _In_ BlockSettings const& settings) noexcept;

    // Anything missing or out of range takes the default, the same as everywhere else in the
    // patch file: these files can come from anywhere.
    BlockSettings BlockSettingsFromJson(_In_ BlockKind kind, _In_ json::JsonObject const& object) noexcept;

    // Short and stable, for telling whether a running route needs to change.
    std::wstring BlockSettingsSignature(_In_ BlockKind kind, _In_ BlockSettings const& settings) noexcept;

    // A velocity from a message, in hundredths of a percent. MIDI 1.0 carries 7 bits and MIDI 2.0
    // carries 16, so both land on the same scale.
    int32_t HundredthsFromVelocity16(_In_ uint16_t velocity) noexcept;

    // The clock divider, on one message. Lets one timing clock in every divideBy through and
    // counts the rest. A start resets the count, so the first clock after it always goes through,
    // and a song position is divided too and moves the count to match. Start, stop, continue and
    // everything else go through untouched.
    //
    // The count is per step rather than per path, so it is atomic: two sources can reach one
    // divider on two threads.
    bool DivideClock(
        _In_ uint32_t divideBy,
        _Inout_ std::atomic<uint32_t>& counter,
        _Inout_updates_(wordCount) uint32_t* words,
        _In_ uint8_t wordCount) noexcept;

    // The settings a running generator cannot take on the fly: equal for two settings that only
    // differ in what it can change while it runs, such as the tempo. Starting a generator again
    // is something the receiving device notices, so this is kept to what it has to.
    std::wstring GeneratorRestartSignature(_In_ BlockKind kind, _In_ BlockSettings const& settings) noexcept;
}
