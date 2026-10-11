// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "Lyrics.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

// windows.h renames GetObject, which Windows.Data.Json uses.
#undef GetObject

#include <algorithm>
#include <cctype>
#include <cmath>
#include <format>
#include <functional>
#include <unordered_map>

namespace midisequencer
{
    namespace
    {
        namespace json = winrt::Windows::Data::Json;

        constexpr wchar_t KeyLanguage[] = L"language";
        constexpr wchar_t KeyTrack[] = L"track";
        constexpr wchar_t KeySyllables[] = L"syllables";
        constexpr wchar_t KeyRubyLanguage[] = L"rubyLanguage";
        constexpr wchar_t KeyRuby[] = L"ruby";

        constexpr wchar_t EndWord[] = L"word";
        constexpr wchar_t EndLine[] = L"line";
        constexpr wchar_t EndVerse[] = L"verse";

        // RFC 5646 asks software to allow tags of at least 35 characters.
        constexpr size_t MaximumLanguageLength = 35;
        constexpr size_t MaximumTrackIdLength = 128;

        // A line with no end of its own is broken at the next word's end past this many
        // characters. RP-017 asks for lines under 40.
        constexpr size_t LongLineCharacters = 40;

        // Lyric Data and Lyrics Language: Flex Data (M2-104 section 7.5.10).
        constexpr uint32_t FlexDataMessageType = 0xD;
        constexpr uint32_t LyricStatusBank = 0x02;
        constexpr uint32_t LyricsStatus = 0x01;
        constexpr uint32_t LyricsLanguageStatus = 0x02;
        constexpr uint32_t RubyStatus = 0x03;
        constexpr uint32_t RubyLanguageStatus = 0x04;
        constexpr uint32_t AddressGroup = 0x1;
        constexpr size_t TextBytesPerPacket = 12;

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

        bool IsControl(_In_ wchar_t c) noexcept
        {
            return c < 0x20 || (c >= 0x7F && c < 0xA0);
        }

        // Without control characters, and no longer than maximum, without cutting a surrogate
        // pair in half.
        std::wstring CleanText(_In_ std::wstring_view text, _In_ size_t maximum)
        {
            std::wstring clean{};
            clean.reserve(std::min(text.size(), maximum));

            for (auto const c : text)
            {
                if (clean.size() >= maximum)
                {
                    break;
                }

                if (!IsControl(c))
                {
                    clean.push_back(c);
                }
            }

            if (!clean.empty() && clean.back() >= 0xD800 && clean.back() <= 0xDBFF)
            {
                clean.pop_back();
            }

            return clean;
        }

        // Letters, digits and hyphens, which is all a BCP 47 tag is made of.
        std::wstring CleanLanguage(_In_ std::wstring_view tag)
        {
            std::wstring clean{};

            for (auto const c : tag)
            {
                if (clean.size() >= MaximumLanguageLength)
                {
                    break;
                }

                if ((c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z') || (c >= L'0' && c <= L'9') || c == L'-')
                {
                    clean.push_back(c);
                }
            }

            return clean;
        }

        bool IsUtf8(_In_ std::string_view bytes) noexcept
        {
            size_t i{ 0 };

            while (i < bytes.size())
            {
                auto const lead = static_cast<uint8_t>(bytes[i]);
                size_t extra{ 0 };

                if (lead < 0x80)
                {
                    extra = 0;
                }
                else if (lead >= 0xC2 && lead <= 0xDF)
                {
                    extra = 1;
                }
                else if (lead >= 0xE0 && lead <= 0xEF)
                {
                    extra = 2;
                }
                else if (lead >= 0xF0 && lead <= 0xF4)
                {
                    extra = 3;
                }
                else
                {
                    return false;
                }

                if (bytes.size() - i - 1 < extra)
                {
                    return false;
                }

                for (size_t k = 1; k <= extra; ++k)
                {
                    if ((static_cast<uint8_t>(bytes[i + k]) & 0xC0) != 0x80)
                    {
                        return false;
                    }
                }

                i += extra + 1;
            }

            return true;
        }

        std::wstring FromCodePage(_In_ std::string_view bytes, _In_ UINT codePage)
        {
            if (bytes.empty())
            {
                return {};
            }

            auto const size = static_cast<int>(std::min<size_t>(bytes.size(), 0x10000));
            auto const count = MultiByteToWideChar(codePage, 0, bytes.data(), size, nullptr, 0);

            if (count <= 0)
            {
                return {};
            }

            std::wstring text(static_cast<size_t>(count), L'\0');
            auto const written = MultiByteToWideChar(codePage, 0, bytes.data(), size, text.data(), count);
            text.resize(static_cast<size_t>(std::max(0, written)));
            return text;
        }

        // The character sets RP-026 names: ANSI unless a file says otherwise.
        constexpr UINT AnsiCodePage = 1252;
        constexpr UINT ShiftJisCodePage = 932;

        bool ContainsTag(_In_ std::string_view bytes, _In_ std::string_view tag) noexcept
        {
            if (bytes.size() < tag.size())
            {
                return false;
            }

            for (size_t i = 0; i + tag.size() <= bytes.size(); ++i)
            {
                size_t k{ 0 };

                while (k < tag.size() && std::tolower(static_cast<uint8_t>(bytes[i + k])) == tag[k])
                {
                    ++k;
                }

                if (k == tag.size())
                {
                    return true;
                }
            }

            return false;
        }

        // A lyric event's text. RP-026 marks UTF-16 with a byte order mark, and names a character
        // set with a tag that lasts until the next one: {@LATIN} for ANSI, {@JP} for Shift-JIS.
        // Text that is valid UTF-8 is read as UTF-8, which is what files written today use.
        std::wstring DecodeFileText(_In_ std::string_view bytes, _Inout_ UINT& codePage)
        {
            if (ContainsTag(bytes, "{@jp}"))
            {
                codePage = ShiftJisCodePage;
            }
            else if (ContainsTag(bytes, "{@latin}"))
            {
                codePage = AnsiCodePage;
            }

            if (bytes.size() >= 2 && (static_cast<uint8_t>(bytes[0]) == 0xFF || static_cast<uint8_t>(bytes[0]) == 0xFE) &&
                static_cast<uint8_t>(bytes[0]) + static_cast<uint8_t>(bytes[1]) == 0xFF + 0xFE)
            {
                auto const littleEndian = static_cast<uint8_t>(bytes[0]) == 0xFF;
                std::wstring text{};

                for (size_t i = 2; i + 1 < bytes.size(); i += 2)
                {
                    auto const first = static_cast<uint8_t>(bytes[i]);
                    auto const second = static_cast<uint8_t>(bytes[i + 1]);
                    text.push_back(static_cast<wchar_t>(littleEndian ? (first | (second << 8)) : ((first << 8) | second)));
                }

                return text;
            }

            constexpr std::string_view utf8Mark{ "\xEF\xBB\xBF" };

            if (bytes.starts_with(utf8Mark))
            {
                return FromUtf8(bytes.substr(utf8Mark.size()));
            }

            if (codePage != ShiftJisCodePage && IsUtf8(bytes))
            {
                return FromUtf8(bytes);
            }

            return FromCodePage(bytes, codePage);
        }

        // RP-026 lyric text split into what's sung and its ruby: escapes become the characters
        // they stand for, tags in braces are taken out, and what's in brackets is the ruby.
        struct SplitText
        {
            std::wstring Plain{};
            std::wstring Ruby{};
        };

        SplitText SplitLyricText(_In_ std::wstring_view text)
        {
            SplitText split{};
            split.Plain.reserve(text.size());

            size_t braces{ 0 };
            size_t brackets{ 0 };

            auto const keep = [&](wchar_t c)
            {
                if (braces == 0)
                {
                    (brackets == 0 ? split.Plain : split.Ruby).push_back(c);
                }
            };

            for (size_t i = 0; i < text.size(); ++i)
            {
                auto const c = text[i];

                if (c == L'\\' && i + 1 < text.size())
                {
                    wchar_t literal{ 0 };

                    switch (text[i + 1])
                    {
                    case L'r': literal = L'\r'; break;
                    case L'n': literal = L'\n'; break;
                    case L't': literal = L' '; break;
                    case L'\\':
                    case L'[':
                    case L']':
                    case L'{':
                    case L'}': literal = text[i + 1]; break;
                    default: break;
                    }

                    if (literal != 0)
                    {
                        keep(literal);
                        ++i;
                        continue;
                    }
                }

                if (c == L'{')
                {
                    ++braces;
                }
                else if (c == L'}' && braces > 0)
                {
                    --braces;
                }
                else if (c == L'[' && braces == 0)
                {
                    ++brackets;
                }
                else if (c == L']' && braces == 0 && brackets > 0)
                {
                    --brackets;
                }
                else
                {
                    keep(c);
                }
            }

            return split;
        }

        std::wstring EscapeLyricText(_In_ std::wstring_view text)
        {
            std::wstring escaped{};
            escaped.reserve(text.size());

            for (auto const c : text)
            {
                if (c == L'\\' || c == L'[' || c == L']' || c == L'{' || c == L'}')
                {
                    escaped.push_back(L'\\');
                }

                escaped.push_back(c);
            }

            return escaped;
        }

        // What follows a syllable in a Standard MIDI File (RP-017) and in Lyric Data (M2-104).
        std::wstring_view FileEnding(_In_ LyricEnd end) noexcept
        {
            switch (end)
            {
            case LyricEnd::Word: return L" ";
            case LyricEnd::Line: return L"\r";
            case LyricEnd::Paragraph: return L"\r\n";
            default: return {};
            }
        }

        std::string_view PacketEnding(_In_ LyricEnd end) noexcept
        {
            switch (end)
            {
            case LyricEnd::Word: return " ";
            case LyricEnd::Line: return "\r";
            case LyricEnd::Paragraph: return "\r\n";
            default: return {};
            }
        }

        std::wstring_view EndName(_In_ LyricEnd end) noexcept
        {
            switch (end)
            {
            case LyricEnd::Word: return EndWord;
            case LyricEnd::Line: return EndLine;
            case LyricEnd::Paragraph: return EndVerse;
            default: return {};
            }
        }

        LyricEnd EndFromName(_In_ std::wstring_view name) noexcept
        {
            if (name == EndWord)
            {
                return LyricEnd::Word;
            }

            if (name == EndLine)
            {
                return LyricEnd::Line;
            }

            if (name == EndVerse)
            {
                return LyricEnd::Paragraph;
            }

            return LyricEnd::None;
        }

        std::array<uint32_t, 4> FlexTextPacket(
            _In_ uint8_t group,
            _In_ PacketFormat format,
            _In_ uint32_t status,
            _In_ std::string_view bytes) noexcept
        {
            std::array<uint32_t, 4> words{};
            words[0] = (FlexDataMessageType << 28) | ((static_cast<uint32_t>(group) & 0x0F) << 24) |
                (static_cast<uint32_t>(format) << 22) | (AddressGroup << 20) | (LyricStatusBank << 8) | status;

            for (size_t i = 0; i < bytes.size() && i < TextBytesPerPacket; ++i)
            {
                words[1 + i / 4] |= static_cast<uint32_t>(static_cast<uint8_t>(bytes[i])) << (24 - 8 * (i % 4));
            }

            return words;
        }

        // Text over as many messages as it takes, twelve bytes in each. Only the last may be
        // shorter: 0 marks the end of the text, so a Start or Continue message must be full
        // (M2-104 section 7.5.9), even when that splits a character between two messages.
        void AppendTextPackets(
            _Inout_ std::vector<std::array<uint32_t, 4>>& packets,
            _In_ uint8_t group,
            _In_ uint32_t status,
            _In_ std::string_view bytes)
        {
            auto const count = (bytes.size() + TextBytesPerPacket - 1) / TextBytesPerPacket;

            for (size_t i = 0; i < count; ++i)
            {
                auto const format = count == 1 ? PacketFormat::Complete :
                    i == 0 ? PacketFormat::Start :
                    i + 1 == count ? PacketFormat::End : PacketFormat::Continue;

                packets.push_back(FlexTextPacket(group, format, status, bytes.substr(i * TextBytesPerPacket, TextBytesPerPacket)));
            }
        }

        std::wstring Quote(_In_ std::wstring_view text)
        {
            return std::wstring{ json::JsonValue::CreateStringValue(text).Stringify() };
        }

        std::wstring StringMember(_In_ json::JsonObject const& object, _In_ std::wstring_view key)
        {
            if (!object.HasKey(key))
            {
                return {};
            }

            auto const value = object.Lookup(key);

            if (value == nullptr || value.ValueType() != json::JsonValueType::String)
            {
                return {};
            }

            return std::wstring{ value.GetString() };
        }

        // Where each placement's notes start, for a track in a sequence or in a file being
        // imported. find looks a clip up by id.
        template<typename TFind>
        std::vector<int64_t> NoteStarts(
            _In_ Track const& track,
            _In_ TFind const& find,
            _In_ int64_t fromTick,
            _In_ size_t limit)
        {
            std::vector<int64_t> starts{};

            if (limit == 0)
            {
                return starts;
            }

            for (auto const& placement : track.Timeline)
            {
                Clip const* clip = find(placement.ClipId);

                if (clip == nullptr || clip->Length <= 0 || clip->Notes.empty())
                {
                    continue;
                }

                auto const length = placement.Length > 0 ? placement.Length : clip->Length;
                auto const end = placement.Tick + (clip->Loop ? length : std::min(length, clip->Length));
                size_t found{ 0 };

                // Only the passes from fromTick on, stopping once this placement alone has given
                // enough, or a whole pass gave nothing, since every pass after it would too.
                auto const firstPass = clip->Loop && fromTick > placement.Tick ? (fromTick - placement.Tick) / clip->Length : 0;

                for (auto passStart = placement.Tick + firstPass * clip->Length; passStart < end && found < limit; passStart += clip->Length)
                {
                    auto const before = found;

                    for (auto const& note : clip->Notes)
                    {
                        auto const tick = passStart + note.Tick;

                        if (note.Tick >= clip->Length || tick >= end)
                        {
                            break;
                        }

                        if (tick >= fromTick)
                        {
                            starts.push_back(tick);
                            ++found;
                        }
                    }

                    if (!clip->Loop || (found == before && passStart >= fromTick))
                    {
                        break;
                    }
                }

                // Only the earliest starts count, so keep no more than those plus some room.
                if (starts.size() >= 2 * limit + 65536)
                {
                    std::sort(starts.begin(), starts.end());
                    starts.erase(std::unique(starts.begin(), starts.end()), starts.end());
                    starts.resize(std::min(starts.size(), limit));
                }
            }

            std::sort(starts.begin(), starts.end());
            starts.erase(std::unique(starts.begin(), starts.end()), starts.end());

            if (starts.size() > limit)
            {
                starts.resize(limit);
            }

            return starts;
        }

        // The syllables of a line joined into the words they make.
        void AppendSyllable(_Inout_ std::wstring& text, _In_ Lyric const& syllable, _In_ bool lastInLine)
        {
            text += syllable.Text;

            if (syllable.End != LyricEnd::None && !lastInLine)
            {
                text += L' ';
            }
        }

        std::wstring LineString(_In_ std::vector<Lyric> const& syllables, _In_ LyricLine const& line)
        {
            std::wstring text{};

            for (size_t i = 0; i < line.Count; ++i)
            {
                AppendSyllable(text, syllables[line.First + i], i + 1 == line.Count);
            }

            return text;
        }

        std::string LrcTime(_In_ double seconds)
        {
            auto const hundredths = std::max<long long>(0, std::llround(seconds * 100.0));
            return std::format("[{:02}:{:02}.{:02}]", hundredths / 6000, (hundredths % 6000) / 100, hundredths % 100);
        }
    }

    // ---- typing ----

    _Use_decl_annotations_
    std::wstring LyricsToText(std::vector<Lyric> const& syllables)
    {
        std::wstring text{};

        // Hyphens, underscores and brackets that belong to the words are typed twice, so they
        // aren't taken for a break between syllables, a melisma or ruby.
        auto const appendDoubled = [&text](std::wstring const& part, std::wstring_view doubled)
        {
            for (auto const c : part)
            {
                if (doubled.find(c) != std::wstring_view::npos)
                {
                    text += c;
                }

                text += c;
            }
        };

        for (auto const& syllable : syllables)
        {
            if (syllable.Text.empty())
            {
                text += L'_';
            }
            else
            {
                appendDoubled(syllable.Text, L"-_[]");
            }

            if (!syllable.Ruby.empty())
            {
                text += L'[';
                appendDoubled(syllable.Ruby, L"[]");
                text += L']';
            }

            switch (syllable.End)
            {
            case LyricEnd::None: text += L'-'; break;
            case LyricEnd::Word: text += L' '; break;
            case LyricEnd::Line: text += L'\n'; break;
            case LyricEnd::Paragraph: text += L"\n\n"; break;
            }
        }

        // No break after the very last syllable.
        while (!text.empty() && (text.back() == L' ' || text.back() == L'\n' || text.back() == L'-'))
        {
            if (text.back() == L'-' && text.size() >= 2 && text[text.size() - 2] == L'-')
            {
                break;
            }

            text.pop_back();
        }

        return text;
    }

    _Use_decl_annotations_
    std::vector<Lyric> ParseLyricsText(std::wstring_view text, size_t maximumSyllables)
    {
        std::vector<Lyric> syllables{};
        auto const maximumText = SequenceLimits{}.MaximumTextLength;

        // The syllable being typed, if there is one, and the ruby typed for it.
        std::wstring current{};
        std::wstring ruby{};
        bool haveSyllable{ false };
        bool melisma{ false };
        bool inRuby{ false };

        // Ends the syllable being typed, if there is one, and says what follows it.
        auto const finish = [&](LyricEnd end)
        {
            if (haveSyllable && syllables.size() < maximumSyllables)
            {
                Lyric syllable{};
                syllable.Text = melisma ? std::wstring{} : CleanText(current, maximumText);
                syllable.End = end;
                syllable.Ruby = CleanText(ruby, maximumText);
                syllables.push_back(std::move(syllable));
            }
            else if (!haveSyllable && end != LyricEnd::None && !syllables.empty())
            {
                syllables.back().End = Stronger(syllables.back().End, end);
            }

            current.clear();
            ruby.clear();
            haveSyllable = false;
            melisma = false;
        };

        // Ruby typed with nothing in front of it reads the syllable before, as RP-026 has it.
        auto const closeRuby = [&]()
        {
            inRuby = false;

            if (!haveSyllable && !ruby.empty())
            {
                if (!syllables.empty())
                {
                    syllables.back().Ruby = CleanText(syllables.back().Ruby + ruby, maximumText);
                }

                ruby.clear();
            }
        };

        size_t lineBreaks{ 0 };

        for (size_t i = 0; i < text.size(); ++i)
        {
            auto c = text[i];
            auto const next = i + 1 < text.size() ? text[i + 1] : L'\0';

            // A TextBox ends its lines with a carriage return alone; other text uses both, or a
            // line feed alone.
            if (c == L'\r')
            {
                if (next == L'\n')
                {
                    continue;
                }

                c = L'\n';
            }

            // Ruby runs to the closing bracket. Spaces and hyphens in it are part of it.
            if (inRuby && c != L'\n')
            {
                if ((c == L'[' || c == L']') && next == c)
                {
                    ruby += c;
                    ++i;
                }
                else if (c == L']')
                {
                    closeRuby();
                }
                else
                {
                    ruby += c == L'\t' ? L' ' : c;
                }

                continue;
            }

            if (inRuby)
            {
                closeRuby();
            }

            if (c == L'\n')
            {
                finish(LyricEnd::Line);
                ++lineBreaks;

                // A blank line ends a verse.
                if (lineBreaks >= 2 && !syllables.empty())
                {
                    syllables.back().End = LyricEnd::Paragraph;
                }

                continue;
            }

            if (c == L' ' || c == L'\t' || c == 0x3000)
            {
                finish(LyricEnd::Word);
                continue;
            }

            lineBreaks = 0;

            if ((c == L'-' || c == L'_' || c == L'[' || c == L']') && next == c)
            {
                current += c;
                haveSyllable = true;
                melisma = false;
                ++i;
                continue;
            }

            if (c == L'[')
            {
                inRuby = true;
                continue;
            }

            if (c == L'-')
            {
                finish(LyricEnd::None);
                continue;
            }

            if (c == L'_' && current.empty())
            {
                haveSyllable = true;
                melisma = true;
                continue;
            }

            if (melisma)
            {
                // Text straight after a melisma's _ starts the next syllable of the word.
                finish(LyricEnd::None);
            }

            current += c;
            haveSyllable = true;
        }

        if (inRuby)
        {
            closeRuby();
        }

        finish(LyricEnd::None);

        return syllables;
    }

    _Use_decl_annotations_
    std::vector<int64_t> TrackNoteStarts(Sequence const& sequence, Track const& track, int64_t fromTick, size_t limit)
    {
        return NoteStarts(track, [&sequence](std::wstring_view id) { return FindClip(sequence, id); }, fromTick, limit);
    }

    _Use_decl_annotations_
    void PlaceLyrics(std::vector<Lyric>& syllables, Sequence const& sequence, std::wstring_view trackId, int64_t fromTick)
    {
        fromTick = std::max<int64_t>(0, fromTick);

        std::vector<int64_t> starts{};

        if (auto const track = FindTrack(sequence, trackId); track != nullptr && !track->IsFolder)
        {
            starts = TrackNoteStarts(sequence, *track, fromTick, syllables.size());
        }

        size_t next{ 0 };
        auto tick = fromTick;

        for (size_t i = 0; i < syllables.size(); ++i)
        {
            if (next < starts.size())
            {
                tick = starts[next++];
            }
            else if (i > 0)
            {
                tick += TicksPerQuarterNote;
            }

            syllables[i].Tick = tick;
        }
    }

    _Use_decl_annotations_
    std::wstring TrackThatSings(std::vector<Track> const& tracks, std::vector<Clip> const& clips, std::vector<Lyric> const& syllables)
    {
        if (syllables.empty())
        {
            return {};
        }

        std::unordered_map<std::wstring_view, Clip const*> byId{};

        for (auto const& clip : clips)
        {
            byId.emplace(clip.Id, &clip);
        }

        auto const find = [&byId](std::wstring_view id) -> Clip const*
        {
            auto const found = byId.find(id);
            return found == byId.end() ? nullptr : found->second;
        };

        std::vector<int64_t> sung{};
        sung.reserve(syllables.size());

        for (auto const& syllable : syllables)
        {
            sung.push_back(syllable.Tick);
        }

        std::sort(sung.begin(), sung.end());
        sung.erase(std::unique(sung.begin(), sung.end()), sung.end());

        std::wstring best{};
        size_t bestMatches{ 0 };

        std::function<void(std::vector<Track> const&)> visit = [&](std::vector<Track> const& level)
        {
            for (auto const& track : level)
            {
                if (track.IsFolder)
                {
                    visit(track.Children);
                    continue;
                }

                // Enough starts to cover the lyrics a few times over is plenty to judge by.
                auto const starts = NoteStarts(track, find, 0, sung.size() * 4 + 64);
                size_t matches{ 0 };

                for (auto const tick : sung)
                {
                    if (std::binary_search(starts.begin(), starts.end(), tick))
                    {
                        ++matches;
                    }
                }

                if (matches > bestMatches)
                {
                    bestMatches = matches;
                    best = track.Id;
                }
            }
        };

        visit(tracks);

        return best;
    }

    _Use_decl_annotations_
    void NormalizeLyrics(LyricsTrack& lyrics, SequenceLimits const& limits)
    {
        lyrics.Language = CleanLanguage(lyrics.Language);
        lyrics.RubyLanguage = CleanLanguage(lyrics.RubyLanguage);

        if (lyrics.TrackId.size() > MaximumTrackIdLength)
        {
            lyrics.TrackId.clear();
        }

        if (lyrics.Syllables.size() > limits.MaximumLyrics)
        {
            lyrics.Syllables.resize(limits.MaximumLyrics);
        }

        for (auto& syllable : lyrics.Syllables)
        {
            syllable.Tick = std::clamp<int64_t>(syllable.Tick, 0, limits.MaximumTick);
            syllable.Text = CleanText(syllable.Text, limits.MaximumTextLength);
            syllable.Ruby = CleanText(syllable.Ruby, limits.MaximumTextLength);

            if (static_cast<uint8_t>(syllable.End) > static_cast<uint8_t>(LyricEnd::Paragraph))
            {
                syllable.End = LyricEnd::Word;
            }
        }

        std::stable_sort(lyrics.Syllables.begin(), lyrics.Syllables.end(),
            [](Lyric const& a, Lyric const& b) { return a.Tick < b.Tick; });
    }

    _Use_decl_annotations_
    std::unique_ptr<SequenceChange> MakeLyricsChange(std::optional<LyricsTrack> before, std::optional<LyricsTrack> after)
    {
        auto const cost = [](std::optional<LyricsTrack> const& lyrics)
        {
            size_t bytes{ sizeof(LyricsTrack) };

            if (lyrics)
            {
                for (auto const& syllable : lyrics->Syllables)
                {
                    bytes += sizeof(Lyric) + (syllable.Text.size() + syllable.Ruby.size()) * sizeof(wchar_t);
                }
            }

            return bytes;
        };

        auto const total = cost(before) + cost(after);

        return std::make_unique<SnapshotChange<std::optional<LyricsTrack>>>(
            [](Sequence& sequence) { return &sequence.Lyrics; },
            std::move(before),
            std::move(after),
            total);
    }

    // ---- showing ----

    _Use_decl_annotations_
    std::vector<LyricLine> LyricLines(std::vector<Lyric> const& syllables)
    {
        std::vector<LyricLine> lines{};
        LyricLine line{};
        size_t characters{ 0 };

        for (size_t i = 0; i < syllables.size(); ++i)
        {
            auto const& syllable = syllables[i];

            if (line.Count == 0)
            {
                line.First = i;
                line.StartTick = syllable.Tick;
                characters = 0;
            }

            ++line.Count;
            characters += syllable.Text.size() + 1;

            auto const ends = syllable.End == LyricEnd::Line || syllable.End == LyricEnd::Paragraph ||
                (syllable.End == LyricEnd::Word && characters >= LongLineCharacters) ||
                i + 1 == syllables.size();

            if (ends)
            {
                line.EndsVerse = syllable.End == LyricEnd::Paragraph;
                line.EndTick = i + 1 < syllables.size() ? syllables[i + 1].Tick : syllable.Tick + 4 * TicksPerQuarterNote;
                line.EndTick = std::max(line.EndTick, line.StartTick + 1);
                lines.push_back(line);
                line = LyricLine{};
            }
        }

        return lines;
    }

    _Use_decl_annotations_
    std::pair<std::wstring, std::wstring> LyricLineText(std::vector<Lyric> const& syllables, LyricLine const& line, int64_t tick)
    {
        std::pair<std::wstring, std::wstring> text{};

        for (size_t i = 0; i < line.Count && line.First + i < syllables.size(); ++i)
        {
            auto const& syllable = syllables[line.First + i];
            AppendSyllable(syllable.Tick <= tick ? text.first : text.second, syllable, i + 1 == line.Count);
        }

        return text;
    }

    _Use_decl_annotations_
    size_t LyricLineAtTick(std::vector<LyricLine> const& lines, int64_t tick) noexcept
    {
        auto const after = std::upper_bound(lines.begin(), lines.end(), tick,
            [](int64_t value, LyricLine const& line) { return value < line.StartTick; });

        return after == lines.begin() ? 0 : static_cast<size_t>(after - lines.begin()) - 1;
    }

    // ---- Standard MIDI Files ----

    _Use_decl_annotations_
    std::vector<Lyric> LyricsFromFileText(std::vector<LyricFileText> const& events, bool karaoke, SequenceLimits const& limits)
    {
        std::vector<Lyric> syllables{};
        UINT codePage{ AnsiCodePage };

        for (auto const& event : events)
        {
            if (syllables.size() >= limits.MaximumLyrics)
            {
                break;
            }

            auto text = DecodeFileText(event.Text, codePage);

            if (karaoke && text.starts_with(L'@'))
            {
                continue;
            }

            std::wstring ruby{};

            if (!karaoke)
            {
                auto split = SplitLyricText(text);
                text = std::move(split.Plain);
                ruby = CleanText(split.Ruby, limits.MaximumTextLength);
            }

            // A break at the start belongs to the syllable before.
            LyricEnd before{ LyricEnd::None };
            size_t lead{ 0 };

            for (; lead < text.size(); ++lead)
            {
                auto const c = text[lead];

                if (c == L'\n' || (karaoke && c == L'\\'))
                {
                    before = LyricEnd::Paragraph;
                }
                else if (c == L'\r' || (karaoke && c == L'/'))
                {
                    before = Stronger(before, LyricEnd::Line);
                }
                else if (c == L' ' || c == L'\t')
                {
                    before = Stronger(before, LyricEnd::Word);
                }
                else
                {
                    break;
                }
            }

            text.erase(0, lead);

            if (before != LyricEnd::None && !syllables.empty())
            {
                syllables.back().End = Stronger(syllables.back().End, before);
            }

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

            text = CleanText(text, limits.MaximumTextLength);

            if (text.empty())
            {
                // Ruby on its own reads the syllable before (RP-026).
                if (!ruby.empty())
                {
                    if (!syllables.empty())
                    {
                        auto& previous = syllables.back();
                        previous.Ruby = CleanText(previous.Ruby + ruby, limits.MaximumTextLength);
                        previous.End = Stronger(previous.End, end);
                    }

                    continue;
                }

                if (end != LyricEnd::None || before != LyricEnd::None)
                {
                    // A break on its own, like RP-017's [CR] event.
                    if (!syllables.empty())
                    {
                        syllables.back().End = Stronger(syllables.back().End, end);
                    }

                    continue;
                }

                // An empty lyric event is a melisma. One that held only RP-026 tags, or an empty
                // text event, is nothing.
                if (!karaoke && event.Text.empty())
                {
                    syllables.push_back(Lyric{ std::max<int64_t>(0, event.Tick), std::wstring{}, LyricEnd::None });
                }

                continue;
            }

            syllables.push_back(Lyric{ std::max<int64_t>(0, event.Tick), std::move(text), end, std::move(ruby) });
        }

        std::stable_sort(syllables.begin(), syllables.end(), [](Lyric const& a, Lyric const& b) { return a.Tick < b.Tick; });

        return syllables;
    }

    _Use_decl_annotations_
    std::vector<LyricFileText> LyricsToFileText(std::vector<Lyric> const& syllables)
    {
        std::vector<LyricFileText> events{};
        events.reserve(syllables.size());

        for (auto const& syllable : syllables)
        {
            auto const ending = FileEnding(syllable.End);

            // RP-026 ruby: in brackets after the text it reads.
            auto const ruby = syllable.Ruby.empty() ? std::wstring{} : L"[" + EscapeLyricText(syllable.Ruby) + L"]";

            if (syllable.Text.empty())
            {
                // A melisma is an empty event. Its ruby and its end follow in one of their own,
                // which reads the event before.
                events.push_back(LyricFileText{ syllable.Tick, std::string{} });

                if (!ruby.empty() || !ending.empty())
                {
                    events.push_back(LyricFileText{ syllable.Tick, ToUtf8(ruby + std::wstring{ ending }) });
                }

                continue;
            }

            events.push_back(LyricFileText{ syllable.Tick, ToUtf8(EscapeLyricText(syllable.Text) + ruby + std::wstring{ ending }) });
        }

        return events;
    }

    // ---- MIDI 2.0 ----

    _Use_decl_annotations_
    std::vector<std::array<uint32_t, 4>> LyricDataPackets(Lyric const& syllable, uint8_t group)
    {
        std::vector<std::array<uint32_t, 4>> packets{};
        auto const ending = PacketEnding(syllable.End);

        if (syllable.Text.empty())
        {
            // A melisma, then the end of the word it finishes, if it finishes one.
            packets.push_back(FlexTextPacket(group, PacketFormat::Complete, LyricsStatus, {}));
            AppendTextPackets(packets, group, LyricsStatus, ending);
            return packets;
        }

        auto bytes = ToUtf8(syllable.Text);
        bytes.erase(std::remove(bytes.begin(), bytes.end(), '\0'), bytes.end());
        bytes += ending;

        AppendTextPackets(packets, group, LyricsStatus, bytes);
        return packets;
    }

    _Use_decl_annotations_
    std::vector<std::array<uint32_t, 4>> LyricsLanguagePackets(std::wstring_view language, uint8_t group)
    {
        std::vector<std::array<uint32_t, 4>> packets{};
        AppendTextPackets(packets, group, LyricsLanguageStatus, ToUtf8(CleanLanguage(language)));
        return packets;
    }

    _Use_decl_annotations_
    std::vector<std::array<uint32_t, 4>> RubyDataPackets(Lyric const& syllable, uint8_t group)
    {
        std::vector<std::array<uint32_t, 4>> packets{};

        if (syllable.Ruby.empty())
        {
            return packets;
        }

        // Each ruby ends a word of its own, so a display that runs them together still keeps them
        // apart. It ends a line or a verse where its syllable does.
        auto bytes = ToUtf8(syllable.Ruby);
        bytes.erase(std::remove(bytes.begin(), bytes.end(), '\0'), bytes.end());
        bytes += PacketEnding(syllable.End == LyricEnd::None ? LyricEnd::Word : syllable.End);

        AppendTextPackets(packets, group, RubyStatus, bytes);
        return packets;
    }

    _Use_decl_annotations_
    std::vector<std::array<uint32_t, 4>> RubyLanguagePackets(std::wstring_view language, uint8_t group)
    {
        std::vector<std::array<uint32_t, 4>> packets{};
        AppendTextPackets(packets, group, RubyLanguageStatus, ToUtf8(CleanLanguage(language)));
        return packets;
    }

    _Use_decl_annotations_
    void RenderLyrics(
        Sequence const& sequence,
        Track const& track,
        int64_t fromTick,
        int64_t toTick,
        bool sendLanguage,
        size_t maximum,
        std::vector<RenderedMessage>& messages)
    {
        if (!sequence.Lyrics || sequence.Lyrics->TrackId.empty() || sequence.Lyrics->TrackId != track.Id || fromTick >= toTick)
        {
            return;
        }

        auto const& lyrics = *sequence.Lyrics;
        auto const& syllables = lyrics.Syllables;

        auto const add = [&messages, maximum](int64_t tick, std::array<uint32_t, 4> const& words)
        {
            if (messages.size() < maximum)
            {
                RenderedMessage message{};
                message.Tick = tick;
                message.Words = words;
                message.WordCount = 4;
                message.Kind = RenderedKind::Other;
                messages.push_back(message);
            }
        };

        auto const addLanguage = [&](int64_t tick)
        {
            for (auto const& packet : LyricsLanguagePackets(lyrics.Language, 0))
            {
                add(tick, packet);
            }

            for (auto const& packet : RubyLanguagePackets(lyrics.RubyLanguage, 0))
            {
                add(tick, packet);
            }
        };

        auto it = std::lower_bound(syllables.begin(), syllables.end(), fromTick,
            [](Lyric const& syllable, int64_t tick) { return syllable.Tick < tick; });

        if (sendLanguage)
        {
            addLanguage(fromTick);
        }
        else if (it == syllables.begin() && it != syllables.end() && it->Tick < toTick)
        {
            addLanguage(it->Tick);
        }

        for (; it != syllables.end() && it->Tick < toTick && messages.size() < maximum; ++it)
        {
            for (auto const& packet : LyricDataPackets(*it, 0))
            {
                add(it->Tick, packet);
            }

            for (auto const& packet : RubyDataPackets(*it, 0))
            {
                add(it->Tick, packet);
            }
        }
    }

    // ---- LRC ----

    _Use_decl_annotations_
    std::string LyricsToLrc(LyricsTrack const& lyrics, std::vector<TempoPoint> const& tempo, std::wstring_view title)
    {
        TempoMap const map{ tempo };

        std::string lrc{ "\xEF\xBB\xBF" };

        // A tag's value ends at the bracket, and a line at a line break.
        auto const tagValue = [](std::wstring_view text)
        {
            std::wstring value{};

            for (auto const c : text)
            {
                value.push_back(c == L']' || c == L'[' || IsControl(c) ? L' ' : c);
            }

            return ToUtf8(value);
        };

        if (!title.empty())
        {
            lrc += "[ti:" + tagValue(title) + "]\n";
        }

        if (!lyrics.Language.empty())
        {
            lrc += "[la:" + tagValue(lyrics.Language) + "]\n";
        }

        auto const lines = LyricLines(lyrics.Syllables);

        for (size_t i = 0; i < lines.size(); ++i)
        {
            auto const& line = lines[i];
            lrc += LrcTime(map.SecondsAtTick(line.StartTick));
            lrc += ToUtf8(LineString(lyrics.Syllables, line));
            lrc += '\n';

            // An empty line clears the words between verses.
            if (line.EndsVerse)
            {
                auto const last = lyrics.Syllables[line.First + line.Count - 1].Tick;
                auto const clear = std::min(line.EndTick, last + 2 * TicksPerQuarterNote);

                if (i + 1 == lines.size() || clear < lines[i + 1].StartTick)
                {
                    lrc += LrcTime(map.SecondsAtTick(clear));
                    lrc += '\n';
                }
            }
        }

        return lrc;
    }

    // ---- the sequence file ----

    _Use_decl_annotations_
    std::wstring LyricsToJsonText(LyricsTrack const& lyrics)
    {
        std::wstring out{ L"{" };
        bool first{ true };

        auto const member = [&](std::wstring_view key)
        {
            out += first ? L"\n    " : L",\n    ";
            first = false;
            out += Quote(key);
            out += L": ";
        };

        if (!lyrics.Language.empty())
        {
            member(KeyLanguage);
            out += Quote(lyrics.Language);
        }

        if (!lyrics.RubyLanguage.empty())
        {
            member(KeyRubyLanguage);
            out += Quote(lyrics.RubyLanguage);
        }

        if (!lyrics.TrackId.empty())
        {
            member(KeyTrack);
            out += Quote(lyrics.TrackId);
        }

        member(KeySyllables);
        out += L"[";

        for (size_t i = 0; i < lyrics.Syllables.size(); ++i)
        {
            auto const& syllable = lyrics.Syllables[i];

            out += i == 0 ? L"\n      " : L",\n      ";
            out += std::format(L"[{}, {}", syllable.Tick, Quote(syllable.Text));

            if (syllable.End != LyricEnd::None)
            {
                out += L", ";
                out += Quote(EndName(syllable.End));
            }

            // What isn't text or an end goes in an object, so a reader can tell it apart.
            if (!syllable.Ruby.empty())
            {
                out += std::format(L", {{{}: {}}}", Quote(KeyRuby), Quote(syllable.Ruby));
            }

            out += L"]";
        }

        out += lyrics.Syllables.empty() ? L"]" : L"\n    ]";

        if (lyrics.Unknown != nullptr)
        {
            std::vector<std::pair<std::wstring, std::wstring>> pairs{};

            for (auto const& pair : lyrics.Unknown)
            {
                pairs.emplace_back(std::wstring{ pair.Key() }, std::wstring{ pair.Value().Stringify() });
            }

            std::sort(pairs.begin(), pairs.end());

            for (auto const& [key, value] : pairs)
            {
                member(key);
                out += value;
            }
        }

        out += L"\n  }";
        return out;
    }

    _Use_decl_annotations_
    std::optional<LyricsTrack> ReadLyricsJson(json::JsonObject const& root, SequenceLimits const& limits, size_t& skipped)
    {
        if (root == nullptr || !root.HasKey(LyricsKey))
        {
            return std::nullopt;
        }

        auto const value = root.Lookup(LyricsKey);

        if (value == nullptr || value.ValueType() != json::JsonValueType::Object)
        {
            ++skipped;
            return std::nullopt;
        }

        auto const object = value.GetObject();

        LyricsTrack lyrics{};
        lyrics.Language = StringMember(object, KeyLanguage);
        lyrics.RubyLanguage = StringMember(object, KeyRubyLanguage);
        lyrics.TrackId = StringMember(object, KeyTrack);

        if (object.HasKey(KeySyllables))
        {
            auto const list = object.Lookup(KeySyllables);

            if (list == nullptr || list.ValueType() != json::JsonValueType::Array)
            {
                ++skipped;
            }
            else
            {
                for (auto const& entry : list.GetArray())
                {
                    if (lyrics.Syllables.size() >= limits.MaximumLyrics || entry.ValueType() != json::JsonValueType::Array)
                    {
                        ++skipped;
                        continue;
                    }

                    auto const fields = entry.GetArray();

                    if (fields.Size() < 2 ||
                        fields.GetAt(0).ValueType() != json::JsonValueType::Number ||
                        fields.GetAt(1).ValueType() != json::JsonValueType::String)
                    {
                        ++skipped;
                        continue;
                    }

                    auto const tick = fields.GetAt(0).GetNumber();

                    if (!std::isfinite(tick))
                    {
                        ++skipped;
                        continue;
                    }

                    Lyric syllable{};
                    syllable.Tick = static_cast<int64_t>(std::clamp(std::round(tick), 0.0, static_cast<double>(limits.MaximumTick)));
                    syllable.Text = CleanText(fields.GetAt(1).GetString(), limits.MaximumTextLength);

                    for (uint32_t k = 2; k < std::min<uint32_t>(fields.Size(), 4); ++k)
                    {
                        auto const field = fields.GetAt(k);

                        if (k == 2 && field.ValueType() == json::JsonValueType::String)
                        {
                            syllable.End = EndFromName(field.GetString());
                        }
                        else if (field.ValueType() == json::JsonValueType::Object)
                        {
                            syllable.Ruby = CleanText(StringMember(field.GetObject(), KeyRuby), limits.MaximumTextLength);
                        }
                    }

                    lyrics.Syllables.push_back(std::move(syllable));
                }
            }
        }

        for (auto const& pair : object)
        {
            std::wstring_view const key{ pair.Key() };

            if (key == KeyLanguage || key == KeyRubyLanguage || key == KeyTrack || key == KeySyllables)
            {
                continue;
            }

            if (lyrics.Unknown == nullptr)
            {
                lyrics.Unknown = json::JsonObject{};
            }

            lyrics.Unknown.SetNamedValue(pair.Key(), pair.Value());
        }

        NormalizeLyrics(lyrics, limits);
        return lyrics;
    }
}
