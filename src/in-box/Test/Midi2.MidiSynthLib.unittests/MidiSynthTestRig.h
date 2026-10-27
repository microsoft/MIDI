// Copyright (c) Microsoft Corporation. All rights reserved.

#pragma once

// Offline rendering for the tests that can only be checked by measuring what the synthesizer
// plays. It renders from the sound set Windows installed, and a test skips when there is none.
namespace MidiSynthTestRig
{
    // Effects off, so the voice is the only thing in the output.
    inline MidiSynth::SynthConfig TestConfig() noexcept
    {
        auto config = MidiSynth::SynthConfig::ForMode(MidiSynth::SynthMode::Modern, 48000);
        config.EnableEffects = false;

        return config;
    }

    // gm.dls, parsed once for every test that uses it. Marks the test skipped and returns nullptr
    // when the sound set can't be loaded.
    inline _Ret_maybenull_ const MidiSynth::DlsCollection* RequireSoundSet()
    {
        static MidiSynth::DlsCollection collection;

        static const bool loaded = []() noexcept
        {
            wchar_t systemDirectory[MAX_PATH]{};

            if (GetSystemDirectoryW(systemDirectory, ARRAYSIZE(systemDirectory)) == 0)
            {
                return false;
            }

            try
            {
                const std::wstring path = std::wstring{ systemDirectory } + L"\\drivers\\gm.dls";

                return MidiSynth::DlsCollection::LoadFromFile(
                    path, MidiSynth::DlsParseLimits{}, MidiSynth::SoundSetOrigin::SystemOnly, collection) == MidiSynth::DlsParseStatus::Ok;
            }
            catch (...)
            {
                return false;
            }
        }();

        if (!loaded)
        {
            WEX::Logging::Log::Result(WEX::Logging::TestResults::Skipped,
                L"The system sound set could not be loaded, so there is nothing to render with.");

            return nullptr;
        }

        return &collection;
    }

    // A fresh engine and dispatcher, so two renders can only differ by what was sent to them.
    class Rig
    {
    public:
        explicit Rig(_In_ const MidiSynth::DlsCollection& collection)
        {
            m_engine.Initialize(&collection, TestConfig());
            m_dispatcher.Initialize(&m_engine, 0, 0x0123456);
        }

        Rig(const Rig&) = delete;
        Rig& operator=(const Rig&) = delete;

        MidiSynth::SynthEngine& Engine() noexcept { return m_engine; }

        uint32_t SampleRate() const noexcept { return TestConfig().RenderSampleRate(); }

        // A MIDI 2.0 channel voice message on group 1.
        void Send(_In_ uint8_t status, _In_ uint8_t channel, _In_ uint8_t index1, _In_ uint8_t index2, _In_ uint32_t data)
        {
            const uint32_t words[2]
            {
                (4u << 28) | (static_cast<uint32_t>(status) << 20) | (static_cast<uint32_t>(channel) << 16) |
                    (static_cast<uint32_t>(index1) << 8) | index2,
                data,
            };

            m_dispatcher.ProcessWords(words, 2);
        }

        // A MIDI 1.0 channel voice message on group 1.
        void SendMidi1(_In_ uint8_t status, _In_ uint8_t channel, _In_ uint8_t data1, _In_ uint8_t data2)
        {
            const uint32_t word =
                (2u << 28) | (static_cast<uint32_t>(status) << 20) | (static_cast<uint32_t>(channel) << 16) |
                (static_cast<uint32_t>(data1 & 0x7F) << 8) | (data2 & 0x7F);

            m_dispatcher.ProcessWords(&word, 1);
        }

        // MIDI 2.0, at three quarters of full velocity.
        void NoteOn(_In_ uint8_t channel, _In_ uint8_t note, _In_ uint8_t attributeType, _In_ uint16_t attributeData)
        {
            Send(0x9, channel, note, attributeType, (0xC000u << 16) | attributeData);
        }

        void NoteOff(_In_ uint8_t channel, _In_ uint8_t note)
        {
            Send(0x8, channel, note, 0, 0x80000000u);
        }

        // A SysEx payload without F0 and F7, split into as many sysex7 packets as it needs.
        void SendSysEx(_In_ const std::vector<uint8_t>& payload)
        {
            for (size_t offset = 0; offset < payload.size(); offset += 6)
            {
                const auto count = static_cast<uint8_t>((std::min)(size_t{ 6 }, payload.size() - offset));

                const bool isFirst = (offset == 0);
                const bool isLast = (offset + count >= payload.size());
                const uint8_t status = (isFirst && isLast) ? 0 : isFirst ? 1 : isLast ? 3 : 2;

                uint8_t bytes[6]{};

                for (uint8_t i = 0; i < count; i++)
                {
                    bytes[i] = payload[offset + i];
                }

                const uint32_t words[2]
                {
                    (3u << 28) | (static_cast<uint32_t>(status) << 20) | (static_cast<uint32_t>(count) << 16) |
                        (static_cast<uint32_t>(bytes[0]) << 8) | bytes[1],
                    (static_cast<uint32_t>(bytes[2]) << 24) | (static_cast<uint32_t>(bytes[3]) << 16) |
                        (static_cast<uint32_t>(bytes[4]) << 8) | bytes[5],
                };

                m_dispatcher.ProcessWords(words, 2);
            }
        }

        std::vector<float> Render(_In_ double seconds)
        {
            const auto frames = static_cast<uint32_t>(seconds * SampleRate());

            std::vector<float> output(static_cast<size_t>(frames) * 2, 0.0f);

            for (uint32_t rendered = 0; rendered < frames;)
            {
                const uint32_t chunk = (std::min)(256u, frames - rendered);
                m_engine.Render(output.data() + static_cast<size_t>(rendered) * 2, chunk);
                rendered += chunk;
            }

            return output;
        }

    private:
        MidiSynth::SynthEngine m_engine;
        MidiSynth::UmpDispatcher m_dispatcher;
    };

    inline double LargestDifference(_In_ const std::vector<float>& a, _In_ const std::vector<float>& b)
    {
        double largest = 0.0;

        for (size_t i = 0; i < a.size() && i < b.size(); i++)
        {
            largest = (std::max)(largest, std::fabs(static_cast<double>(a[i]) - b[i]));
        }

        return largest;
    }

    inline double Peak(_In_ const std::vector<float>& samples)
    {
        double peak = 0.0;

        for (const auto sample : samples)
        {
            peak = (std::max)(peak, std::fabs(static_cast<double>(sample)));
        }

        return peak;
    }

    inline double Cents(_In_ double hertz, _In_ double referenceHertz)
    {
        return 1200.0 * std::log2(hertz / referenceHertz);
    }

    // The fundamental of a steady tone, from the normalized square difference of the signal with
    // itself, refined between samples with a parabola through the best lag. Only lags between
    // lowHertz and highHertz are searched, so a multiple of the period can't be picked instead.
    inline double EstimateHertz(
        _In_ const std::vector<float>& stereo,
        _In_ uint32_t sampleRate,
        _In_ double lowHertz,
        _In_ double highHertz)
    {
        const size_t frames = stereo.size() / 2;

        if (frames == 0)
        {
            return 0.0;
        }

        std::vector<double> mono(frames);
        double mean = 0.0;

        for (size_t i = 0; i < frames; i++)
        {
            mono[i] = 0.5 * (static_cast<double>(stereo[2 * i]) + stereo[2 * i + 1]);
            mean += mono[i];
        }

        mean /= static_cast<double>(frames);

        for (auto& sample : mono)
        {
            sample -= mean;
        }

        const auto minimumLag = (std::max)(size_t{ 2 }, static_cast<size_t>(std::floor(sampleRate / highHertz)));
        const auto maximumLag = (std::min)(frames / 3, static_cast<size_t>(std::ceil(sampleRate / lowHertz)));

        if (maximumLag <= minimumLag + 1)
        {
            return 0.0;
        }

        const size_t window = frames - maximumLag - 2;
        std::vector<double> difference(maximumLag + 2, 0.0);

        for (size_t lag = minimumLag - 1; lag <= maximumLag + 1; lag++)
        {
            double correlation = 0.0;
            double energy = 0.0;

            for (size_t i = 0; i < window; i++)
            {
                correlation += mono[i] * mono[i + lag];
                energy += mono[i] * mono[i] + mono[i + lag] * mono[i + lag];
            }

            difference[lag] = (energy > 0.0) ? 2.0 * correlation / energy : 0.0;
        }

        size_t best = 0;
        double bestValue = -2.0;

        for (size_t lag = minimumLag; lag <= maximumLag; lag++)
        {
            if (difference[lag] > bestValue)
            {
                bestValue = difference[lag];
                best = lag;
            }
        }

        if (best == 0 || bestValue < 0.3)
        {
            return 0.0;
        }

        const double before = difference[best - 1];
        const double at = difference[best];
        const double after = difference[best + 1];
        const double curvature = before - 2.0 * at + after;
        const double shift = (curvature != 0.0) ? 0.5 * (before - after) / curvature : 0.0;

        return static_cast<double>(sampleRate) / (static_cast<double>(best) + shift);
    }
}
