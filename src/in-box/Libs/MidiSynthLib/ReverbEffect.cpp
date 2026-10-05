#include "MidiSynth/ReverbEffect.h"

#include <algorithm>
#include <cmath>

namespace MidiSynth
{
    namespace
    {
        // Freeverb's delay lengths, which Jezar placed in the public domain, converted from samples
        // at 44.1 kHz to milliseconds so they hold at any rate. They are spread so the resonances of
        // the combs do not line up and ring.
        constexpr double CombMilliseconds[]{ 25.31, 26.94, 28.96, 30.75, 32.24, 33.81, 35.31, 36.67 };
        constexpr double AllpassMilliseconds[]{ 12.61, 10.00, 7.73, 5.10 };

        // The right side runs slightly longer, which is what makes the two sides of the tail differ.
        constexpr double RightChannelOffsetMilliseconds = 0.52;

        // Puts the tail of a centered sound back at the level the earlier four comb design gave it,
        // so content sends the same amount of reverb it always has. Measured: the denser network
        // came out 6.03 dB louder for the same input.
        constexpr float OutputGain = 0.5f;

        size_t DelayLength(double milliseconds, uint32_t sampleRate) noexcept
        {
            const auto frames = static_cast<size_t>(milliseconds * 0.001 * sampleRate);
            return (std::max)(frames, static_cast<size_t>(1));
        }

        // A tail left to decay into denormal floats costs many times the processor time for no
        // audible difference, so anything this far below hearing is treated as silence.
        float FlushDenormal(float value) noexcept
        {
            return (std::fabs(value) < 1.0e-20f) ? 0.0f : value;
        }
    }

    _Use_decl_annotations_
    bool ReverbEffect::Configure(uint32_t sampleRate)
    {
        if (sampleRate == 0)
        {
            return false;
        }

        static_assert(std::size(CombMilliseconds) == CombCount);
        static_assert(std::size(AllpassMilliseconds) == AllpassCount);

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

        const double time = (std::max)(m_timeSeconds, 0.05);

        // Feedback that loses 60 dB over the requested time, worked out from each comb's own loop
        // length so that none of them rings on after the others have gone.
        auto setFeedback = [this, time](auto& combs) noexcept
        {
            for (auto& comb : combs)
            {
                const double loopSeconds = static_cast<double>(comb.Buffer.size()) / m_sampleRate;

                comb.Feedback = static_cast<float>((std::min)(std::pow(10.0, -3.0 * loopSeconds / time), 0.98));
            }
        };

        setFeedback(m_combLeft);
        setFeedback(m_combRight);

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

        const auto wet = static_cast<float>(m_wetLevel) * OutputGain;

        // Depth is the diffusion. The default lands on 0.5, the value Freeverb uses throughout.
        const auto allpassFeedback = static_cast<float>(0.2 + 0.43 * m_depth);

        auto runCombs = [this](auto& combs, float in) noexcept
        {
            float sum = 0.0f;

            for (auto& comb : combs)
            {
                float& sample = comb.Buffer[comb.Index];
                const float delayed = sample;

                // One pole lowpass inside the loop, so each pass loses more high frequency.
                comb.Filtered = FlushDenormal(delayed * m_damp2 + comb.Filtered * m_damp1);

                sample = in + comb.Filtered * comb.Feedback;

                if (++comb.Index >= comb.Buffer.size())
                {
                    comb.Index = 0;
                }

                sum += delayed;
            }

            return sum / static_cast<float>(CombCount);
        };

        auto runAllpasses = [allpassFeedback](auto& allpasses, float in) noexcept
        {
            float value = in;

            for (auto& allpass : allpasses)
            {
                float& sample = allpass.Buffer[allpass.Index];
                const float delayed = sample;

                sample = FlushDenormal(value + delayed * allpassFeedback);
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
            // Both sides are fed the same mono signal. Feeding each its own channel kept the tail
            // of a sound panned hard left on the left alone.
            const float in = (input[frame * 2] + input[frame * 2 + 1]) * 0.5f;

            const float wetLeft = runAllpasses(m_allpassLeft, runCombs(m_combLeft, in));
            const float wetRight = runAllpasses(m_allpassRight, runCombs(m_combRight, in));

            output[frame * 2] += wetLeft * wet;
            output[frame * 2 + 1] += wetRight * wet;
        }
    }
}
