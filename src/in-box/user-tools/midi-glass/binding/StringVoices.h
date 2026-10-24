// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Deliberately free of pch.h, XAML and the MIDI SDK. Which note each string is sounding, what a
// hammer-on or a strum has to send, and when a string stops, is bookkeeping, and a string left
// ringing after the hand has gone is the defect nobody forgives. So it is tested as bookkeeping.

#include <sal.h>
#include <array>
#include <cstdint>
#include <span>

#include "BindingEngine.h"
#include "Fretboard.h"
#include "LayoutModel.h"

namespace glass
{
    // How many semitones a full bend reaches on the wire, which the player tells the instrument
    // before the first note. A slide along the string bends as well as a push across it, so it
    // is more than the bend range. A string on a channel of its own uses 24, two octaves of
    // frets. MPE uses 48, the default the MPE profile sets.
    constexpr double FretChannelBendSensitivity = 24.0;
    constexpr double FretMpeBendSensitivity = 48.0;
    constexpr double FretPerNoteBendSensitivity = 48.0;
    // One thing a fretboard has to send. The player turns each into words on the string's
    // channel.
    enum class StringActionKind
    {
        NoteOn = 0,
        NoteOff = 1,

        // How far the string's note is bent, in semitones above the note that was struck. Sent
        // before every note on as well, so a channel left bent by the last note starts in tune.
        Bend = 2,
    };

    struct StringAction
    {
        StringActionKind Kind{ StringActionKind::NoteOn };
        uint8_t String{ 0 };
        uint8_t Note{ 0 };

        // The velocity of a note on, from 0 to 1, or the semitones of a bend.
        double Value{ 0.0 };
    };

    // The most one event on one string can need: the note off for the old note, a bend and the
    // note on for the new one.
    constexpr size_t MaximumStringActions = 4;

    // What a finger on a fretboard did.
    enum class FretTouchPhase
    {
        // Landed on a string, at a fret or in the open column.
        Down = 0,

        // Moved along its string or pushed across it.
        Move = 1,

        Up = 2,

        // Swept across a string in the strum area.
        Strum = 3,
    };

    struct FretTouch
    {
        FretTouchPhase Phase{ FretTouchPhase::Down };
        uint32_t Touch{ 0 };
        int32_t String{ -1 };
        int32_t Fret{ 0 };
        double FretPosition{ 0.0 };

        // How far the push across the string bends it, in semitones.
        double Bend{ 0.0 };

        double Velocity{ 1.0 };
    };

    // Who is fretting which string, and what each string is sounding.
    //
    // Each string sounds one note at a time, the way a real string does. The highest fret held
    // on a string is the one that sounds: fretting higher replaces the note (a hammer-on), and
    // lifting goes back to the fret still held (a pull-off).
    class StringVoices
    {
    public:
        // A finger on every string, with fingers to spare.
        static constexpr size_t MaximumFingersPerString = 4;
        static constexpr size_t MaximumTouches = 16;

        // How the fretboard plays: tuning, capo, key, play mode, slide, hammer-ons and whether
        // every string shares one channel. Notes already sounding keep sounding.
        void Configure(_In_ FretboardSpec const& spec);

        FretboardSpec const& Spec() const noexcept { return m_spec; }

        // A finger landed on a string, at a fret. Fret 0 is the open string.
        uint32_t Press(
            _In_ uint32_t touch,
            _In_ int32_t string,
            _In_ int32_t fret,
            _In_ double fretPosition,
            _In_ double velocity,
            _Out_writes_to_(actions.size(), return) std::span<StringAction> actions) noexcept;

        // A finger moved along its string, or pushed across it. Bend is how far the push across
        // bends it, in semitones. A finger stays on the string it landed on, the way a finger
        // bending a real string carries it.
        uint32_t Move(
            _In_ uint32_t touch,
            _In_ int32_t fret,
            _In_ double fretPosition,
            _In_ double bend,
            _Out_writes_to_(actions.size(), return) std::span<StringAction> actions) noexcept;

        uint32_t Release(
            _In_ uint32_t touch,
            _Out_writes_to_(actions.size(), return) std::span<StringAction> actions) noexcept;

        // A strum crossed a string: it plays at the highest fret held on it, or open.
        uint32_t Strum(
            _In_ int32_t string,
            _In_ double velocity,
            _Out_writes_to_(actions.size(), return) std::span<StringAction> actions) noexcept;

        // Every string stops. The fingers still down stay down, silent, until they play again.
        // Needs room for MaximumFretStrings actions.
        uint32_t MuteAll(_Out_writes_to_(actions.size(), return) std::span<StringAction> actions) noexcept;

        // Every finger lifted and every string stopped, when the layout closes or the page
        // changes.
        uint32_t ReleaseAll(_Out_writes_to_(actions.size(), return) std::span<StringAction> actions) noexcept;

        bool IsSounding(_In_ int32_t string) const noexcept;

        // The note a string is sounding, or -1.
        int32_t SoundingNote(_In_ int32_t string) const noexcept;

        // The highest fret held on a string, or -1 when no finger is on it.
        int32_t HeldFret(_In_ int32_t string) const noexcept;

        // The string a finger is on, or -1.
        int32_t StringOf(_In_ uint32_t touch) const noexcept;

        size_t FingerCount() const noexcept;

    private:
        struct Finger
        {
            uint32_t Touch{ 0 };
            int32_t Fret{ 0 };
            double FretPosition{ 0.0 };
            double Bend{ 0.0 };
            double Velocity{ 1.0 };

            // When it landed, so of two fingers on one fret the later one sounds.
            uint32_t Order{ 0 };
        };

        struct StringState
        {
            std::array<Finger, MaximumFingersPerString> Fingers{};
            size_t FingerCount{ 0 };

            bool Sounding{ false };
            uint8_t Note{ 0 };

            // The fret the note was struck at, which a slide bends away from.
            int32_t StruckFret{ 0 };

            // The finger that owns the note: the one that bends and slides it. 0 when the
            // string is ringing open or on its own after the finger lifted.
            uint32_t Owner{ 0 };

            double Bend{ 0.0 };
        };

        // The finger whose fret sounds, or null.
        Finger const* Active(_In_ StringState const& state) const noexcept;
        Finger* FindFinger(_In_ StringState& state, _In_ uint32_t touch) noexcept;

        // The note struck fresh: the old one ends, then the new one starts.
        void Attack(
            _In_ int32_t string,
            _In_ int32_t fret,
            _In_ double velocity,
            _In_ double bend,
            _In_ uint32_t owner,
            _Inout_ std::span<StringAction> actions,
            _Inout_ uint32_t& written) noexcept;

        // The note changed without a new attack: the new one starts before the old one ends,
        // so a mono instrument slurs from one to the other.
        void Legato(
            _In_ int32_t string,
            _In_ Finger const& finger,
            _Inout_ std::span<StringAction> actions,
            _Inout_ uint32_t& written) noexcept;

        void Stop(_In_ int32_t string, _Inout_ std::span<StringAction> actions, _Inout_ uint32_t& written) noexcept;

        // Whether another string is sounding this note. On one shared channel, two strings on
        // the same note are one note: it starts with the first and ends with the last.
        bool OtherStringSounds(_In_ int32_t string, _In_ uint8_t note) const noexcept;

        void Append(
            _Inout_ std::span<StringAction> actions,
            _Inout_ uint32_t& written,
            _In_ StringActionKind kind,
            _In_ int32_t string,
            _In_ uint8_t note,
            _In_ double value) const noexcept;

        std::array<StringState, MaximumFretStrings> m_strings{};
        FretboardSpec m_spec{};
        uint32_t m_order{ 0 };
    };

    // The channel a string plays on, as channels up from the note row's own: the row's channel
    // for every string with per-note bend, one each counted up from it, or with MPE one each
    // above the manager channel, which is the row's.
    uint8_t StringChannelOffset(_In_ FretOutput output, _In_ int32_t string) noexcept;

    // What one action becomes on the wire.
    StringSend StringSendFor(_In_ FretOutput output, _In_ StringAction const& action) noexcept;

    // What the instrument has to be told before a string's first note: how far a bend reaches,
    // on the string's channel, or with MPE on the manager channel only. False when nothing
    // needs to be.
    bool StringSetupFor(_In_ FretOutput output, _In_ int32_t string, _Out_ StringSend& setup) noexcept;
}
