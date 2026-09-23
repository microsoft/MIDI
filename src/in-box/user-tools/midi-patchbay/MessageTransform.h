// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "MessageFilter.h"

namespace midipatchbay
{
    enum class VelocityCurve : int32_t
    {
        Unchanged = 0,

        // Soft playing gets softer. What you want when a controller feels too easy to max out.
        LinearToCurved = 1,

        // The opposite, for a keyboard that already curves and feels hard to play loudly.
        CurvedToLinear = 2,

        // Every note leaves at the same velocity, whatever was played.
        Fixed = 3,
    };

    // How a value that is really a fraction of full scale is shown and typed. MIDI 2.0 carries
    // velocity in sixteen bits, so a percentage is what the number actually means, but plenty of
    // devices assign a meaning to each of the 128 MIDI 1.0 steps and those customers need to type
    // the step.
    enum class ValueScale : int32_t
    {
        SevenBit = 0,
        Percent = 1,
    };

    constexpr size_t NoteMapSize = 128;
    constexpr size_t ControlMapSize = 128;
    constexpr size_t ChannelMapSize = 16;
    constexpr size_t ProgramMapSize = 128;
    constexpr size_t BankMapSize = 128;

    constexpr int32_t MinimumTranspose = -48;
    constexpr int32_t MaximumTranspose = 48;

    // Values that mean a fraction of full scale are stored in hundredths of a percent, whichever
    // scale the customer is typing in. That is fine enough that a 0 to 127 value survives the
    // round trip exactly, and it is what a MIDI 2.0 message wants anyway.
    constexpr int32_t FullScaleHundredths = 10000;

    // How many entries any one mapping table may hold, so a hand edited file cannot grow the UI
    // without bound.
    constexpr size_t MaximumMapEntries = 128;

    // 0 to 127 and hundredths of a percent, both ways. Rounding is symmetrical, so a value typed
    // as a MIDI 1.0 step comes back as the same step.
    int32_t HundredthsFromSevenBit(_In_ int32_t value) noexcept;
    int32_t SevenBitFromHundredths(_In_ int32_t hundredths) noexcept;

    // What a stored value looks like in the scale the customer chose, and back again.
    double DisplayFromHundredths(_In_ int32_t hundredths, _In_ ValueScale scale) noexcept;
    int32_t HundredthsFromDisplay(_In_ double value, _In_ ValueScale scale) noexcept;

    // "72" or "56.69%", for a summary line.
    winrt::hstring DescribeScaledValue(_In_ int32_t hundredths, _In_ ValueScale scale) noexcept;

    // What one connection does to the messages it passes on.
    //
    // Like MessageFilter this is a plain value with no UI and no WinRT, because it runs on the
    // service callback thread for every message and a future API would expose this shape.
    struct MessageTransform
    {
        // Checked first, so an untouched transform costs one bool.
        bool IsActive{ false };

        // Only affects how the dialog shows and takes numbers. Nothing on the wire changes with it.
        ValueScale Scale{ ValueScale::Percent };

        // Applied before anything else, because the channel is the outermost address. -1 where a
        // channel is not remapped. Only messages that carry a channel are touched.
        std::array<int16_t, ChannelMapSize> ChannelMap{};

        // Applied to every note carrying message that the table below does not name.
        int32_t TransposeSemitones{ 0 };

        // -1 where a note is not remapped. An entry here wins over the transpose, so the table
        // reads as a list of exceptions.
        std::array<int16_t, NoteMapSize> NoteMap{};

        // A MIDI 2.0 note can carry the exact pitch it wants in its attribute, which makes the
        // note number a slot rather than a pitch. Left alone when this is set; otherwise the
        // pitch is moved with the note so the message stays consistent.
        bool IgnoreExactPitchNotes{ false };

        // Note on velocity only.
        VelocityCurve Curve{ VelocityCurve::Unchanged };

        // Hundredths of a percent of full scale.
        int32_t FixedVelocityHundredths{ 7874 };

        bool RescaleVelocity{ false };
        int32_t MinimumVelocityHundredths{ 0 };
        int32_t MaximumVelocityHundredths{ FullScaleHundredths };

        // -1 where a controller is not remapped. Values are carried over untouched.
        std::array<int16_t, ControlMapSize> ControlMap{};

        // Program change. The bank tables cover bank select MSB (controller 0) and LSB
        // (controller 32) on MIDI 1.0, and the bank fields a MIDI 2.0 program change carries.
        std::array<int16_t, ProgramMapSize> ProgramMap{};
        std::array<int16_t, BankMapSize> BankMsbMap{};
        std::array<int16_t, BankMapSize> BankLsbMap{};

        MessageTransform() noexcept;

        bool ChangesNothing() const noexcept;
        void Reset() noexcept;

        // Rewrites the message in place. The caller owns the buffer, which is the send buffer for
        // one destination, so one target's transform cannot disturb another's copy.
        void Apply(_Inout_updates_(wordCount) uint32_t* words, _In_ uint8_t wordCount) const noexcept;

        // What one note becomes, for the dialog to show an example.
        uint8_t ResultingNote(_In_ uint8_t note) const noexcept;

        // 0 to 1 in, 0 to 1 out. The dialog draws the curve with this.
        double ShapeUnit(_In_ double value) const noexcept;

    private:
        uint8_t ShapeVelocity7(_In_ uint8_t velocity) const noexcept;
        uint16_t ShapeVelocity16(_In_ uint16_t velocity) const noexcept;
    };

    winrt::hstring SummarizeTransform(_In_ MessageTransform const& transform) noexcept;

    json::JsonObject TransformToJson(_In_ MessageTransform const& transform) noexcept;
    MessageTransform TransformFromJson(_In_ json::JsonObject const& object) noexcept;

    std::wstring TransformSignature(_In_ MessageTransform const& transform) noexcept;
}
