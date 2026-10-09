// Schroeder reverberator: parallel comb filters feeding series allpass sections, laid out the way
// Jezar's public domain Freeverb lays them out. The input is summed to mono and both sides are fed
// from it with slightly different delay lengths, so the tail fills the stereo field no matter where
// the dry sound is panned.

#pragma once

#include "MidiSynth/AudioEffect.h"

#include <array>
#include <vector>

namespace MidiSynth
{
    class ReverbEffect final : public IAudioEffect
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
        // Eight combs and four allpasses per side. Four and two gave a few hundred echoes a second,
        // which is heard as a grainy, fluttering tail rather than a room.
        static constexpr size_t CombCount = 8;
        static constexpr size_t AllpassCount = 4;

        struct Comb
        {
            std::vector<float> Buffer;
            size_t Index{ 0 };
            float Filtered{ 0.0f };

            // Per comb, so every one of them takes the requested time to die away.
            float Feedback{ 0.0f };
        };

        struct Allpass
        {
            std::vector<float> Buffer;
            size_t Index{ 0 };
        };

        void UpdateFeedback() noexcept;

        std::array<Comb, CombCount> m_combLeft;
        std::array<Comb, CombCount> m_combRight;
        std::array<Allpass, AllpassCount> m_allpassLeft;
        std::array<Allpass, AllpassCount> m_allpassRight;

        uint32_t m_sampleRate{ 0 };

        double m_wetLevel{ 0.0 };
        double m_timeSeconds{ 1.5 };
        double m_depth{ 0.7 };
        double m_damping{ 0.4 };

        float m_damp1{ 0.0f };
        float m_damp2{ 0.0f };
    };
}
