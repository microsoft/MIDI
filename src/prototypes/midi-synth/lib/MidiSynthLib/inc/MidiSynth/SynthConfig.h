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
        // What the in-box synth does: lower channel numbers win.
        LowestChannelPriority,

        // Prefer voices already releasing, then the quietest, then the oldest.
        QuietestReleasing,
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

        InterpolationQuality Interpolation{ InterpolationQuality::Cubic };

        VoiceStealingPolicy StealingPolicy{ VoiceStealingPolicy::QuietestReleasing };

        // Applied to the whole mix. Set so output level matches the in-box synth, measured by
        // capturing it and correcting for the loopback path gain: a drop-in replacement that is
        // quieter or louder than what it replaces changes how every existing MIDI file sounds.
        double MasterGainDb{ -1.3 };

        // Measured: the mix reaches full scale at about 20 simultaneous notes. The in-box synth
        // clips too and is documented as doing so, so Compatible keeps that; Modern cannot, since
        // it allows far more voices. Lowering the master gain instead would break the level match.
        bool EnableLimiter{ true };

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
                config.StealingPolicy = VoiceStealingPolicy::LowestChannelPriority;
                config.EnableLimiter = false;
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
