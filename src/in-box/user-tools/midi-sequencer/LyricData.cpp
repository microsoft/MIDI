// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Lyrics read back from MIDI 2.0 Lyric Data and Ruby Data messages (M2-104 sections 7.5.10 to
// 7.5.13), the way a MIDI Clip File carries them. Everything here comes from a file, so counts
// and lengths are capped.

#include "Lyrics.h"

#include <algorithm>
#include <map>

namespace midisequencer
{
    namespace
    {
        constexpr uint32_t FlexDataMessageType = 0xD;
        constexpr uint32_t LyricStatusBank = 0x02;
        constexpr uint8_t LyricsStatus = 0x01;
        constexpr uint8_t LyricsLanguageStatus = 0x02;
        constexpr uint8_t RubyStatus = 0x03;
        constexpr uint8_t RubyLanguageStatus = 0x04;
        constexpr size_t TextBytesPerPacket = 12;

        // No syllable or language tag needs more than this.
        constexpr size_t MaximumTextBytes = 4096;

        enum class PacketFormat : uint32_t
        {
            Complete = 0,
            Start = 1,
            Continue = 2,
            End = 3,
        };

        LyricEnd Stronger(_In_ LyricEnd a, _In_ LyricEnd b) noexcept
        {
            return static_cast<uint8_t>(a) >= static_cast<uint8_t>(b) ? a : b;
        }

        bool IsLyricMessage(_In_ ClipEvent const& event) noexcept
        {
            auto const word0 = event.Words[0];
            auto const status = word0 & 0xFF;

            return event.WordCount == 4 && (word0 >> 28) == FlexDataMessageType && ((word0 >> 8) & 0xFF) == LyricStatusBank &&
                status >= LyricsStatus && status <= RubyLanguageStatus;
        }

        PacketFormat FormOf(_In_ ClipEvent const& event) noexcept
        {
            return static_cast<PacketFormat>((event.Words[0] >> 22) & 0x03);
        }

        // Who a message is for: one channel, or the whole group. The group itself doesn't
        // matter, because the destination sets it.
        uint32_t AddressOf(_In_ ClipEvent const& event) noexcept
        {
            auto const word0 = event.Words[0];
            return ((word0 >> 20) & 0x03) == 1 ? 0x10 : (word0 >> 16) & 0x0F;
        }

        // One message's text, from a Complete message or from Start, Continue and End.
        struct MessageText
        {
            int64_t Tick{ 0 };
            uint8_t Status{ 0 };
            std::string Bytes{};

            // A Complete message with no text, which keeps the syllable before sounding.
            bool Melisma{ false };
        };

        std::vector<MessageText> CollectTexts(_In_ std::vector<ClipEvent> const& events, _In_ uint32_t address, _In_ size_t maximum)
        {
            std::vector<MessageText> texts{};
            std::map<uint8_t, MessageText> open{};

            for (auto const& event : events)
            {
                if (texts.size() >= maximum)
                {
                    break;
                }

                if (!IsLyricMessage(event) || AddressOf(event) != address)
                {
                    continue;
                }

                auto const status = static_cast<uint8_t>(event.Words[0] & 0xFF);
                auto const form = FormOf(event);

                if (form == PacketFormat::Complete || form == PacketFormat::Start)
                {
                    open[status] = MessageText{ event.Tick, status, std::string{}, false };
                }

                auto const found = open.find(status);

                // A Continue or an End with no Start before it.
                if (found == open.end())
                {
                    continue;
                }

                auto& text = found->second;
                auto const before = text.Bytes.size();

                // A 0 byte ends the text, and the rest of the message is padding.
                for (size_t i = 0; i < TextBytesPerPacket; ++i)
                {
                    auto const value = static_cast<char>((event.Words[1 + i / 4] >> (24 - 8 * (i % 4))) & 0xFF);

                    if (value == 0)
                    {
                        break;
                    }

                    if (text.Bytes.size() < MaximumTextBytes)
                    {
                        text.Bytes.push_back(value);
                    }
                }

                if (form == PacketFormat::Complete || form == PacketFormat::End)
                {
                    text.Melisma = form == PacketFormat::Complete && text.Bytes.size() == before;
                    texts.push_back(std::move(text));
                    open.erase(found);
                }
            }

            return texts;
        }

        // The breaks at the start of a text, which belong to the syllable before it.
        LyricEnd TakeLeadingBreaks(_Inout_ std::wstring& text)
        {
            LyricEnd end{ LyricEnd::None };
            size_t lead{ 0 };

            for (; lead < text.size(); ++lead)
            {
                auto const c = text[lead];

                if (c == L'\n')
                {
                    end = LyricEnd::Paragraph;
                }
                else if (c == L'\r')
                {
                    end = Stronger(end, LyricEnd::Line);
                }
                else if (c == L' ' || c == L'\t')
                {
                    end = Stronger(end, LyricEnd::Word);
                }
                else
                {
                    break;
                }
            }

            text.erase(0, lead);
            return end;
        }

        // What follows the text: a space ends a word, a carriage return a line, and a carriage
        // return and a line feed a verse.
        LyricEnd TakeEnd(_Inout_ std::wstring& text)
        {
            LyricEnd end{ LyricEnd::None };

            while (!text.empty())
            {
                auto const c = text.back();

                if (c == L'\n')
                {
                    end = LyricEnd::Paragraph;
                }
                else if (c == L'\r')
                {
                    end = Stronger(end, LyricEnd::Line);
                }
                else if (c == L' ' || c == L'\t')
                {
                    end = Stronger(end, LyricEnd::Word);
                }
                else
                {
                    break;
                }

                text.pop_back();
            }

            return end;
        }

        void AddSyllable(_Inout_ std::vector<Lyric>& syllables, _In_ MessageText const& message, _In_ size_t maximum)
        {
            if (message.Melisma)
            {
                if (syllables.size() < maximum)
                {
                    syllables.push_back(Lyric{ std::max<int64_t>(0, message.Tick), std::wstring{}, LyricEnd::None });
                }

                return;
            }

            auto text = FromUtf8(message.Bytes);
            auto const before = TakeLeadingBreaks(text);

            if (before != LyricEnd::None && !syllables.empty())
            {
                syllables.back().End = Stronger(syllables.back().End, before);
            }

            auto const end = TakeEnd(text);

            // Only a break: the end of the word a melisma finished, or of a line.
            if (text.empty())
            {
                if (!syllables.empty())
                {
                    syllables.back().End = Stronger(syllables.back().End, end);
                }

                return;
            }

            if (syllables.size() < maximum)
            {
                syllables.push_back(Lyric{ std::max<int64_t>(0, message.Tick), std::move(text), end });
            }
        }
    }

    _Use_decl_annotations_
    std::optional<LyricsTrack> TakeLyricsFromClip(Clip& clip, SequenceLimits const& limits)
    {
        // The channel or group that sings the most syllables.
        std::map<uint32_t, size_t> counts{};

        for (auto const& event : clip.Events)
        {
            auto const form = FormOf(event);

            if (IsLyricMessage(event) && (event.Words[0] & 0xFF) == LyricsStatus && (form == PacketFormat::Complete || form == PacketFormat::Start))
            {
                ++counts[AddressOf(event)];
            }
        }

        if (counts.empty())
        {
            return std::nullopt;
        }

        auto const address = std::max_element(counts.begin(), counts.end(),
            [](auto const& a, auto const& b) { return a.second < b.second; })->first;

        LyricsTrack lyrics{};
        std::vector<std::pair<int64_t, std::wstring>> rubies{};

        for (auto const& message : CollectTexts(clip.Events, address, limits.MaximumLyrics * 4))
        {
            switch (message.Status)
            {
            case LyricsStatus:
                AddSyllable(lyrics.Syllables, message, limits.MaximumLyrics);
                break;

            case LyricsLanguageStatus:
                if (lyrics.Language.empty())
                {
                    lyrics.Language = FromUtf8(message.Bytes);
                }
                break;

            case RubyStatus:
            {
                auto ruby = FromUtf8(message.Bytes);
                TakeLeadingBreaks(ruby);
                TakeEnd(ruby);

                if (!message.Melisma && !ruby.empty() && rubies.size() < limits.MaximumLyrics)
                {
                    rubies.emplace_back(message.Tick, std::move(ruby));
                }

                break;
            }

            case RubyLanguageStatus:
                if (lyrics.RubyLanguage.empty())
                {
                    lyrics.RubyLanguage = FromUtf8(message.Bytes);
                }
                break;

            default:
                break;
            }
        }

        if (lyrics.Syllables.empty())
        {
            return std::nullopt;
        }

        auto& syllables = lyrics.Syllables;
        std::stable_sort(syllables.begin(), syllables.end(), [](Lyric const& a, Lyric const& b) { return a.Tick < b.Tick; });

        // Ruby reads the syllable being sung when it arrives: the one at its tick, or before it.
        for (auto& [tick, ruby] : rubies)
        {
            auto const after = std::upper_bound(syllables.begin(), syllables.end(), tick,
                [](int64_t value, Lyric const& syllable) { return value < syllable.Tick; });

            if (after != syllables.begin())
            {
                auto& read = *(after - 1);

                if (read.Ruby.size() < limits.MaximumTextLength)
                {
                    read.Ruby += ruby;
                }
            }
        }

        std::erase_if(clip.Events, [address](ClipEvent const& event) { return IsLyricMessage(event) && AddressOf(event) == address; });

        NormalizeLyrics(lyrics, limits);
        return lyrics;
    }
}
