// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Plays a synthesizer offline through its dispatcher, the way messages arrive from the wire, for
// the tests that can only be checked by measuring what it plays.

#pragma once

namespace SoundModuleTestRig
{
    // The test sines (Sf2Test::MakeSine) play A440 on key 69, so this is where any pitch lands.
    inline double HertzOf(_In_ double pitch) noexcept
    {
        return 440.0 * std::pow(2.0, (pitch - 69.0) / 12.0);
    }

    inline std::shared_ptr<SoundFontSynth::SoundFont const> LoadFont(_In_ Sf2Test::Font const& source)
    {
        auto const bytes = Sf2Test::Build(source);

        SoundFontSynth::MemoryByteSource bytesSource(bytes.data(), bytes.size());

        auto font = std::make_shared<SoundFontSynth::SoundFont>();

        if (SoundFontSynth::SoundFont::Load(bytesSource, SoundFontSynth::Sf2LoadLimits{}, *font, nullptr) !=
            SoundFontSynth::Sf2LoadStatus::Ok)
        {
            return nullptr;
        }

        return font;
    }

    // One synthesizer and the dispatcher in front of it, fresh for each render.
    class Rig
    {
    public:
        bool Initialize(
            _In_ std::shared_ptr<SoundFontSynth::SoundFont const> const& font,
            _In_ uint32_t sampleRate = 48000,
            _In_ SoundFontSynth::BankSelectMode bankSelect = SoundFontSynth::BankSelectMode::Automatic)
        {
            SoundFontSynth::SynthesizerConfig config{};
            config.SampleRate = sampleRate;
            config.EnableEffects = false;
            config.BankSelect = bankSelect;

            if (font == nullptr || !m_synthesizer.Initialize(font, config))
            {
                return false;
            }

            m_dispatcher.Initialize(&m_synthesizer, 0, 0);

            return true;
        }

        SoundFontSynth::Synthesizer& Engine() noexcept { return m_synthesizer; }

        // A MIDI 2.0 channel voice message on group 1.
        void Send(_In_ uint8_t status, _In_ uint8_t channel, _In_ uint8_t index1, _In_ uint8_t index2, _In_ uint32_t data)
        {
            uint32_t const words[2]
            {
                (4u << 28) | (static_cast<uint32_t>(status) << 20) | (static_cast<uint32_t>(channel) << 16) |
                    (static_cast<uint32_t>(index1) << 8) | index2,
                data,
            };

            (void)m_dispatcher.ProcessWords(words, 2);
        }

        // A MIDI 1.0 channel voice message on group 1.
        void SendMidi1(_In_ uint8_t status, _In_ uint8_t channel, _In_ uint8_t data1, _In_ uint8_t data2)
        {
            uint32_t const word =
                (2u << 28) | (static_cast<uint32_t>(status) << 20) | (static_cast<uint32_t>(channel) << 16) |
                (static_cast<uint32_t>(data1 & 0x7F) << 8) | (data2 & 0x7F);

            (void)m_dispatcher.ProcessWords(&word, 1);
        }

        // A whole System Exclusive message on group 1, without F0 and F7, six bytes to a packet.
        void SendSysEx(_In_ std::vector<uint8_t> const& payload)
        {
            for (size_t offset = 0; offset < payload.size(); offset += 6)
            {
                auto const count = (std::min)(size_t{ 6 }, payload.size() - offset);
                auto const first = offset == 0;
                auto const last = offset + count >= payload.size();

                // 0 is a complete message, 1 starts one, 2 continues it and 3 ends it.
                uint32_t const status = (first && last) ? 0u : (first ? 1u : (last ? 3u : 2u));

                uint8_t bytes[6]{};

                for (size_t i = 0; i < count; i++)
                {
                    bytes[i] = payload[offset + i] & 0x7F;
                }

                uint32_t const words[2]
                {
                    (3u << 28) | (status << 20) | (static_cast<uint32_t>(count) << 16) |
                        (static_cast<uint32_t>(bytes[0]) << 8) | bytes[1],
                    (static_cast<uint32_t>(bytes[2]) << 24) | (static_cast<uint32_t>(bytes[3]) << 16) |
                        (static_cast<uint32_t>(bytes[4]) << 8) | bytes[5],
                };

                (void)m_dispatcher.ProcessWords(words, 2);
            }
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

        std::vector<float> Render(_In_ double seconds)
        {
            auto const frames = static_cast<uint32_t>(seconds * m_synthesizer.SampleRate());

            std::vector<float> output(static_cast<size_t>(frames) * 2);

            for (uint32_t done = 0; done < frames;)
            {
                auto const block = (std::min)(frames - done, 256u);
                m_synthesizer.Render(output.data() + static_cast<size_t>(done) * 2, block);
                done += block;
            }

            return output;
        }

    private:
        SoundFontSynth::Synthesizer m_synthesizer;
        SoundFontSynth::SynthDispatcher m_dispatcher;
    };

    // Upward zero crossings per second on the left channel. Measure after the start of a note: the
    // first step up out of silence is not a real crossing.
    inline double MeasureFrequency(_In_ std::vector<float> const& stereo, _In_ uint32_t sampleRate)
    {
        uint32_t crossings{ 0 };
        size_t first{ SIZE_MAX };
        size_t last{ 0 };

        for (size_t i = 2; i < stereo.size(); i += 2)
        {
            if (stereo[i - 2] <= 0.0f && stereo[i] > 0.0f)
            {
                if (first == SIZE_MAX)
                {
                    first = i;
                }
                else
                {
                    crossings++;
                }

                last = i;
            }
        }

        if (first == SIZE_MAX || last <= first)
        {
            return 0.0;
        }

        return static_cast<double>(crossings) * sampleRate / (static_cast<double>(last - first) / 2.0);
    }

    inline double LargestDifference(_In_ std::vector<float> const& a, _In_ std::vector<float> const& b)
    {
        double largest{ 0.0 };

        for (size_t i = 0; i < a.size() && i < b.size(); i++)
        {
            largest = (std::max)(largest, std::fabs(static_cast<double>(a[i]) - b[i]));
        }

        return largest;
    }

    inline bool IsNear(_In_ double measured, _In_ double expected, _In_ double fraction) noexcept
    {
        return std::fabs(measured - expected) <= expected * fraction;
    }
}
