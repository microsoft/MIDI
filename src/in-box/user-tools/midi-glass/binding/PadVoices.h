// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Deliberately free of pch.h, XAML and the MIDI SDK. Which note a finger is holding, and what a
// slide onto the next pad has to send, is bookkeeping, and a note left sounding after the finger
// has gone is the defect nobody forgives. So it is tested as bookkeeping.

#include <sal.h>
#include <array>
#include <cstdint>
#include <span>

#include "LayoutModel.h"

namespace glass
{
    // One thing a pad grid has to send. The binding engine turns each into words.
    enum class PadActionKind
    {
        NoteOn = 0,
        NoteOff = 1,

        // Control change 84, naming the note the next note on glides from.
        PortamentoFrom = 2,

        // MIDI 2.0 per-note pitch bend, in semitones from the note that was struck.
        Bend = 3,

        // How far a per-note pitch bend reaches either way, in semitones.
        BendRange = 4,
    };

    struct PadAction
    {
        PadActionKind Kind{ PadActionKind::NoteOn };
        uint8_t Note{ 0 };

        // The velocity of a note on, from 0 to 1; semitones for a bend or a bend range.
        double Value{ 0.0 };
    };

    // The most one finger can need in one event: a press that bends is four, a slide that
    // glides is three.
    constexpr size_t MaximumPadActions = 4;

    // The smallest change in a bend worth sending, in semitones. One cent: finer than any ear,
    // and coarse enough that a finger resting on glass sends nothing.
    constexpr double PadBendStep = 0.01;

    // Who is holding which note on one pad grid.
    //
    // A finger is known by its pointer id, so two fingers are two notes and a chord is a chord.
    // Two fingers on the same note share it: the note starts with the first and ends with the
    // last, because a second note on for a note already sounding is a retrigger on most
    // instruments and the first release would cut off the finger still holding it.
    class PadVoices
    {
    public:
        // As many fingers as anybody has, and a few for the pen and the mouse.
        static constexpr size_t MaximumTouches = 16;

        void Configure(_In_ PadGlide glide, _In_ int32_t bendRangeSemitones) noexcept;

        PadGlide Glide() const noexcept { return m_glide; }

        // A finger landed on a pad. Pitch is where it landed, for a note that bends.
        uint32_t Press(
            _In_ uint32_t touch,
            _In_ int32_t note,
            _In_ double velocity,
            _In_ double pitch,
            _Inout_ std::span<PadAction> actions) noexcept;

        // A finger moved. Note is the pad under it now, -1 for none; pitch is the pitch under it,
        // which only a bending note listens to.
        uint32_t Move(
            _In_ uint32_t touch,
            _In_ int32_t note,
            _In_ double velocity,
            _In_ double pitch,
            _Inout_ std::span<PadAction> actions) noexcept;

        // A finger came up. It ends the note it is actually holding, which after a slide is not
        // the one it landed on.
        uint32_t Release(_In_ uint32_t touch, _Inout_ std::span<PadAction> actions) noexcept;

        // Every note still sounding, ended. Needs room for MaximumTouches actions.
        uint32_t ReleaseAll(_Inout_ std::span<PadAction> actions) noexcept;

        size_t HeldCount() const noexcept;

    private:
        struct Voice
        {
            bool InUse{ false };
            uint32_t Touch{ 0 };

            // The note this finger is holding. For a note that bends, the one it struck.
            uint8_t Note{ 0 };

            // Whether this finger started the note or joined somebody else's. Only the finger
            // that owns a note may bend it or glide it away.
            bool Owns{ false };

            // The last bend sent for it, so a finger that has not moved sends nothing.
            double Bend{ 0.0 };
        };

        Voice* Find(_In_ uint32_t touch) noexcept;

        // The note one finger lets go of. Ends it when nobody else holds it, and hands it to a
        // finger that does when the one letting go owned it.
        void LetGo(_Inout_ Voice& voice, _Inout_ std::span<PadAction> actions, _Inout_ uint32_t& written) noexcept;

        std::array<Voice, MaximumTouches> m_voices{};

        // How many fingers are holding each note.
        std::array<uint8_t, 128> m_holders{};

        PadGlide m_glide{ PadGlide::Off };
        int32_t m_bendRange{ 48 };
    };
}
