// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// The logic steps as messages meet them: the parts of a message they read and write, the tags a
// message carries, the memories a patch keeps, and which way a Branch or a Switch sends a
// message. Pure, so the unit tests compile this exactly as it ships. Everything here runs on the
// service callback thread: no allocation, no locks, nothing that throws.

#include "ProcessingBlock.h"

#include <array>
#include <atomic>
#include <optional>

namespace midipatchbay
{
    // How far a value's scale reaches. Plain is a whole number, such as a note or a channel.
    constexpr uint32_t PlainRange = 0;
    constexpr uint32_t SevenBitRange = 0x7F;
    constexpr uint32_t FourteenBitRange = 0x3FFF;
    constexpr uint32_t SixteenBitRange = 0xFFFF;
    constexpr uint32_t ThirtyTwoBitRange = 0xFFFFFFFF;
    constexpr uint32_t HundredthsRange = static_cast<uint32_t>(FullScaleHundredths);

    // A value as a logic step holds it: the number, the top of its scale, and whether anything has
    // set it. A value keeps the resolution it was read at, so one copied from a MIDI 2.0 message
    // into another goes over whole.
    struct LogicValue
    {
        uint32_t Raw{ 0 };
        uint32_t Range{ PlainRange };
        bool HasValue{ false };
    };

    LogicValue PlainValue(_In_ uint32_t number) noexcept;
    LogicValue ScaledValue(_In_ uint32_t raw, _In_ uint32_t range) noexcept;

    // A number typed into a step, in its unit.
    LogicValue ValueFromUnit(_In_ uint32_t number, _In_ LogicUnit unit) noexcept;

    // In hundredths of a percent. A plain number counts on the 0 to 127 scale.
    int32_t HundredthsOf(_In_ LogicValue const& value) noexcept;

    // As a whole number. A value is brought to the 0 to 127 scale first.
    uint32_t WholeOf(_In_ LogicValue const& value) noexcept;

    // In the unit a test holds its numbers in: a value in hundredths, everything else whole.
    uint32_t ValueInUnit(_In_ LogicValue const& value, _In_ LogicUnit unit) noexcept;

    // For a field this many bits wide, scaled the way MIDI 2.0 scales between resolutions. A
    // plain number counts on the 0 to 127 scale.
    uint32_t FieldValueOf(_In_ LogicValue const& value, _In_ uint32_t bits) noexcept;

    // The part of a message, or an empty value when the message has no such part.
    LogicValue ReadPart(
        _In_ PartPlace const& place,
        _In_reads_(wordCount) uint32_t const* words,
        _In_ uint8_t wordCount) noexcept;

    // False when the message has no such part, and then it is left as it was. A MIDI 1.0 note on
    // is never turned into a note off, nor a note off into a note on.
    bool WritePart(
        _In_ PartPlace const& place,
        _In_ LogicValue const& value,
        _Inout_updates_(wordCount) uint32_t* words,
        _In_ uint8_t wordCount) noexcept;

    bool PassesCondition(_In_ LogicCondition const& condition, _In_ LogicUnit unit, _In_ LogicValue const& value) noexcept;

    // One tag set on the way, on the stack of the step that set it. A message's tags are the chain
    // back from the last step that set one, so each path through a patch carries its own and no
    // other message can see them.
    struct TagLink
    {
        TagLink const* Previous{ nullptr };
        LogicValue Value{};
        uint32_t Slot{ 0 };
    };

    LogicValue FindTag(_In_opt_ TagLink const* tags, _In_ uint32_t slot) noexcept;

    // A memory is one 64-bit atomic: the number, the top of its scale, and whether it has one.
    uint64_t PackLogicValue(_In_ LogicValue const& value) noexcept;
    LogicValue UnpackLogicValue(_In_ uint64_t packed) noexcept;

    // What a Set memory step makes of a memory. Source is what its source read, for Set.
    LogicValue NextMemoryValue(
        _In_ SetMemorySettings const& settings,
        _In_ LogicValue const& current,
        _In_ LogicValue const& source) noexcept;

    // The ways out of a Branch or a Switch a message takes, as bits by way: up to 65 of them.
    struct WaySet
    {
        uint64_t Low{ 0 };
        uint64_t High{ 0 };

        static WaySet Every() noexcept;
        static WaySet One(_In_ int32_t way) noexcept;

        bool Contains(_In_ int32_t way) const noexcept;
        bool IsEmpty() const noexcept;
        bool IsEvery() const noexcept;

        WaySet& operator|=(_In_ WaySet const& other) noexcept;
        bool operator==(WaySet const&) const = default;
    };

    // Where a message goes by what the step tests, which is empty when it can't look.
    WaySet DecideBranch(_In_ BranchSettings const& settings, _In_ LogicValue const& subject) noexcept;
    WaySet DecideSwitch(_In_ SwitchSettings const& settings, _In_ LogicValue const& subject) noexcept;

    // What a Branch or a Switch remembers between messages, so that it never leaves a note
    // sounding or splits a message in pieces: where each note went, which ways a held pedal went
    // down on, and where the first packet of a long message went. Made when the step starts
    // routing, never on the way of a message.
    struct WayMemory
    {
        // By group, channel and note: 0 for not known, the way plus one, KeptOutMark or
        // EveryWayMark.
        std::array<std::atomic<uint8_t>, 16 * 16 * 128> Notes{};

        // By group, channel and pedal (sustain, sostenuto, soft): the ways it went down on, as a
        // low and a high word.
        std::array<std::atomic<uint64_t>, 16 * 16 * 3 * 2> Pedals{};

        // By group and kind of long message (SysEx, 128-bit data, flex data): the ways its first
        // packet took, as a low and a high word. The top bit of the high word says one is known.
        std::array<std::atomic<uint64_t>, 16 * 3 * 2> Packets{};

        // What the step tested last, for the inspector.
        std::atomic<uint64_t> LastTested{ 0 };
    };

    constexpr uint8_t KeptOutMark = 0xFE;
    constexpr uint8_t EveryWayMark = 0xFF;

    // The ways a message really goes, from what the step decided: a note off goes where its note
    // on went, so does anything else about that note, a pedal let go goes everywhere it was held
    // down, all notes off goes every way, and the rest of a long message follows its start.
    WaySet FollowWays(
        _Inout_ WayMemory& memory,
        _In_reads_(wordCount) uint32_t const* words,
        _In_ uint8_t wordCount,
        _In_ WaySet const& decided) noexcept;
}
