// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "MessageTranslation.h"

#include "SequenceModel.h"

#include <algorithm>

namespace midisequencer
{
    namespace
    {
        void AddOneWord(_Inout_ TranslatedMessages& result, _In_ uint32_t word) noexcept
        {
            if (result.Count < result.Messages.size())
            {
                result.Messages[result.Count][0] = word;
                result.WordCounts[result.Count] = 1;
                ++result.Count;
            }
        }

        void PassThrough(_Inout_ TranslatedMessages& result, _In_reads_(wordCount) uint32_t const* words, _In_ uint8_t wordCount) noexcept
        {
            auto const count = std::min<uint8_t>(wordCount, 4);
            std::copy_n(words, count, result.Messages[0].begin());
            result.WordCounts[0] = count;
            result.Count = count > 0 ? 1 : 0;
        }

        uint32_t Midi1(_In_ uint32_t group, _In_ uint32_t status, _In_ uint32_t channel, _In_ uint32_t data1, _In_ uint32_t data2) noexcept
        {
            return 0x20000000u | (group << 24) | (status << 20) | (channel << 16) | ((data1 & 0x7F) << 8) | (data2 & 0x7F);
        }

        uint32_t ControlChange(_In_ uint32_t group, _In_ uint32_t channel, _In_ uint32_t controller, _In_ uint32_t value) noexcept
        {
            return Midi1(group, 0xB, channel, controller, value);
        }
    }

    _Use_decl_annotations_
    TranslatedMessages TranslateToMidi1(uint32_t const* words, uint8_t wordCount) noexcept
    {
        TranslatedMessages result{};

        if (wordCount == 0)
        {
            return result;
        }

        auto const word0 = words[0];

        if ((word0 >> 28) != 0x4 || wordCount < 2)
        {
            PassThrough(result, words, wordCount);
            return result;
        }

        auto const word1 = words[1];
        auto const group = (word0 >> 24) & 0x0F;
        auto const status = (word0 >> 20) & 0x0F;
        auto const channel = (word0 >> 16) & 0x0F;
        auto const index1 = (word0 >> 8) & 0xFF;
        auto const index2 = word0 & 0xFF;

        switch (status)
        {
        case 0x9:
        {
            // A MIDI 1.0 note on at velocity 0 is a note off, so the quietest note is 1.
            auto const velocity = std::max<uint32_t>(1, ScaleDown(word1 >> 16, 16, 7));
            AddOneWord(result, Midi1(group, 0x9, channel, index1, velocity));
            break;
        }

        case 0x8:
            AddOneWord(result, Midi1(group, 0x8, channel, index1, ScaleDown(word1 >> 16, 16, 7)));
            break;

        case 0xA:
            AddOneWord(result, Midi1(group, 0xA, channel, index1, ScaleDown(word1, 32, 7)));
            break;

        case 0xB:
            AddOneWord(result, ControlChange(group, channel, index1, ScaleDown(word1, 32, 7)));
            break;

        case 0xC:
            if ((index2 & 0x01) != 0)
            {
                AddOneWord(result, ControlChange(group, channel, 0, (word1 >> 8) & 0x7F));
                AddOneWord(result, ControlChange(group, channel, 32, word1 & 0x7F));
            }

            AddOneWord(result, Midi1(group, 0xC, channel, word1 >> 24, 0));
            break;

        case 0xD:
            AddOneWord(result, Midi1(group, 0xD, channel, ScaleDown(word1, 32, 7), 0));
            break;

        case 0xE:
        {
            auto const bend = ScaleDown(word1, 32, 14);
            AddOneWord(result, Midi1(group, 0xE, channel, bend & 0x7F, (bend >> 7) & 0x7F));
            break;
        }

        case 0x2:   // registered controller, RPN
        case 0x3:   // assignable controller, NRPN
        {
            auto const value = ScaleDown(word1, 32, 14);
            auto const registered = status == 0x2;

            AddOneWord(result, ControlChange(group, channel, registered ? 101 : 99, index1 & 0x7F));
            AddOneWord(result, ControlChange(group, channel, registered ? 100 : 98, index2 & 0x7F));
            AddOneWord(result, ControlChange(group, channel, 6, (value >> 7) & 0x7F));
            AddOneWord(result, ControlChange(group, channel, 38, value & 0x7F));
            break;
        }

        default:
            // Per-note controllers, per-note pitch bend, per-note management and relative
            // controllers have no MIDI 1.0 equivalent (UMP spec, appendix D.2.8).
            result.Dropped = true;
            break;
        }

        return result;
    }

    _Use_decl_annotations_
    TranslatedMessages TranslateToMidi2(uint32_t const* words, uint8_t wordCount) noexcept
    {
        TranslatedMessages result{};

        if (wordCount == 0)
        {
            return result;
        }

        auto const word0 = words[0];

        if ((word0 >> 28) != 0x2)
        {
            PassThrough(result, words, wordCount);
            return result;
        }

        auto const group = (word0 >> 24) & 0x0F;
        auto status = (word0 >> 20) & 0x0F;
        auto const channel = (word0 >> 16) & 0x0F;
        auto const data1 = (word0 >> 8) & 0x7F;
        auto const data2 = word0 & 0x7F;

        auto const head = [group, channel](uint32_t messageStatus, uint32_t index1, uint32_t index2)
        {
            return 0x40000000u | (group << 24) | (messageStatus << 20) | (channel << 16) | (index1 << 8) | index2;
        };

        uint32_t word1{ 0 };
        uint32_t first{ 0 };

        switch (status)
        {
        case 0x9:
            if (data2 == 0)
            {
                status = 0x8;
            }

            first = head(status, data1, 0);
            word1 = status == 0x9 ? ScaleUp(data2, 7, 16) << 16 : 0;
            break;

        case 0x8:
            first = head(0x8, data1, 0);
            word1 = ScaleUp(data2, 7, 16) << 16;
            break;

        case 0xA:
            first = head(0xA, data1, 0);
            word1 = ScaleUp(data2, 7, 32);
            break;

        case 0xB:
            first = head(0xB, data1, 0);
            word1 = ScaleUp(data2, 7, 32);
            break;

        case 0xC:
            first = head(0xC, 0, 0);
            word1 = data1 << 24;
            break;

        case 0xD:
            first = head(0xD, 0, 0);
            word1 = ScaleUp(data1, 7, 32);
            break;

        case 0xE:
            first = head(0xE, 0, 0);
            word1 = ScaleUp((data2 << 7) | data1, 14, 32);
            break;

        default:
            PassThrough(result, words, wordCount);
            return result;
        }

        result.Messages[0][0] = first;
        result.Messages[0][1] = word1;
        result.WordCounts[0] = 2;
        result.Count = 1;
        return result;
    }
}
