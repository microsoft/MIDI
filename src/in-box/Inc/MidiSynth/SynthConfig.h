// Engine configuration. The Compatible profile reproduces what the in-box synth does today,
// including its limits; the Modern profile removes them.

#pragma once

#include <sal.h>

#include <cstdint>

namespace MidiSynth
{
    enum class SynthMode
    {
        // Matches the observable behavior of the in-box Microsoft GS Wavetable Synth.
        Compatible,

        // Same sound set, without the historic limits.
        Modern,
    };

    enum class InterpolationQuality
    {
        // Nearest neighbor. Present because it is part of the lo-fi character, not because
        // anyone should choose it on merit.
        None,

        Linear,

        // Fourth order Hermite. Default for Modern.
        Cubic,
    };

    enum class VoiceStealingPolicy
    {
        // What the older in-box synthesizer does. Channel 10 has the highest priority, then
        // channels 1 to 9, then 11 to 16. A note may take a voice from its own priority or lower:
        // a released note first, the quietest of those, otherwise the oldest.
        ChannelPriority,

        // Prefer voices already releasing, then the quietest, then the oldest.
        QuietestReleasing,
    };

    // How an incoming bank select is read.
    //
    // This matters because gm.dls is addressed the Roland GS way: the variation lives in the bank
    // MSB and the LSB is always zero. A file written for XG puts the variation in the LSB and a
    // file written for GM2 fixes the MSB at 121, so on either one every variation lookup misses and
    // silently falls back to the capital tone. Measured over 175,163 real files, XG style LSB
    // selects outnumber GM2 style by more than twenty to one, so this is not a rare case.
    enum class BankSelectMode
    {
        // Native addressing for this sound set.
        RolandGS = 0,

        // Yamaha XG: variation in the LSB. MSB 127 and 126 select drum kits.
        YamahaXG,

        // General MIDI 2: MSB 121 melodic, MSB 120 drum kit, variation in the LSB.
        GeneralMidi2,

        // Follow whichever System On or reset the sender last sent, starting from GS.
        Automatic,
    };

    struct SynthConfig
    {
        SynthMode Mode{ SynthMode::Modern };

        // Rate the engine renders at. The audio sink supplies this; for WASAPI shared mode that
        // is the mix format rate, so the audio engine does no conversion and no device setting
        // is disturbed. Compatible mode renders internally at 22050 regardless, then resamples,
        // because the bandwidth limit is part of the sound being reproduced.
        uint32_t OutputSampleRate{ 48000 };

        uint32_t MaxVoices{ 256 };

        // Voices beyond MaxVoices that a burst of notes can use for a moment. Each render then fades
        // out the oldest notes on the lowest priority channel until no more than MaxVoices are left,
        // which is how the older synthesizer frees room before it has to steal.
        uint32_t SpareVoices{ 0 };

        InterpolationQuality Interpolation{ InterpolationQuality::Cubic };

        VoiceStealingPolicy StealingPolicy{ VoiceStealingPolicy::QuietestReleasing };

        BankSelectMode BankSelect{ BankSelectMode::Automatic };

        // Applied to the whole mix. Set so output level matches the in-box synth, measured by
        // capturing it and correcting for the loopback path gain: a drop-in replacement that is
        // quieter or louder than what it replaces changes how every existing MIDI file sounds.
        double MasterGainDb{ 0.0 };

        // Measured: the mix reaches full scale at about 20 simultaneous notes. The in-box synth
        // clips too and is documented as doing so, so Compatible keeps that; Modern cannot, since
        // it allows far more voices. Lowering the master gain instead would break the level match.
        bool EnableLimiter{ true };

        // The in-box synth has no reverb or chorus at all, so Compatible has none either. Also
        // turned off by the offline analysis modes, where a reverb tail would smear the very thing
        // being measured.
        bool EnableEffects{ true };

        // General MIDI 2 recommends that drums ignore modulation, the damper pedal, pitch bend, pitch
        // bend sensitivity, and channel and master tuning. The older synthesizer predates that: its
        // drums bend, take modulation, the damper and fine tuning, and skip only coarse tuning, so
        // Compatible does the same.
        bool DrumsFollowGeneralMidi2{ true };

        // Channel and master tuning normally retune notes that are already sounding. The older
        // synthesizer read the tuning once, when each note started, so in Compatible a change
        // applies only to the notes that follow it.
        bool TuningFixedAtNoteOn{ false };

        // Control rate divisor. Envelopes and modulation update every this many frames, with
        // gain ramped across the block so nothing zippers.
        uint32_t ControlRateFrames{ 32 };

        static SynthConfig ForMode(_In_ SynthMode mode, _In_ uint32_t outputSampleRate) noexcept
        {
            SynthConfig config;
            config.Mode = mode;
            config.OutputSampleRate = outputSampleRate;

            if (mode == SynthMode::Compatible)
            {
                config.MaxVoices = 32;
                config.Interpolation = InterpolationQuality::Linear;
                config.StealingPolicy = VoiceStealingPolicy::ChannelPriority;
                config.SpareVoices = 8;
                config.EnableLimiter = false;
                config.EnableEffects = false;

                // Reproducing the older synthesizer means reproducing the misses too.
                config.BankSelect = BankSelectMode::RolandGS;

                config.DrumsFollowGeneralMidi2 = false;
                config.TuningFixedAtNoteOn = true;
            }

            return config;
        }

        // Rate the voice mixer runs at, which is not always the output rate.
        uint32_t RenderSampleRate() const noexcept
        {
            return Mode == SynthMode::Compatible ? 22050u : OutputSampleRate;
        }
    };
}
