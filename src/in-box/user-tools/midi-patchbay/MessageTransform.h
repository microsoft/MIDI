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
    };

    constexpr size_t NoteMapSize = 128;
    constexpr size_t ControlMapSize = 128;

    constexpr int32_t MinimumTranspose = -48;
    constexpr int32_t MaximumTranspose = 48;

    // How many entries either mapping table may hold, so a hand edited file cannot grow the UI
    // without bound.
    constexpr size_t MaximumMapEntries = 128;

    // What one connection does to the messages it passes on.
    //
    // Like MessageFilter this is a plain value with no UI and no WinRT, because it runs on the
    // service callback thread for every message and a future API would expose this shape.
    struct MessageTransform
    {
        // Checked first, so an untouched transform costs one bool.
        bool IsActive{ false };

        // Applied to every note carrying message that the table below does not name.
        int32_t TransposeSemitones{ 0 };

        // -1 where a note is not remapped. An entry here wins over the transpose, so the table
        // reads as a list of exceptions.
        std::array<int16_t, NoteMapSize> NoteMap{};

        // Note on velocity only.
        VelocityCurve Curve{ VelocityCurve::Unchanged };

        bool RescaleVelocity{ false };
        uint8_t MinimumVelocity{ 1 };
        uint8_t MaximumVelocity{ 127 };

        // -1 where a controller is not remapped. Values are carried over untouched.
        std::array<int16_t, ControlMapSize> ControlMap{};

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
