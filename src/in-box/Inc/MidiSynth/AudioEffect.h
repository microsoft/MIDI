// Deliberately small effect abstraction. The goal is only that an effect can be replaced without
// touching the engine, and that they all look alike from the outside. It is not trying to be APO,
// VST or CLAP.

#pragma once

#include <sal.h>

#include <cstdint>

namespace MidiSynth
{
    // A shared vocabulary so every effect is configured the same way. An effect ignores what does
    // not apply to it and says so by returning false, rather than silently accepting the value.
    enum class AudioEffectParameter
    {
        // 0 to 1. How much processed signal is added back into the mix.
        WetLevel,

        // Seconds. Reverb decay time, or chorus base delay.
        Time,

        // Hertz. Modulation rate. Reverb does not use this.
        Rate,

        // 0 to 1. Modulation depth, or reverb diffusion.
        Depth,

        // 0 to 1. How quickly high frequencies are lost.
        Damping,
    };

    // All effects are stereo in, stereo out, interleaved, and additive: Process mixes its result
    // into the output rather than replacing it, so a send bus does not need a separate adder.
    struct IAudioEffect
    {
        virtual ~IAudioEffect() = default;

        // Allocates. Called when the sample rate is known or changes, never on the audio thread.
        virtual bool Configure(_In_ uint32_t sampleRate) = 0;

        // Clears delay lines and history without disturbing parameters.
        virtual void Reset() noexcept = 0;

        // Returns false when the effect has no such parameter, so a caller setting something
        // meaningless finds out instead of assuming it worked.
        virtual bool SetParameter(_In_ AudioEffectParameter parameter, _In_ double value) noexcept = 0;
        virtual double GetParameter(_In_ AudioEffectParameter parameter) const noexcept = 0;

        // Audio thread. Must not block, allocate, lock or throw.
        virtual void Process(
            _In_reads_(frameCount * 2) const float* input,
            _Inout_updates_(frameCount * 2) float* output,
            _In_ uint32_t frameCount) noexcept = 0;
    };
}
