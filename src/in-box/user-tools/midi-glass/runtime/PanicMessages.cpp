// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "PanicMessages.h"

#include "BindingEngine.h"

namespace glass
{
    namespace
    {
        constexpr uint8_t ControlChangeStatus = 0xB;
        constexpr uint8_t PitchBendStatus = 0xE;

        constexpr uint8_t SustainController = 64;
        constexpr uint8_t AllSoundOffController = 120;
        constexpr uint8_t AllNotesOffController = 123;
    }

    _Use_decl_annotations_
    uint32_t BuildPanicWords(uint8_t group, uint8_t channel, std::span<uint32_t> words) noexcept
    {
        if (words.size() < PanicWordsPerChannel)
        {
            return 0;
        }

        words[0] = BuildMidi1ChannelVoice(group, ControlChangeStatus, channel, SustainController, 0);
        words[1] = BuildMidi1ChannelVoice(group, ControlChangeStatus, channel, AllNotesOffController, 0);
        words[2] = BuildMidi1ChannelVoice(group, ControlChangeStatus, channel, AllSoundOffController, 0);

        // Center is 0x2000, which is LSB 0 and MSB 0x40.
        words[3] = BuildMidi1ChannelVoice(group, PitchBendStatus, channel, 0, 0x40);

        return PanicWordsPerChannel;
    }

    _Use_decl_annotations_
    uint32_t BuildPanicWordsForGroup(uint8_t group, std::span<uint32_t> words) noexcept
    {
        constexpr uint32_t total = PanicWordsPerChannel * PanicChannelCount;

        if (words.size() < total)
        {
            return 0;
        }

        uint32_t written{ 0 };

        for (uint8_t channel = 0; channel < PanicChannelCount; channel++)
        {
            written += BuildPanicWords(group, channel, words.subspan(written, PanicWordsPerChannel));
        }

        return written;
    }
}
