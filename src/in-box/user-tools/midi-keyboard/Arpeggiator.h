// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "AppSettings.h"

namespace midikeyboard
{
    // Plays the held keys one at a time. The held keys are what the player is touching; the
    // notes this sends are separate, so releasing a key never leaves an arpeggiated note on.
    //
    // Everything here runs on the UI thread, driven by a dispatcher queue timer. That is
    // accurate to roughly a frame, which is fine for playing along with, and it keeps note
    // ordering identical to the keys the player is holding with no locking.
    class Arpeggiator
    {
    public:
        using NoteOnHandler = std::function<void(int32_t noteNumber, uint16_t velocity)>;
        using NoteOffHandler = std::function<void(int32_t noteNumber)>;

        void Initialize(
            winrt::Microsoft::UI::Dispatching::DispatcherQueue const& queue,
            NoteOnHandler onNoteOn,
            NoteOffHandler onNoteOff) noexcept;

        void Shutdown() noexcept;

        void Mode(ArpeggiatorMode value) noexcept;
        ArpeggiatorMode Mode() const noexcept { return m_mode; }

        bool IsEnabled() const noexcept { return m_mode != ArpeggiatorMode::Off; }

        void Rate(uint32_t bpm, ArpeggiatorDivision division) noexcept;

        void HoldNote(int32_t noteNumber, uint16_t velocity) noexcept;
        void ReleaseNote(int32_t noteNumber) noexcept;

        // silences whatever is sounding and forgets the held keys
        void Reset() noexcept;

        // the note the arpeggiator is sounding right now, or -1
        int32_t SoundingNote() const noexcept { return m_soundingNote; }

        static uint32_t StepsPerBeat(ArpeggiatorDivision division) noexcept;

    private:
        struct HeldNote
        {
            int32_t NoteNumber{ 0 };
            uint16_t Velocity{ 0 };
        };

        void Start() noexcept;
        void Stop() noexcept;
        void ApplyInterval() noexcept;
        void OnStep() noexcept;
        void SilenceSoundingNote() noexcept;
        std::vector<HeldNote> BuildSequence() const noexcept;

        winrt::Microsoft::UI::Dispatching::DispatcherQueue m_queue{ nullptr };
        winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer m_stepTimer{ nullptr };
        winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer m_gateTimer{ nullptr };

        NoteOnHandler m_onNoteOn{};
        NoteOffHandler m_onNoteOff{};

        ArpeggiatorMode m_mode{ ArpeggiatorMode::Off };
        uint32_t m_bpm{ 120 };
        ArpeggiatorDivision m_division{ ArpeggiatorDivision::Sixteenth };

        std::vector<HeldNote> m_held{};
        size_t m_stepIndex{ 0 };
        int32_t m_soundingNote{ -1 };

        std::mt19937 m_random{ std::random_device{}() };
    };
}
