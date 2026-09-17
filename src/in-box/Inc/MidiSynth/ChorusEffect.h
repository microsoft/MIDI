// Modulated delay line. Two taps in quadrature so the result is wider than a single detune.

#pragma once

#include "MidiSynth/AudioEffect.h"

#include <vector>

namespace MidiSynth
{
    class ChorusEffect final : public IAudioEffect
    {
    public:
        bool Configure(_In_ uint32_t sampleRate) override;
        void Reset() noexcept override;

        bool SetParameter(_In_ AudioEffectParameter parameter, _In_ double value) noexcept override;
        double GetParameter(_In_ AudioEffectParameter parameter) const noexcept override;

        void Process(
            _In_reads_(frameCount * 2) const float* input,
            _Inout_updates_(frameCount * 2) float* output,
            _In_ uint32_t frameCount) noexcept override;

    private:
        float ReadDelayed(_In_ const std::vector<float>& line, _In_ double delayFrames) const noexcept;

        std::vector<float> m_left;
        std::vector<float> m_right;
        size_t m_writeIndex{ 0 };

        uint32_t m_sampleRate{ 0 };

        double m_wetLevel{ 0.0 };
        double m_delaySeconds{ 0.012 };
        double m_rateHertz{ 0.8 };
        double m_depth{ 0.4 };
        double m_damping{ 0.0 };

        double m_phase{ 0.0 };
    };
}
