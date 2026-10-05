// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Pure, like MessageFilter.h. The summaries a customer reads are in MessageText.h.

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

    // How a controller or aftertouch value rises from the bottom of its range to the top.
    enum class ValueCurve : int32_t
    {
        Linear = 0,

        // Rises slowly at first, which gives finer control near the bottom.
        SlowRise = 1,

        // Rises quickly at first, which gives finer control near the top.
        FastRise = 2,
    };

    // Reshapes one continuous value, in this order: find where it sits in the input range,
    // flip it, bend it, then place it in the output range. Ranges are in hundredths of a percent.
    // A range typed high to low runs backwards, so the numbers always mean what they say.
    struct ValueShape
    {
        bool Invert{ false };
        ValueCurve Curve{ ValueCurve::Linear };

        // Anything outside the input range is held at its nearest end, so a pedal that never
        // reaches either end can still cover the whole output range.
        int32_t InputMinimumHundredths{ 0 };
        int32_t InputMaximumHundredths{ FullScaleHundredths };

        int32_t OutputMinimumHundredths{ 0 };
        int32_t OutputMaximumHundredths{ FullScaleHundredths };

        bool ChangesNothing() const noexcept;

        // 0 to 1 in, 0 to 1 out. The dialog draws the preview with this.
        double ShapeUnit(_In_ double value) const noexcept;

        // Worked out in whole MIDI 1.0 steps, so a range typed as 0 to 127 lands on exactly the
        // steps that were typed.
        uint8_t Shape7(_In_ uint8_t value) const noexcept;

        uint32_t Shape32(_In_ uint32_t value) const noexcept;
    };

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

        // -1 where a controller is not remapped. The map moves the controller; the value shapes
        // below are what change its value.
        std::array<int16_t, ControlMapSize> ControlMap{};

        // Indexed by the controller number AFTER the map above, so a shape describes what the
        // destination receives. A shape that changes nothing is the same as no shape.
        std::array<ValueShape, ControlMapSize> ControlValueShapes{};

        // Channel pressure and poly pressure. A pressure of zero always goes out as zero, because
        // it means the key was let go.
        ValueShape AftertouchShape{};

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

    json::JsonObject TransformToJson(_In_ MessageTransform const& transform) noexcept;
    MessageTransform TransformFromJson(_In_ json::JsonObject const& object) noexcept;

    std::wstring TransformSignature(_In_ MessageTransform const& transform) noexcept;

    // The pieces of the transform file format that the blocks reuse, so a block writes a map or
    // a shape exactly the way a whole transform does.
    json::JsonArray MapToJson(_In_reads_(count) int16_t const* map, _In_ size_t count) noexcept;

    // Fills the whole map: -1 everywhere the array does not name.
    void MapFromJson(
        _In_ json::JsonObject const& object,
        _In_ std::wstring_view key,
        _Out_writes_(count) int16_t* map,
        _In_ size_t count) noexcept;

    json::JsonObject ShapeToJson(_In_ ValueShape const& shape, _In_ bool includeInvert) noexcept;
    ValueShape ShapeFromJson(_In_ json::JsonObject const& object, _In_ bool includeInvert) noexcept;

    json::JsonArray ControlValueShapesToJson(_In_ std::array<ValueShape, ControlMapSize> const& shapes) noexcept;
    void ControlValueShapesFromJson(
        _In_ json::JsonObject const& object,
        _Inout_ std::array<ValueShape, ControlMapSize>& shapes) noexcept;

    std::wstring ShapeSignature(_In_ ValueShape const& shape);

    // How many entries of a map move something somewhere else.
    size_t CountMapEntries(_In_reads_(count) int16_t const* map, _In_ size_t count) noexcept;
}
