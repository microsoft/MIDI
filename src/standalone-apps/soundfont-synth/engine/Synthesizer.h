// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "SoundFont.h"

#include "MidiSynth/AudioEffect.h"

#include <memory>

namespace SoundFontSynth
{
    constexpr size_t MidiChannelCount = 16;

    // Registered Per-Note Controller numbers this synthesizer acts on.
    constexpr uint8_t PerNoteControllerPitch = 3;
    constexpr uint8_t PerNoteControllerVolume = 7;
    constexpr uint8_t PerNoteControllerPan = 10;

    enum class BankSelectMode : int32_t
    {
        // Roland GS until a System On message says otherwise.
        Automatic = 0,

        // The bank number is CC0. This is how most General MIDI SoundFonts number their banks.
        RolandGS,

        // Variations in CC32, with CC0 127 selecting a drum kit.
        YamahaXG,

        // Variations in CC32 under CC0 121, with CC0 120 selecting a drum kit.
        GeneralMidi2,
    };

    struct SynthesizerConfig
    {
        uint32_t SampleRate{ 48000 };

        // Voices allowed to sound at once. A few more are kept back so a stolen voice can fade
        // out instead of being cut off.
        uint32_t Polyphony{ 256 };

        bool EnableEffects{ true };

        BankSelectMode BankSelect{ BankSelectMode::Automatic };
    };

    // What a channel has selected. Readable from any thread while another renders.
    struct ChannelSelection
    {
        uint8_t BankMsb{ 0 };
        uint8_t BankLsb{ 0 };
        uint8_t Program{ 0 };
        bool IsDrumChannel{ false };

        // Index into the SoundFont's presets, or -1 when nothing in the bank can be selected.
        int32_t PresetIndex{ -1 };
    };

    struct SynthesizerStatistics
    {
        uint64_t DroppedNotes{ 0 };
        uint64_t StolenVoices{ 0 };
        uint32_t PeakVoices{ 0 };
    };

    // Plays a SoundFont. Everything except Selection and the statistics must be called from one
    // thread at a time, which is the thread that renders.
    class Synthesizer
    {
    public:
        Synthesizer() noexcept;
        ~Synthesizer();

        Synthesizer(Synthesizer const&) = delete;
        Synthesizer& operator=(Synthesizer const&) = delete;

        // Channel state survives a second Initialize, so changing the sample rate does not lose
        // the instruments a song selected.
        bool Initialize(_In_ std::shared_ptr<SoundFont const> font, _In_ SynthesizerConfig const& config);

        bool IsInitialized() const noexcept;
        uint32_t SampleRate() const noexcept;

        // Velocity is 16 bit, as MIDI 2.0 carries it. Zero is a note on, not a note off.
        void NoteOn(_In_ uint8_t channel, _In_ uint8_t note, _In_ uint16_t velocity);

        // MIDI 2.0 Pitch 7.9 attribute: the note number still chooses the zones, the pitch is
        // the fractional note the sample is tuned to.
        void NoteOnWithPitch(_In_ uint8_t channel, _In_ uint8_t note, _In_ uint16_t velocity, _In_ double pitchNoteNumber);

        void NoteOff(_In_ uint8_t channel, _In_ uint8_t note);

        void ControlChange(_In_ uint8_t channel, _In_ uint8_t controller, _In_ uint8_t value);
        void ControlChange32(_In_ uint8_t channel, _In_ uint8_t controller, _In_ uint32_t value);

        void ProgramChange(_In_ uint8_t channel, _In_ uint8_t program);
        void ProgramChangeWithBank(_In_ uint8_t channel, _In_ uint8_t bankMsb, _In_ uint8_t bankLsb, _In_ uint8_t program);

        // Fourteen bit, 8192 is centered.
        void PitchBend(_In_ uint8_t channel, _In_ uint16_t value);
        void PitchBend32(_In_ uint8_t channel, _In_ uint32_t value);

        void ChannelPressure(_In_ uint8_t channel, _In_ uint8_t value);
        void ChannelPressure32(_In_ uint8_t channel, _In_ uint32_t value);
        void PolyPressure(_In_ uint8_t channel, _In_ uint8_t note, _In_ uint8_t value);
        void PolyPressure32(_In_ uint8_t channel, _In_ uint8_t note, _In_ uint32_t value);

        // MIDI 2.0 Registered Controller, which addresses an RPN directly with a 32 bit value.
        void RegisteredController(_In_ uint8_t channel, _In_ uint8_t bank, _In_ uint8_t index, _In_ uint32_t value);

        // Normalized -1 to +1 across the channel's bend range, for every sounding note with this number.
        void PerNotePitchBend(_In_ uint8_t channel, _In_ uint8_t note, _In_ double normalized);
        void PerNoteController(_In_ uint8_t channel, _In_ uint8_t note, _In_ uint8_t controller, _In_ uint32_t value);
        void PerNoteManagement(_In_ uint8_t channel, _In_ uint8_t note, _In_ bool detach, _In_ bool resetPerNoteControllers);

        void AllSoundOff(_In_ uint8_t channel);
        void AllNotesOff(_In_ uint8_t channel);
        void ResetAllControllers(_In_ uint8_t channel);
        void SystemReset();

        void SetDrumChannel(_In_ uint8_t channel, _In_ bool isDrumChannel);
        bool IsDrumChannel(_In_ uint8_t channel) const noexcept;

        void SetBankSelectMode(_In_ BankSelectMode mode);

        // A System On or reset tells which convention the sender uses. Only Automatic follows it.
        void NotifyAddressingConvention(_In_ BankSelectMode convention);

        // GM2 device controls. Fourteen bit values, 8192 centered for the fine tuning.
        void SetMasterVolume(_In_ uint16_t value) noexcept;
        void SetMasterFineTuning(_In_ uint16_t value) noexcept;
        void SetMasterCoarseTuning(_In_ uint8_t msb) noexcept;

        // The customer's own level, kept apart from the GM2 master volume so a reset in a song
        // cannot discard it.
        void SetUserVolumeDb(_In_ double decibels) noexcept;
        double UserVolumeDb() const noexcept;

        static constexpr double MinimumUserVolumeDb{ -60.0 };
        static constexpr double MaximumUserVolumeDb{ 12.0 };

        void ActiveSensing() noexcept;

        _Ret_maybenull_ MidiSynth::IAudioEffect* Reverb() noexcept;
        _Ret_maybenull_ MidiSynth::IAudioEffect* Chorus() noexcept;

        // Adds nothing to what is already there: the output is overwritten.
        void Render(_Out_writes_(frameCount * 2) float* interleavedStereo, _In_ uint32_t frameCount) noexcept;

        uint32_t ActiveVoiceCount() const noexcept;

        ChannelSelection Selection(_In_ uint8_t channel) const noexcept;
        SynthesizerStatistics Statistics() const noexcept;

        std::shared_ptr<SoundFont const> const& Font() const noexcept;

    private:
        struct Impl;
        std::unique_ptr<Impl> m_impl;
    };
}
