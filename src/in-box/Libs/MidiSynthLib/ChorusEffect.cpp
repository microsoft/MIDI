#include "MidiSynth/ChorusEffect.h"

#include <algorithm>
#include <cmath>

namespace MidiSynth
{
    namespace
    {
        constexpr double Pi = 3.14159265358979323846;

        // Enough room for the longest delay plus full modulation swing.
        constexpr double MaxDelaySeconds = 0.050;
    }

    _Use_decl_annotations_
    bool ChorusEffect::Configure(uint32_t sampleRate)
    {
        if (sampleRate == 0)
        {
            return false;
        }

        m_sampleRate = sampleRate;

        const auto frames = static_cast<size_t>(MaxDelaySeconds * sampleRate) + 4;

        m_left.assign(frames, 0.0f);
        m_right.assign(frames, 0.0f);

        Reset();

        return true;
    }

    void ChorusEffect::Reset() noexcept
    {
        std::fill(m_left.begin(), m_left.end(), 0.0f);
        std::fill(m_right.begin(), m_right.end(), 0.0f);

        m_writeIndex = 0;
        m_phase = 0.0;
    }

    _Use_decl_annotations_
    bool ChorusEffect::SetParameter(AudioEffectParameter parameter, double value) noexcept
    {
        switch (parameter)
        {
        case AudioEffectParameter::WetLevel:
            m_wetLevel = (std::clamp)(value, 0.0, 1.0);
            return true;

        case AudioEffectParameter::Time:
            m_delaySeconds = (std::clamp)(value, 0.001, 0.030);
            return true;

        case AudioEffectParameter::Rate:
            m_rateHertz = (std::clamp)(value, 0.05, 10.0);
            return true;

        case AudioEffectParameter::Depth:
            m_depth = (std::clamp)(value, 0.0, 1.0);
            return true;

        case AudioEffectParameter::Damping:
            m_damping = (std::clamp)(value, 0.0, 1.0);
            return true;

        default:
            return false;
        }
    }

    _Use_decl_annotations_
    double ChorusEffect::GetParameter(AudioEffectParameter parameter) const noexcept
    {
        switch (parameter)
        {
        case AudioEffectParameter::WetLevel: return m_wetLevel;
        case AudioEffectParameter::Time: return m_delaySeconds;
        case AudioEffectParameter::Rate: return m_rateHertz;
        case AudioEffectParameter::Depth: return m_depth;
        case AudioEffectParameter::Damping: return m_damping;
        default: return 0.0;
        }
    }

    _Use_decl_annotations_
    float ChorusEffect::ReadDelayed(const std::vector<float>& line, double delayFrames) const noexcept
    {
        const auto size = line.size();

        if (size == 0)
        {
            return 0.0f;
        }

        // Clamped so a parameter change mid block can never index outside the line.
        const double clamped = (std::clamp)(delayFrames, 1.0, static_cast<double>(size - 2));

        const double readPosition = static_cast<double>(m_writeIndex) + static_cast<double>(size) - clamped;

        const auto base = static_cast<size_t>(readPosition) % size;
        const auto next = (base + 1) % size;

        const auto fraction = static_cast<float>(readPosition - std::floor(readPosition));

        return line[base] + (line[next] - line[base]) * fraction;
    }

    _Use_decl_annotations_
    void ChorusEffect::Process(const float* input, float* output, uint32_t frameCount) noexcept
    {
        if (m_sampleRate == 0 || m_wetLevel <= 0.0 || m_left.empty())
        {
            return;
        }

        const auto wet = static_cast<float>(m_wetLevel);

        const double centerFrames = m_delaySeconds * m_sampleRate;
        const double swingFrames = centerFrames * 0.5 * m_depth;
        const double phaseStep = 2.0 * Pi * m_rateHertz / m_sampleRate;

        // The modulator runs at a few hertz against a sample rate in the tens of thousands, so it
        // is evaluated once per short chunk and interpolated. Over one chunk at the fastest rate
        // the phase moves about 0.04 radians, where a straight line is indistinguishable from the
        // curve.
        constexpr uint32_t LfoChunkFrames = 32;

        uint32_t frame = 0;

        while (frame < frameCount)
        {
            const uint32_t chunk = (std::min)(LfoChunkFrames, frameCount - frame);

            const double startSin = std::sin(m_phase);
            const double startCos = std::cos(m_phase);

            const double endPhase = m_phase + phaseStep * chunk;

            const double endSin = std::sin(endPhase);
            const double endCos = std::cos(endPhase);

            const double sinStep = (endSin - startSin) / chunk;
            const double cosStep = (endCos - startCos) / chunk;

            double modulationSin = startSin;
            double modulationCos = startCos;

            for (uint32_t i = 0; i < chunk; i++)
            {
                const uint32_t index = frame + i;

                m_left[m_writeIndex] = input[index * 2];
                m_right[m_writeIndex] = input[index * 2 + 1];

                // Quadrature taps, so the two sides sweep out of step and the result is wide.
                const double leftDelay = centerFrames + swingFrames * modulationSin;
                const double rightDelay = centerFrames + swingFrames * modulationCos;

                output[index * 2] += ReadDelayed(m_left, leftDelay) * wet;
                output[index * 2 + 1] += ReadDelayed(m_right, rightDelay) * wet;

                if (++m_writeIndex >= m_left.size())
                {
                    m_writeIndex = 0;
                }

                modulationSin += sinStep;
                modulationCos += cosStep;
            }

            m_phase = endPhase;

            if (m_phase >= 2.0 * Pi)
            {
                m_phase -= 2.0 * Pi;
            }

            frame += chunk;
        }
    }
}
