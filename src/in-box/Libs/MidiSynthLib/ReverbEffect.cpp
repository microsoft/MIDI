#include "MidiSynth/ReverbEffect.h"

#include <algorithm>
#include <cmath>

namespace MidiSynth
{
    namespace
    {
        // Mutually prime lengths in milliseconds, so the comb resonances do not line up and ring.
        // The right channel is offset slightly to widen the tail.
        constexpr double CombMilliseconds[]{ 29.7, 37.1, 41.1, 43.7 };
        constexpr double AllpassMilliseconds[]{ 5.0, 1.7 };
        constexpr double RightChannelOffsetMilliseconds = 0.9;

        constexpr float AllpassFeedback = 0.5f;

        size_t DelayLength(double milliseconds, uint32_t sampleRate) noexcept
        {
            const auto frames = static_cast<size_t>(milliseconds * 0.001 * sampleRate);
            return (std::max)(frames, static_cast<size_t>(1));
        }
    }

    _Use_decl_annotations_
    bool ReverbEffect::Configure(uint32_t sampleRate)
    {
        if (sampleRate == 0)
        {
            return false;
        }

        m_sampleRate = sampleRate;

        for (size_t i = 0; i < CombCount; i++)
        {
            m_combLeft[i].Buffer.assign(DelayLength(CombMilliseconds[i], sampleRate), 0.0f);
            m_combRight[i].Buffer.assign(
                DelayLength(CombMilliseconds[i] + RightChannelOffsetMilliseconds, sampleRate), 0.0f);
        }

        for (size_t i = 0; i < AllpassCount; i++)
        {
            m_allpassLeft[i].Buffer.assign(DelayLength(AllpassMilliseconds[i], sampleRate), 0.0f);
            m_allpassRight[i].Buffer.assign(
                DelayLength(AllpassMilliseconds[i] + RightChannelOffsetMilliseconds, sampleRate), 0.0f);
        }

        Reset();
        UpdateFeedback();

        return true;
    }

    void ReverbEffect::Reset() noexcept
    {
        auto clearCombs = [](auto& combs)
        {
            for (auto& comb : combs)
            {
                std::fill(comb.Buffer.begin(), comb.Buffer.end(), 0.0f);
                comb.Index = 0;
                comb.Filtered = 0.0f;
            }
        };

        auto clearAllpasses = [](auto& allpasses)
        {
            for (auto& allpass : allpasses)
            {
                std::fill(allpass.Buffer.begin(), allpass.Buffer.end(), 0.0f);
                allpass.Index = 0;
            }
        };

        clearCombs(m_combLeft);
        clearCombs(m_combRight);
        clearAllpasses(m_allpassLeft);
        clearAllpasses(m_allpassRight);
    }

    void ReverbEffect::UpdateFeedback() noexcept
    {
        if (m_sampleRate == 0)
        {
            return;
        }

        // Feedback that decays by 60 dB over the requested time, using the longest comb as the
        // representative loop length.
        const double loopSeconds = CombMilliseconds[CombCount - 1] * 0.001;
        const double time = (std::max)(m_timeSeconds, 0.05);

        m_combFeedback = static_cast<float>((std::min)(std::pow(10.0, -3.0 * loopSeconds / time), 0.98));

        m_damp1 = static_cast<float>((std::clamp)(m_damping, 0.0, 0.95));
        m_damp2 = 1.0f - m_damp1;
    }

    _Use_decl_annotations_
    bool ReverbEffect::SetParameter(AudioEffectParameter parameter, double value) noexcept
    {
        switch (parameter)
        {
        case AudioEffectParameter::WetLevel:
            m_wetLevel = (std::clamp)(value, 0.0, 1.0);
            return true;

        case AudioEffectParameter::Time:
            m_timeSeconds = (std::clamp)(value, 0.05, 20.0);
            UpdateFeedback();
            return true;

        case AudioEffectParameter::Depth:
            m_depth = (std::clamp)(value, 0.0, 1.0);
            return true;

        case AudioEffectParameter::Damping:
            m_damping = (std::clamp)(value, 0.0, 1.0);
            UpdateFeedback();
            return true;

        case AudioEffectParameter::Rate:
        default:
            return false;
        }
    }

    _Use_decl_annotations_
    double ReverbEffect::GetParameter(AudioEffectParameter parameter) const noexcept
    {
        switch (parameter)
        {
        case AudioEffectParameter::WetLevel: return m_wetLevel;
        case AudioEffectParameter::Time: return m_timeSeconds;
        case AudioEffectParameter::Depth: return m_depth;
        case AudioEffectParameter::Damping: return m_damping;
        default: return 0.0;
        }
    }

    _Use_decl_annotations_
    void ReverbEffect::Process(const float* input, float* output, uint32_t frameCount) noexcept
    {
        if (m_sampleRate == 0 || m_wetLevel <= 0.0)
        {
            return;
        }

        const auto wet = static_cast<float>(m_wetLevel);
        const auto allpassMix = static_cast<float>(0.3 + 0.4 * m_depth);

        auto runCombs = [this](auto& combs, float in) noexcept
        {
            float sum = 0.0f;

            for (auto& comb : combs)
            {
                float& sample = comb.Buffer[comb.Index];
                const float delayed = sample;

                // One pole lowpass inside the loop, so each pass loses more high frequency.
                comb.Filtered = delayed * m_damp2 + comb.Filtered * m_damp1;

                sample = in + comb.Filtered * m_combFeedback;

                if (++comb.Index >= comb.Buffer.size())
                {
                    comb.Index = 0;
                }

                sum += delayed;
            }

            return sum / static_cast<float>(CombCount);
        };

        auto runAllpasses = [allpassMix](auto& allpasses, float in) noexcept
        {
            float value = in;

            for (auto& allpass : allpasses)
            {
                float& sample = allpass.Buffer[allpass.Index];
                const float delayed = sample;

                sample = value + delayed * AllpassFeedback * allpassMix;
                value = delayed - value;

                if (++allpass.Index >= allpass.Buffer.size())
                {
                    allpass.Index = 0;
                }
            }

            return value;
        };

        for (uint32_t frame = 0; frame < frameCount; frame++)
        {
            const float left = input[frame * 2];
            const float right = input[frame * 2 + 1];

            const float wetLeft = runAllpasses(m_allpassLeft, runCombs(m_combLeft, left));
            const float wetRight = runAllpasses(m_allpassRight, runCombs(m_combRight, right));

            output[frame * 2] += wetLeft * wet;
            output[frame * 2 + 1] += wetRight * wet;
        }
    }
}
