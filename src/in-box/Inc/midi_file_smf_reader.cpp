// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Deliberately free of pch.h, WinRT and XAML so that this file compiles unchanged into the unit
// test project. See SmfReader.h for the rules this reader follows about untrusted input.

#include "midi_file_smf_reader.h"
#include "midi_file_common_characters.h"

#include <windows.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <climits>
#include <string>
#include <string_view>

namespace midifile
{
    namespace
    {
        constexpr uint32_t MaximumTick = 0xFFFFFFF0;

        constexpr uint8_t MetaEventStatus = 0xFF;
        constexpr uint8_t SystemExclusiveStatus = 0xF0;
        constexpr uint8_t SystemExclusiveEndStatus = 0xF7;

        constexpr uint8_t MetaEndOfTrack = 0x2F;
        constexpr uint8_t MetaSetTempo = 0x51;
        constexpr uint8_t MetaTimeSignature = 0x58;
        constexpr uint8_t MetaKeySignature = 0x59;

        constexpr uint8_t ControllerBankSelectMsb = 0x00;
        constexpr uint8_t ControllerBankSelectLsb = 0x20;

        // Chord symbols in the wild travel in a manufacturer system exclusive, because version 1
        // of the file format never defined an event for them:
        //   F0 00 20 24 00 01 <chord name in ASCII> 0A F7
        // Seen across a large corpus of real files, always carrying names like "Bb", "F#m7" or
        // "D/F#" for a lead sheet display. The message is still passed to the device as well as
        // being read here, since it is the manufacturer's own and may mean something to them.
        constexpr uint8_t ChordSymbolPrefix[]{ 0xF0, 0x00, 0x20, 0x24, 0x00, 0x01 };
        constexpr size_t MaximumChordSymbolLength = 32;

        class ByteReader
        {
        public:
            explicit ByteReader(std::span<uint8_t const> bytes) noexcept : m_bytes(bytes) {}

            size_t Offset() const noexcept { return m_offset; }
            size_t Remaining() const noexcept { return m_offset >= m_bytes.size() ? 0 : m_bytes.size() - m_offset; }
            bool AtEnd() const noexcept { return Remaining() == 0; }

            bool TryPeek(uint8_t& value) const noexcept
            {
                if (Remaining() == 0)
                {
                    return false;
                }

                value = m_bytes[m_offset];
                return true;
            }

            bool TryReadByte(uint8_t& value) noexcept
            {
                if (!TryPeek(value))
                {
                    return false;
                }

                ++m_offset;
                return true;
            }

            bool TryReadUInt16(uint16_t& value) noexcept
            {
                if (Remaining() < 2)
                {
                    return false;
                }

                value = static_cast<uint16_t>((m_bytes[m_offset] << 8) | m_bytes[m_offset + 1]);
                m_offset += 2;

                return true;
            }

            bool TryReadUInt32(uint32_t& value) noexcept
            {
                if (Remaining() < 4)
                {
                    return false;
                }

                value = (static_cast<uint32_t>(m_bytes[m_offset]) << 24)
                    | (static_cast<uint32_t>(m_bytes[m_offset + 1]) << 16)
                    | (static_cast<uint32_t>(m_bytes[m_offset + 2]) << 8)
                    | static_cast<uint32_t>(m_bytes[m_offset + 3]);

                m_offset += 4;

                return true;
            }

            // RP-001 variable length quantity. Four bytes is the documented ceiling, and stopping
            // there is what stops a run of high-bit bytes from being read as one enormous number.
            bool TryReadVariableLength(uint32_t& value) noexcept
            {
                value = 0;

                for (int index = 0; index < 4; ++index)
                {
                    uint8_t byte{};

                    if (!TryReadByte(byte))
                    {
                        return false;
                    }

                    value = (value << 7) | static_cast<uint32_t>(byte & 0x7F);

                    if ((byte & 0x80) == 0)
                    {
                        return true;
                    }
                }

                return false;
            }

            bool TryReadSpan(size_t count, std::span<uint8_t const>& value) noexcept
            {
                if (Remaining() < count)
                {
                    return false;
                }

                value = m_bytes.subspan(m_offset, count);
                m_offset += count;

                return true;
            }

            bool TrySkip(size_t count) noexcept
            {
                if (Remaining() < count)
                {
                    m_offset = m_bytes.size();
                    return false;
                }

                m_offset += count;
                return true;
            }

            void SeekTo(size_t offset) noexcept
            {
                m_offset = offset > m_bytes.size() ? m_bytes.size() : offset;
            }

        private:
            std::span<uint8_t const> m_bytes{};
            size_t m_offset{ 0 };
        };

        bool ChunkIdIs(std::span<uint8_t const> bytes, size_t offset, char const* id) noexcept
        {
            if (offset + 4 > bytes.size())
            {
                return false;
            }

            for (size_t index = 0; index < 4; ++index)
            {
                if (bytes[offset + index] != static_cast<uint8_t>(id[index]))
                {
                    return false;
                }
            }

            return true;
        }

        bool HasNonAsciiByte(_In_ std::span<uint8_t const> bytes) noexcept
        {
            return std::any_of(bytes.begin(), bytes.end(), [](uint8_t value) noexcept { return value >= 0x80; });
        }

        // True when the bytes are UTF-8. usable is how many of them to decode: all of them, unless
        // the text was cut at MaximumTextBytes part way through a character, in which case the cut
        // character is left off. A cut that would leave no other character outside ASCII is not
        // treated as UTF-8, so that whether a text decodes to nothing never depends on the encoding.
        bool TryMeasureUtf8(
            _In_ std::span<uint8_t const> bytes,
            _In_ bool truncated,
            _Out_ size_t& usable) noexcept
        {
            usable = bytes.size();

            size_t index = 0;
            bool sawNonAscii = false;

            while (index < bytes.size())
            {
                auto const lead = bytes[index];

                if (lead < 0x80)
                {
                    ++index;
                    continue;
                }

                size_t continuations = 0;

                if ((lead & 0xE0) == 0xC0) { continuations = 1; }
                else if ((lead & 0xF0) == 0xE0) { continuations = 2; }
                else if ((lead & 0xF8) == 0xF0) { continuations = 3; }
                else { return false; }

                auto const remaining = bytes.size() - index - 1;
                auto const available = continuations < remaining ? continuations : remaining;

                for (size_t offset = 1; offset <= available; ++offset)
                {
                    if ((bytes[index + offset] & 0xC0) != 0x80)
                    {
                        return false;
                    }
                }

                if (available < continuations)
                {
                    if (!truncated || !sawNonAscii)
                    {
                        return false;
                    }

                    usable = index;
                    return true;
                }

                sawNonAscii = true;
                index += continuations + 1;
            }

            return true;
        }

        // Control characters in a track name wreck a text block's layout.
        //
        // Lyrics are the one kind where the whitespace is the content: a karaoke file marks its
        // word breaks with a trailing space and its line breaks with a carriage return, so tidying
        // those away would run a whole verse together.
        void TidyText(_Inout_ std::string& text, _In_ bool preserveWhitespace) noexcept
        {
            std::erase_if(text, [preserveWhitespace](char value) noexcept
                {
                    auto const byte = static_cast<unsigned char>(value);

                    if (preserveWhitespace && (byte == '\r' || byte == '\n'))
                    {
                        return false;
                    }

                    return byte < 0x20 && byte != '\t';
                });

            while (!preserveWhitespace && !text.empty() && (text.back() == ' ' || text.back() == '\t'))
            {
                text.pop_back();
            }
        }

        // Text in a Standard MIDI File is bytes with no declared encoding. Modern files are
        // usually UTF-8, older Western ones are Latin-1, older Japanese ones are Shift-JIS, older
        // Chinese ones are GBK or Big5, and older Korean ones are code page 949.
        //
        // This decodes one text on its own: UTF-8 when it is valid UTF-8, and Latin-1 otherwise,
        // which cannot fail. The double-byte code pages can't be told apart from Latin-1, or from
        // each other, one short text at a time, so that decision is made once for the whole file,
        // after every text has been read. See TextEncodingDetector.
        std::string DecodeText(
            _In_ std::span<uint8_t const> bytes,
            _In_ bool preserveWhitespace,
            _In_ bool truncated) noexcept
        {
            std::string text{};

            if (bytes.empty())
            {
                return text;
            }

            size_t usable{};

            if (TryMeasureUtf8(bytes, truncated, usable))
            {
                text.assign(reinterpret_cast<char const*>(bytes.data()), usable);
            }
            else
            {
                text.reserve(bytes.size() * 2);

                for (auto const byte : bytes)
                {
                    if (byte < 0x80)
                    {
                        text.push_back(static_cast<char>(byte));
                    }
                    else
                    {
                        text.push_back(static_cast<char>(0xC0 | (byte >> 6)));
                        text.push_back(static_cast<char>(0x80 | (byte & 0x3F)));
                    }
                }
            }

            TidyText(text, preserveWhitespace);

            return text;
        }

        constexpr UINT ShiftJisCodePage = 932;

        bool IsShiftJisLeadByte(_In_ uint8_t value) noexcept
        {
            return (value >= 0x81 && value <= 0x9F) || (value >= 0xE0 && value <= 0xFC);
        }

        bool IsAsciiLetter(_In_ uint8_t value) noexcept
        {
            return (value >= 'A' && value <= 'Z') || (value >= 'a' && value <= 'z');
        }

        constexpr UINT GbkCodePage = 936;
        constexpr UINT Big5CodePage = 950;
        constexpr UINT KoreanCodePage = 949;

        // A character that sits inside a Western word, right after an ASCII letter or right before
        // a lowercase one, is more likely two accented letters than one CJK character, so it is
        // never allowed to count in favor of an encoding.
        int ScoreCharacter(_In_ int value, _In_ bool insideWord) noexcept
        {
            return insideWord && value > 0 ? 0 : value;
        }

        // Decodes with a double-byte code page to UTF-8, refusing anything that isn't clean.
        //
        // MB_ERR_INVALID_CHARS rejects codes the code page doesn't define and a lead byte without
        // a trail byte. Code pages 932 and 936 still accept codes real text doesn't use: 0x80
        // becomes a control character in 932, and the user defined areas of both become private
        // use characters no font draws. Those are refused here.
        //
        // Allocates, so it can throw std::bad_alloc.
        bool TryDecodeStrictly(
            _In_ UINT codePage,
            _In_ std::span<uint8_t const> bytes,
            _Out_ std::string& utf8)
        {
            utf8.clear();

            if (bytes.empty())
            {
                return true;
            }

            if (bytes.size() > static_cast<size_t>(INT_MAX))
            {
                return false;
            }

            auto const source = reinterpret_cast<char const*>(bytes.data());
            auto const sourceLength = static_cast<int>(bytes.size());

            auto const wideLength = ::MultiByteToWideChar(
                codePage, MB_ERR_INVALID_CHARS, source, sourceLength, nullptr, 0);

            if (wideLength <= 0)
            {
                return false;
            }

            std::wstring wide(static_cast<size_t>(wideLength), L'\0');

            if (::MultiByteToWideChar(
                codePage, MB_ERR_INVALID_CHARS, source, sourceLength, wide.data(), wideLength) != wideLength)
            {
                return false;
            }

            for (auto const character : wide)
            {
                if ((character >= 0x80 && character <= 0x9F) || (character >= 0xE000 && character <= 0xF8FF))
                {
                    return false;
                }
            }

            auto const utf8Length = ::WideCharToMultiByte(
                CP_UTF8, 0, wide.data(), wideLength, nullptr, 0, nullptr, nullptr);

            if (utf8Length <= 0)
            {
                return false;
            }

            utf8.resize(static_cast<size_t>(utf8Length));

            if (::WideCharToMultiByte(
                CP_UTF8, 0, wide.data(), wideLength, utf8.data(), utf8Length, nullptr, nullptr) != utf8Length)
            {
                utf8.clear();
                return false;
            }

            return true;
        }

        // Half-width ｳ, ｶ to ﾄ and ﾊ to ﾎ can take the voicing mark ﾞ; ﾊ to ﾎ can take ﾟ.
        bool TakesDakuten(_In_ uint8_t halfWidth) noexcept
        {
            return halfWidth == 0xB3 || (halfWidth >= 0xB6 && halfWidth <= 0xC4) || (halfWidth >= 0xCA && halfWidth <= 0xCE);
        }

        bool TakesHandakuten(_In_ uint8_t halfWidth) noexcept
        {
            return halfWidth >= 0xCA && halfWidth <= 0xCE;
        }

        // Decodes Shift-JIS (Windows code page 932) to UTF-8. Returns false when the bytes are not
        // clean Shift-JIS: an unknown double-byte code, a lead byte with no trail byte, or a code
        // that only means something to one vendor's software. A lead byte cut off by
        // MaximumTextBytes is left off rather than counted against the text.
        //
        // sawJapanese is set when the text holds a double-byte character that Western text can't
        // easily produce by accident:
        //
        //  - Its lead byte is from 0x81 to 0x9F. Latin-1 uses those for control codes. Accented
        //    letters in Latin-1 (0xC0 to 0xFF) can also form valid Shift-JIS pairs, as in "Canção",
        //    so they never count.
        //  - Its trail byte is above 0x7F, or its lead byte is 0x81 to 0x83, the rows that hold
        //    Japanese punctuation, hiragana and katakana.
        //  - It doesn't come straight after an ASCII letter, and the byte after it isn't a lowercase
        //    ASCII letter. Old DOS files keep accented letters at 0x80 to 0x9F, and Windows-1252
        //    keeps curly quotes there, and both sit inside a word: "Müde", "Flügel", "Über",
        //    "don’t". Japanese text rarely puts a character between two ASCII letters like that.
        //
        // score is how much the text looks like real Japanese, for choosing between Shift-JIS and
        // GBK when the same bytes read cleanly in both (see ScoreCharacter). Half-width katakana,
        // single bytes from 0xA1 to 0xDF, are what GBK text read as Shift-JIS is mostly made of,
        // but older Japanese files use them a lot too, so on their own they count for nothing.
        // A voicing mark on a kana that takes it does count, and is also evidence of Japanese.
        //
        // Allocates, so it can throw std::bad_alloc.
        bool TryDecodeShiftJis(
            _In_ std::span<uint8_t const> bytes,
            _In_ bool truncated,
            _Out_ std::string& utf8,
            _Inout_ bool& sawJapanese,
            _Inout_ int& score)
        {
            utf8.clear();

            size_t usable = bytes.size();
            bool afterAsciiLetter = false;
            uint8_t previousHalfWidth = 0;
            bool openedBracket = false;

            // Half-width kana count for the text only once it shows the structure above, which
            // short Chinese or Korean text read as Shift-JIS doesn't have.
            int halfWidthKana = 0;
            bool structured = false;

            for (size_t index = 0; index < bytes.size();)
            {
                auto const lead = bytes[index];

                if (!IsShiftJisLeadByte(lead))
                {
                    // Half-width katakana have structure that GBK text read as Shift-JIS doesn't:
                    // the voicing marks ﾞ and ﾟ only follow kana that can take them, as in ﾌﾟ and
                    // ﾄﾞ, and the long vowel mark ｰ follows a kana, as in ﾘｭｰ.
                    if (lead == 0xDE && TakesDakuten(previousHalfWidth))
                    {
                        sawJapanese = true;
                        structured = true;
                        score += 2;
                    }
                    else if (lead == 0xDF && TakesHandakuten(previousHalfWidth))
                    {
                        sawJapanese = true;
                        structured = true;
                        score += 2;
                    }
                    else if (lead == 0xB0 && previousHalfWidth >= 0xA6 && previousHalfWidth <= 0xDF)
                    {
                        score += 1;
                    }
                    else if ((lead == 0xA2 && previousHalfWidth == 0 && index + 1 < bytes.size() &&
                                 bytes[index + 1] >= 0xA6 && bytes[index + 1] <= 0xDD) ||
                        (lead == 0xA3 && openedBracket && previousHalfWidth >= 0xA6 && previousHalfWidth <= 0xDF))
                    {
                        // The half-width corner brackets ｢ and ｣ around kana, as in ｢ﾏﾁ｣. The
                        // opening one starts a word and the closing one only follows an opening one.
                        openedBracket = lead == 0xA2;
                        sawJapanese = true;
                        structured = true;
                        score += 2;
                    }
                    else if (lead >= 0xA6 && lead <= 0xDD)
                    {
                        ++halfWidthKana;
                    }

                    previousHalfWidth = (lead >= 0xA1 && lead <= 0xDF) ? lead : 0;
                    afterAsciiLetter = IsAsciiLetter(lead);
                    ++index;
                    continue;
                }

                previousHalfWidth = 0;

                if (index + 1 >= bytes.size())
                {
                    if (!truncated)
                    {
                        return false;
                    }

                    usable = index;
                    break;
                }

                auto const trail = bytes[index + 1];
                auto const beforeLowercaseLetter =
                    index + 2 < bytes.size() && bytes[index + 2] >= 'a' && bytes[index + 2] <= 'z';
                auto const insideWord = afterAsciiLetter || beforeLowercaseLetter;

                if (lead <= 0x9F && (lead <= 0x83 || trail >= 0x80) && !insideWord)
                {
                    sawJapanese = true;
                }

                // Rows 0x82 and 0x83 are hiragana and katakana, 0x81 is punctuation, 0x88 to 0x98
                // are the everyday kanji of JIS level 1. Rows 0x87, 0xED, 0xEE and 0xFA to 0xFC
                // are vendor extensions that real text seldom needs.
                int value = 0;

                if (lead == 0x82 || lead == 0x83) { value = 2; }
                else if (lead == 0x81 || (lead >= 0x88 && lead <= 0x98)) { value = 1; }
                else if (lead == 0x87 || lead == 0xED || lead == 0xEE || lead >= 0xFA) { value = -1; }

                score += ScoreCharacter(value, insideWord);

                afterAsciiLetter = false;
                index += 2;
            }

            if (structured)
            {
                score += halfWidthKana;
            }

            return TryDecodeStrictly(ShiftJisCodePage, bytes.first(usable), utf8);
        }

        bool IsCommon(_In_ std::span<uint16_t const> table, _In_ uint8_t lead, _In_ uint8_t trail) noexcept
        {
            return std::binary_search(table.begin(), table.end(), static_cast<uint16_t>((lead << 8) | trail));
        }

        // How a text reads in one of the code pages where every byte from 0x81 to 0xFE starts a
        // two byte character: GBK, Big5 and Korean. None of them has anything like half-width
        // katakana.
        struct DoubleByteReading
        {
            int Score{ 0 };

            // At least one character that Western text can't easily make by accident: one with a
            // trail byte of 0xA1 or above that isn't inside a word, or one that comes right after
            // another. Code pages whose trail bytes can be ASCII letters need this, because a
            // Latin-1 symbol followed by a letter, as in ©M or °C, is one valid character in them.
            bool Evidence{ false };
        };

        // Walks the text in a double-byte code page, adding valueOf(lead, trail) for each
        // character to the score (see ScoreCharacter), and then decodes it strictly. Returns false
        // when the bytes are not clean in that code page. A lead byte cut off by
        // MaximumTextBytes is left off rather than counted against the text.
        //
        // scoreKanaRuns adds 2 for a GB2312 kana, rows 0xA4 and 0xA5, that comes right after
        // another one. Japanese written in GBK reads as runs of kana, and Big5 text, whose most
        // common characters sit on those same two rows, reads as kana scattered among hanzi.
        //
        // Allocates, so it can throw std::bad_alloc.
        template <typename ValueOf>
        bool TryDecodeDoubleByte(
            _In_ UINT codePage,
            _In_ std::span<uint8_t const> bytes,
            _In_ bool truncated,
            _In_ ValueOf const& valueOf,
            _In_ bool scoreKanaRuns,
            _Out_ std::string& utf8,
            _Inout_ DoubleByteReading& reading)
        {
            utf8.clear();

            size_t usable = bytes.size();
            bool afterAsciiLetter = false;
            bool afterDoubleByte = false;
            bool afterKana = false;

            for (size_t index = 0; index < bytes.size();)
            {
                auto const lead = bytes[index];

                if (lead < 0x80)
                {
                    afterAsciiLetter = IsAsciiLetter(lead);
                    afterDoubleByte = false;
                    afterKana = false;
                    ++index;
                    continue;
                }

                // These code pages read 0x80 alone as the euro sign, which their text doesn't use
                // but Western text has at 0x80.
                if (lead == 0x80 || lead == 0xFF)
                {
                    return false;
                }

                if (index + 1 >= bytes.size())
                {
                    if (!truncated)
                    {
                        return false;
                    }

                    usable = index;
                    break;
                }

                auto const trail = bytes[index + 1];
                auto const beforeLowercaseLetter =
                    index + 2 < bytes.size() && bytes[index + 2] >= 'a' && bytes[index + 2] <= 'z';
                auto const insideWord = afterAsciiLetter || beforeLowercaseLetter;

                reading.Score += ScoreCharacter(valueOf(lead, trail), insideWord);

                if ((trail >= 0xA1 && !insideWord) || afterDoubleByte)
                {
                    reading.Evidence = true;
                }

                auto const kana = scoreKanaRuns && (lead == 0xA4 || lead == 0xA5) && trail >= 0xA1;

                if (kana && afterKana)
                {
                    reading.Score += 2;
                }

                afterKana = kana;
                afterDoubleByte = true;
                afterAsciiLetter = false;
                index += 2;
            }

            return TryDecodeStrictly(codePage, bytes.first(usable), utf8);
        }

        // Every value below adds 2 for a character in the code page's list of common characters
        // (midi_file_common_characters.h), the strongest sign of real text.
        //
        // GBK (code page 936), older Simplified Chinese files and some Japanese ones. Characters
        // from the GB2312 core (both bytes 0xA1 or above) count for it: rows 0xA4 and 0xA5, which
        // hold hiragana and katakana, the punctuation rows 0xA1 and 0xA3, and the common hanzi
        // from 0xB0 to 0xD7. Characters only GBK added, with a trail byte below 0xA1, count
        // against it, because that is what Shift-JIS text read as GBK is mostly made of. So do
        // row 0xA2, circled and Roman numerals, which is where the half-width bracket ｢ of
        // Shift-JIS lands, and row 0xA8, pinyin and bopomofo, which titles seldom use.
        int GbkValue(_In_ uint8_t lead, _In_ uint8_t trail) noexcept
        {
            int value = -1;

            if (lead >= 0xA1 && lead <= 0xF7 && trail >= 0xA1 && trail <= 0xFE)
            {
                if (lead == 0xA4 || lead == 0xA5) { value = 2; }
                else if (lead == 0xA2 || lead == 0xA8) { value = -1; }
                else if (lead <= 0xA3 || (lead >= 0xB0 && lead <= 0xD7)) { value = 1; }
                else { value = 0; }
            }

            return IsCommon(common_characters::Gbk, lead, trail) ? value + 2 : value;
        }

        // Big5 (code page 950), older Traditional Chinese files from Taiwan and Hong Kong. Row
        // 0xA1, punctuation, and the frequently used hanzi from 0xA440 to 0xC67E count for it.
        int Big5Value(_In_ uint8_t lead, _In_ uint8_t trail) noexcept
        {
            auto const code = static_cast<uint16_t>((lead << 8) | trail);
            int value = 0;

            if (lead == 0xA1 || (code >= 0xA440 && code <= 0xC67E))
            {
                value = 1;
            }

            return IsCommon(common_characters::Big5, lead, trail) ? value + 2 : value;
        }

        // Korean (code page 949: EUC-KR and the Unified Hangul Code extension). The Hangul
        // syllables from 0xB0 to 0xC8 and the symbols of row 0xA1 count for it, and 0xA8 0xCF, the
        // circled c that Korean files use as their copyright sign, counts a lot: it is often the
        // only Korean in an otherwise English file, and Big5 reads it as a common hanzi. The loose
        // letters of row 0xA4, which read like ㄴㄴㄴ, and the extension's syllables, which only
        // rare words need, count against it.
        int KoreanValue(_In_ uint8_t lead, _In_ uint8_t trail) noexcept
        {
            int value = -1;

            if (lead >= 0xA1 && trail >= 0xA1 && trail <= 0xFE)
            {
                if (lead >= 0xB0 && lead <= 0xC8) { value = 1; }
                else if (lead == 0xA1) { value = 1; }
                else if (lead == 0xA8 && trail == 0xCF) { value = 4; }
                else if (lead == 0xA4) { value = -1; }
                else { value = 0; }
            }

            return IsCommon(common_characters::Korean, lead, trail) ? value + 2 : value;
        }

        constexpr UINT AnsiCodePage = 1252;

        // Allocates, so it can throw std::bad_alloc.
        std::string WideToUtf8(_In_ std::wstring_view wide)
        {
            std::string utf8{};

            if (wide.empty() || wide.size() > static_cast<size_t>(INT_MAX))
            {
                return utf8;
            }

            auto const wideLength = static_cast<int>(wide.size());
            auto const length = ::WideCharToMultiByte(CP_UTF8, 0, wide.data(), wideLength, nullptr, 0, nullptr, nullptr);

            if (length <= 0)
            {
                return utf8;
            }

            utf8.resize(static_cast<size_t>(length));

            if (::WideCharToMultiByte(CP_UTF8, 0, wide.data(), wideLength, utf8.data(), length, nullptr, nullptr) != length)
            {
                utf8.clear();
            }

            return utf8;
        }

        // Decodes with a Windows code page to UTF-8. Anything the code page can't map is replaced
        // rather than refused, because the encoding is already known here, not being guessed.
        //
        // Allocates, so it can throw std::bad_alloc.
        std::string DecodeCodePage(_In_ UINT codePage, _In_ std::span<uint8_t const> bytes)
        {
            if (bytes.empty() || bytes.size() > static_cast<size_t>(INT_MAX))
            {
                return {};
            }

            auto const source = reinterpret_cast<char const*>(bytes.data());
            auto const sourceLength = static_cast<int>(bytes.size());
            auto const wideLength = ::MultiByteToWideChar(codePage, 0, source, sourceLength, nullptr, 0);

            if (wideLength <= 0)
            {
                return {};
            }

            std::wstring wide(static_cast<size_t>(wideLength), L'\0');

            if (::MultiByteToWideChar(codePage, 0, source, sourceLength, wide.data(), wideLength) != wideLength)
            {
                return {};
            }

            return WideToUtf8(wide);
        }

        // How many bytes hold whole Shift-JIS characters, leaving off a lead byte at the very end
        // that has no trail byte.
        size_t ShiftJisCompleteLength(_In_ std::span<uint8_t const> bytes) noexcept
        {
            size_t index = 0;

            while (index < bytes.size())
            {
                if (!IsShiftJisLeadByte(bytes[index]))
                {
                    ++index;
                }
                else if (index + 1 >= bytes.size())
                {
                    return index;
                }
                else
                {
                    index += 2;
                }
            }

            return bytes.size();
        }

        enum class ByteOrderMark : uint8_t
        {
            None,
            Utf8,
            Utf16LittleEndian,
            Utf16BigEndian,
        };

        ByteOrderMark ReadByteOrderMark(_In_ std::span<uint8_t const> bytes) noexcept
        {
            if (bytes.size() >= 3 && bytes[0] == 0xEF && bytes[1] == 0xBB && bytes[2] == 0xBF)
            {
                return ByteOrderMark::Utf8;
            }

            if (bytes.size() >= 2 && bytes[0] == 0xFF && bytes[1] == 0xFE)
            {
                return ByteOrderMark::Utf16LittleEndian;
            }

            if (bytes.size() >= 2 && bytes[0] == 0xFE && bytes[1] == 0xFF)
            {
                return ByteOrderMark::Utf16BigEndian;
            }

            return ByteOrderMark::None;
        }

        // RP-026: text that starts with a Unicode byte order mark is Unicode, whatever else the
        // file says. The mark covers only the event it starts, because text without one can't be
        // recognized as UTF-16 reliably. Empty on failure.
        std::string DecodeUnicodeText(
            _In_ std::span<uint8_t const> bytes,
            _In_ ByteOrderMark mark,
            _In_ bool preserveWhitespace) noexcept
        {
            try
            {
                std::string text{};

                if (mark == ByteOrderMark::Utf8)
                {
                    auto const body = bytes.subspan(3);
                    size_t usable{};

                    if (TryMeasureUtf8(body, true, usable))
                    {
                        text.assign(reinterpret_cast<char const*>(body.data()), usable);
                    }
                    else
                    {
                        text = DecodeCodePage(CP_UTF8, body);
                    }
                }
                else
                {
                    auto const body = bytes.subspan(2);
                    auto const bigEndian = mark == ByteOrderMark::Utf16BigEndian;

                    std::wstring wide(body.size() / 2, L'\0');

                    for (size_t index = 0; index < wide.size(); ++index)
                    {
                        auto const first = body[index * 2];
                        auto const second = body[index * 2 + 1];

                        wide[index] = bigEndian
                            ? static_cast<wchar_t>((first << 8) | second)
                            : static_cast<wchar_t>((second << 8) | first);
                    }

                    // Half of a surrogate pair, cut by MaximumTextBytes.
                    if (!wide.empty() && wide.back() >= 0xD800 && wide.back() <= 0xDBFF)
                    {
                        wide.pop_back();
                    }

                    text = WideToUtf8(wide);
                }

                TidyText(text, preserveWhitespace);

                return text;
            }
            catch (...)
            {
                return {};
            }
        }

        // RP-026 character code sets, chosen by a {@<code_set>} tag in a lyric event.
        enum class CharacterCodeSet : uint8_t
        {
            None,       // no tag seen yet
            Latin,      // {@LATIN}: ANSI, read as Windows code page 1252
            Japanese,   // {@JP}: MS-Kanji, which is Shift-JIS, read as Windows code page 932
            Unknown,    // a tag RP-026 doesn't define
        };

        bool EqualsIgnoringAsciiCase(_In_ std::span<uint8_t const> bytes, _In_ char const* expected) noexcept
        {
            size_t index = 0;

            for (; expected[index] != '\0'; ++index)
            {
                if (index >= bytes.size())
                {
                    return false;
                }

                auto value = bytes[index];

                if (value >= 'A' && value <= 'Z')
                {
                    value = static_cast<uint8_t>(value - 'A' + 'a');
                }

                if (value != static_cast<uint8_t>(expected[index]))
                {
                    return false;
                }
            }

            return index == bytes.size();
        }

        // Reads the code set tags at the start of a lyric event and returns what follows them.
        // codeSet is left alone when there is no tag. Something that starts like a tag but has no
        // closing brace within a short distance is not a tag, and is left as text.
        //
        // RP-026 spells the names LATIN, Latin or latin, and JP, Jp or jp. Any mix of case is
        // accepted here, and so are spaces around the name.
        std::span<uint8_t const> ReadCodeSetTags(
            _In_ std::span<uint8_t const> bytes,
            _Inout_ CharacterCodeSet& codeSet) noexcept
        {
            constexpr size_t MaximumTagLength = 32;

            while (bytes.size() >= 3 && bytes[0] == '{' && bytes[1] == '@')
            {
                size_t close = 2;

                while (close < bytes.size() && close < MaximumTagLength && bytes[close] != '}')
                {
                    ++close;
                }

                if (close >= bytes.size() || bytes[close] != '}')
                {
                    break;
                }

                auto name = bytes.subspan(2, close - 2);

                while (!name.empty() && name.front() == ' ') { name = name.subspan(1); }
                while (!name.empty() && name.back() == ' ') { name = name.first(name.size() - 1); }

                if (EqualsIgnoringAsciiCase(name, "latin"))
                {
                    codeSet = CharacterCodeSet::Latin;
                }
                else if (EqualsIgnoringAsciiCase(name, "jp"))
                {
                    codeSet = CharacterCodeSet::Japanese;
                }
                else
                {
                    codeSet = CharacterCodeSet::Unknown;
                }

                bytes = bytes.subspan(close + 1);
            }

            return bytes;
        }

        // Decodes a lyric in the code set its tag named. RP-026 says lyrics in a code set it
        // doesn't define are ignored until a defined one appears. Plain ASCII reads the same in
        // every code set, so those lyrics are kept, which also keeps the line breaks of a song;
        // anything else is dropped rather than shown as nonsense. Empty when dropped.
        std::string DecodeTaggedText(
            _In_ std::span<uint8_t const> bytes,
            _In_ CharacterCodeSet codeSet,
            _In_ bool preserveWhitespace) noexcept
        {
            try
            {
                std::string text{};

                switch (codeSet)
                {
                case CharacterCodeSet::Latin:
                    text = DecodeCodePage(AnsiCodePage, bytes);
                    break;

                case CharacterCodeSet::Japanese:
                    text = DecodeCodePage(ShiftJisCodePage, bytes.first(ShiftJisCompleteLength(bytes)));
                    break;

                default:
                    if (HasNonAsciiByte(bytes))
                    {
                        return {};
                    }

                    text.assign(reinterpret_cast<char const*>(bytes.data()), bytes.size());
                    break;
                }

                TidyText(text, preserveWhitespace);

                return text;
            }
            catch (...)
            {
                return {};
            }
        }

        enum class TextTarget : uint8_t
        {
            None,               // read for the encoding decision only
            TrackName,
            InstrumentName,
            DeviceName,
            Copyright,
            TextEvent,
            SongTitle,          // RP-026 song information
            SongComposer,
            SongLyricist,
            SongArtist,
        };

        struct RecordedText
        {
            std::span<uint8_t const> Bytes{};   // points into the file, which outlives the parse
            size_t TargetIndex{ 0 };            // the track, or the entry in TextEvents
            TextTarget Target{ TextTarget::None };
            bool Truncated{ false };
            bool PreserveWhitespace{ false };
            bool SameEventAsPrevious{ false };  // one event can fill two places; score it once
        };

        // Picks one text encoding for the whole file, in this order:
        //
        //  1. UTF-8, when every text that has a byte outside ASCII is valid UTF-8.
        //  2. Shift-JIS, GBK (Simplified Chinese), Big5 (Traditional Chinese) or Korean. Shift-JIS
        //     fits when every text is clean Shift-JIS AND at least one holds a character Latin-1
        //     text can't produce by accident (see TryDecodeShiftJis). A short Latin-1 name made
        //     only of capital accented letters, such as "ÀÉÎ", is also valid half-width katakana,
        //     so it stays Latin-1. The other three fit when every text is clean in them and they
        //     score at least MinimumDoubleByteScore (see TryDecodeDoubleByte), and Big5 also needs
        //     its evidence. They all overlap a lot: 千本桜 in GBK is also valid Shift-JIS, and the
        //     most common Big5 hanzi sit where GBK keeps its kana. When several fit, the highest
        //     score wins, and a tie goes to the first in that list.
        //  3. Otherwise each text keeps the decoding DecodeText gave it, which is how this reader
        //     has always behaved.
        //
        // RP-026 adds two ways for a file to say what it uses, and they come first:
        //
        //  - A text that starts with a Unicode byte order mark is decoded as Unicode while it is
        //    read, and never reaches this class.
        //  - A {@LATIN} or {@JP} tag in a lyric event sets the code set of the lyrics that follow
        //    it, until the next tag. Those lyrics are also decoded while they are read.
        //
        // Everything else, meaning names, the copyright, other text events and any lyrics before
        // the first tag, is decided here. RP-026 only defines the tag for lyrics, but a file that
        // declares its lyrics Japanese or ANSI almost always means the rest of its text too, so
        // the first defined tag in the file replaces step 2 above: {@JP} picks Shift-JIS whenever
        // every text decodes cleanly in it, without looking for further evidence, and {@LATIN}
        // picks Windows code page 1252. UTF-8 still wins when every text is valid UTF-8, and a
        // file with no tag is guessed as described above. RP-026's own default for a file with
        // no tag is ANSI, but the guess reads the same ANSI files the same way and also reads
        // the many UTF-8 and Japanese files that never carried a tag.
        //
        // Deciding per file matters because one Japanese name is often only a few bytes long,
        // and the copyright or another track name in the same file is what settles it.
        //
        // The model is filled in as the file is read, decoded the DecodeText way, and only
        // rewritten here when the file turns out to be Shift-JIS, GBK, Big5, Korean or tagged
        // ANSI. Whether a text decodes to nothing, and whether it starts with "@K", come out the
        // same in every encoding, so the choices made while reading still hold afterward.
        class TextEncodingDetector
        {
        public:
            CharacterCodeSet CurrentCodeSet() const noexcept { return m_codeSet; }

            void SetCurrentCodeSet(_In_ CharacterCodeSet codeSet) noexcept
            {
                m_codeSet = codeSet;

                if (m_firstCodeSet == CharacterCodeSet::None &&
                    (codeSet == CharacterCodeSet::Latin || codeSet == CharacterCodeSet::Japanese))
                {
                    m_firstCodeSet = codeSet;
                }
            }

            void Record(
                _In_ std::span<uint8_t const> bytes,
                _In_ bool truncated,
                _In_ bool preserveWhitespace,
                _In_ TextTarget target,
                _In_ size_t targetIndex) noexcept
            {
                // ASCII reads the same in every encoding this reader considers.
                if (m_abandoned || !HasNonAsciiByte(bytes))
                {
                    return;
                }

                if (target == TextTarget::None)
                {
                    if (m_evidenceOnlyCount >= MaximumEvidenceOnlyTexts)
                    {
                        return;
                    }

                    ++m_evidenceOnlyCount;
                }

                try
                {
                    auto const sameEventAsPrevious = !m_texts.empty() && m_texts.back().Bytes.data() == bytes.data();

                    m_texts.push_back(RecordedText{ bytes, targetIndex, target, truncated, preserveWhitespace, sameEventAsPrevious });
                }
                catch (...)
                {
                    // Out of memory. Without the full list, a file wide decision can't be made,
                    // so the texts keep the decoding they already have.
                    m_abandoned = true;
                    m_texts.clear();
                }
            }

            void SawByteOrderMark() noexcept { m_sawByteOrderMark = true; }

            // Decodes the recorded texts in the encoding the file uses, and records which one that
            // is and how it was chosen.
            void Apply(_Inout_ MidiSequence& sequence) noexcept
            {
                auto const guessed = Guess(sequence);

                TextEncodingInfo info{};

                if (m_firstCodeSet == CharacterCodeSet::Japanese)
                {
                    info = TextEncodingInfo{ TextEncoding::ShiftJis, TextEncodingSource::CodeSetTag };
                }
                else if (m_firstCodeSet == CharacterCodeSet::Latin)
                {
                    info = TextEncodingInfo{ TextEncoding::Windows1252, TextEncodingSource::CodeSetTag };
                }
                else if (guessed != TextEncoding::None)
                {
                    info = TextEncodingInfo{ guessed, TextEncodingSource::Detected };
                }
                else if (m_sawByteOrderMark)
                {
                    info = TextEncodingInfo{ TextEncoding::Unicode, TextEncodingSource::ByteOrderMark };
                }

                sequence.TextEncoding = info;
            }

        private:
            // Returns the encoding the untagged text was decoded with: None when it is all ASCII,
            // and Latin1 when nothing better fit, so each text kept its UTF-8 or Latin-1 reading.
            TextEncoding Guess(_Inout_ MidiSequence& sequence) noexcept
            {
                if (m_abandoned || m_texts.empty())
                {
                    return TextEncoding::None;
                }

                try
                {
                    auto const allUtf8 = std::all_of(m_texts.begin(), m_texts.end(), [](RecordedText const& text) noexcept
                        {
                            size_t usable{};
                            return TryMeasureUtf8(text.Bytes, text.Truncated, usable);
                        });

                    if (allUtf8)
                    {
                        return TextEncoding::Utf8;
                    }

                    if (m_firstCodeSet == CharacterCodeSet::Latin)
                    {
                        std::vector<std::string> ansi(m_texts.size());

                        for (size_t index = 0; index < m_texts.size(); ++index)
                        {
                            ansi[index] = DecodeCodePage(AnsiCodePage, m_texts[index].Bytes);
                            TidyText(ansi[index], m_texts[index].PreserveWhitespace);
                        }

                        for (size_t index = 0; index < m_texts.size(); ++index)
                        {
                            auto const destination = Destination(sequence, m_texts[index]);

                            if (destination != nullptr && !ansi[index].empty())
                            {
                                *destination = std::move(ansi[index]);
                            }
                        }

                        return TextEncoding::Windows1252;
                    }

                    // Every text is decoded before any is written back, so the model never ends up
                    // with some texts in one encoding and the rest in another.
                    auto const japaneseTagged = m_firstCodeSet == CharacterCodeSet::Japanese;

                    // In the order that wins a tie.
                    enum CandidateIndex : size_t { ShiftJis, Gbk, Big5, Korean, CandidateCount };

                    struct Candidate
                    {
                        std::vector<std::string> Decoded{};
                        DoubleByteReading Reading{};
                        bool Clean{ true };
                    };

                    std::array<Candidate, CandidateCount> candidates{};
                    bool sawJapanese = japaneseTagged;

                    for (auto& candidate : candidates)
                    {
                        candidate.Decoded.resize(m_texts.size());
                    }

                    // A {@JP} tag leaves only Shift-JIS to check.
                    if (japaneseTagged)
                    {
                        candidates[Gbk].Clean = false;
                        candidates[Big5].Clean = false;
                        candidates[Korean].Clean = false;
                    }

                    for (size_t index = 0; index < m_texts.size(); ++index)
                    {
                        auto const& text = m_texts[index];
                        DoubleByteReading ignored{};

                        auto readingFor = [&](Candidate& candidate) noexcept -> DoubleByteReading&
                            {
                                return text.SameEventAsPrevious ? ignored : candidate.Reading;
                            };

                        auto decode = [&](CandidateIndex which, UINT codePage, auto const& valueOf, bool scoreKanaRuns)
                            {
                                auto& candidate = candidates[which];

                                if (candidate.Clean)
                                {
                                    candidate.Clean = TryDecodeDoubleByte(
                                        codePage, text.Bytes, text.Truncated, valueOf, scoreKanaRuns,
                                        candidate.Decoded[index], readingFor(candidate));
                                    TidyText(candidate.Decoded[index], text.PreserveWhitespace);
                                }
                            };

                        if (candidates[ShiftJis].Clean)
                        {
                            candidates[ShiftJis].Clean = TryDecodeShiftJis(
                                text.Bytes, text.Truncated, candidates[ShiftJis].Decoded[index], sawJapanese,
                                readingFor(candidates[ShiftJis]).Score);
                            TidyText(candidates[ShiftJis].Decoded[index], text.PreserveWhitespace);
                        }

                        decode(Gbk, GbkCodePage, GbkValue, true);
                        decode(Big5, Big5CodePage, Big5Value, false);
                        decode(Korean, KoreanCodePage, KoreanValue, false);

                        if (std::none_of(candidates.begin(), candidates.end(), [](Candidate const& candidate) noexcept { return candidate.Clean; }))
                        {
                            return TextEncoding::Latin1;
                        }
                    }

                    std::array<bool, CandidateCount> const fits
                    {
                        candidates[ShiftJis].Clean && sawJapanese,
                        candidates[Gbk].Clean && candidates[Gbk].Reading.Score >= MinimumDoubleByteScore,
                        candidates[Big5].Clean && candidates[Big5].Reading.Score >= MinimumDoubleByteScore && candidates[Big5].Reading.Evidence,
                        candidates[Korean].Clean && candidates[Korean].Reading.Score >= MinimumDoubleByteScore,
                    };

                    // Several can read cleanly, because their byte ranges overlap. The one that reads
                    // most like real text wins.
                    std::vector<std::string>* chosen{ nullptr };
                    auto chosenEncoding = TextEncoding::Latin1;
                    int bestScore = 0;

                    constexpr std::array<TextEncoding, CandidateCount> EncodingOf
                    {
                        TextEncoding::ShiftJis, TextEncoding::Gbk, TextEncoding::Big5, TextEncoding::Korean,
                    };

                    for (size_t which = 0; which < CandidateCount; ++which)
                    {
                        if (fits[which] && (chosen == nullptr || candidates[which].Reading.Score > bestScore))
                        {
                            chosen = &candidates[which].Decoded;
                            chosenEncoding = EncodingOf[which];
                            bestScore = candidates[which].Reading.Score;
                        }
                    }

                    if (chosen == nullptr)
                    {
                        return TextEncoding::Latin1;
                    }

                    for (size_t index = 0; index < m_texts.size(); ++index)
                    {
                        auto& decoded = (*chosen)[index];

                        // Only a dropped cut lead byte can make this empty; keep what was read.
                        if (decoded.empty())
                        {
                            continue;
                        }

                        auto const destination = Destination(sequence, m_texts[index]);

                        if (destination != nullptr)
                        {
                            *destination = std::move(decoded);
                        }
                    }

                    return chosenEncoding;
                }
                catch (...)
                {
                    // Out of memory part way through the decision. Nothing has been written back
                    // yet, so every text keeps the decoding it was read with.
                    return TextEncoding::Latin1;
                }
            }

            // Enough texts to settle the encoding of any real file, without letting a file made
            // of nothing but comments grow this list without limit.
            static constexpr size_t MaximumEvidenceOnlyTexts = 4096;

            // GBK, Big5 and Korean have no rule like the Shift-JIS one about control code lead
            // bytes, so each needs a few characters that read like real text before it is
            // believed. Two is one kana or common character, or two plain hanzi or Hangul.
            static constexpr int MinimumDoubleByteScore = 2;

            static std::string* Destination(_Inout_ MidiSequence& sequence, _In_ RecordedText const& text) noexcept
            {
                switch (text.Target)
                {
                case TextTarget::TrackName:
                    return text.TargetIndex < sequence.Tracks.size() ? &sequence.Tracks[text.TargetIndex].Name : nullptr;

                case TextTarget::InstrumentName:
                    return text.TargetIndex < sequence.Tracks.size() ? &sequence.Tracks[text.TargetIndex].InstrumentName : nullptr;

                case TextTarget::DeviceName:
                    return text.TargetIndex < sequence.Tracks.size() ? &sequence.Tracks[text.TargetIndex].DeviceName : nullptr;

                case TextTarget::Copyright:
                    return &sequence.Copyright;

                case TextTarget::TextEvent:
                    return text.TargetIndex < sequence.TextEvents.size() ? &sequence.TextEvents[text.TargetIndex].Text : nullptr;

                case TextTarget::SongTitle:
                    return &sequence.SongInfo.Title;

                case TextTarget::SongComposer:
                    return &sequence.SongInfo.Composer;

                case TextTarget::SongLyricist:
                    return &sequence.SongInfo.Lyricist;

                case TextTarget::SongArtist:
                    return &sequence.SongInfo.Artist;

                default:
                    return nullptr;
                }
            }

            std::vector<RecordedText> m_texts{};
            size_t m_evidenceOnlyCount{ 0 };
            CharacterCodeSet m_codeSet{ CharacterCodeSet::None };
            CharacterCodeSet m_firstCodeSet{ CharacterCodeSet::None };   // first LATIN or JP tag
            bool m_sawByteOrderMark{ false };
            bool m_abandoned{ false };
        };

        // Decodes one piece of event text the way the file says to, or, when it says nothing,
        // the DecodeText way and leaves it to the file wide guess. decided is false in that case,
        // and the caller has to Record the text with the detector.
        std::string DecodeEventText(
            _In_ std::span<uint8_t const> bytes,
            _In_ CharacterCodeSet codeSet,
            _In_ bool preserveWhitespace,
            _In_ bool truncated,
            _Inout_ TextEncodingDetector& encodingDetector,
            _Out_ bool& decided) noexcept
        {
            decided = true;

            if (auto const mark = ReadByteOrderMark(bytes); mark != ByteOrderMark::None)
            {
                encodingDetector.SawByteOrderMark();
                return DecodeUnicodeText(bytes, mark, preserveWhitespace);
            }

            if (codeSet != CharacterCodeSet::None)
            {
                return DecodeTaggedText(bytes, codeSet, preserveWhitespace);
            }

            decided = false;
            return DecodeText(bytes, preserveWhitespace, truncated);
        }

        std::span<uint8_t const> TrimSpaces(_In_ std::span<uint8_t const> bytes) noexcept
        {
            while (!bytes.empty() && bytes.front() == ' ') { bytes = bytes.subspan(1); }
            while (!bytes.empty() && bytes.back() == ' ') { bytes = bytes.first(bytes.size() - 1); }

            return bytes;
        }

        // Reads the RP-026 song information tags at the start of a lyric event, such as
        // {#Title=Beautiful Song}{#Composer=Tom Smith}{#}, into sequence.SongInfo, and returns what
        // follows them. The item names are TITLE, COMPOSER, LYRICS and ARTIST in any case; others
        // are skipped, and {#} only marks the end of a group. The first value of each item wins.
        //
        // RP-026 warns that the closing brace can't always be found in a multi-byte code set,
        // because 0x7D can also be the second byte of a character, so the start of the next item
        // ends an item too. Here an item runs to the next "{#", or the end of the event, and ends
        // at the last closing brace before that.
        std::span<uint8_t const> ReadSongInformationTags(
            _Inout_ MidiSequence& sequence,
            _In_ std::span<uint8_t const> bytes,
            _In_ CharacterCodeSet codeSet,
            _In_ bool truncated,
            _Inout_ TextEncodingDetector& encodingDetector) noexcept
        {
            auto const startsWithTag = [](std::span<uint8_t const> rest) noexcept
                {
                    rest = TrimSpaces(rest);
                    return rest.size() >= 2 && rest[0] == '{' && rest[1] == '#';
                };

            bool sawTag = false;

            while (startsWithTag(bytes))
            {
                // RP-026's own example puts a space between tags.
                while (bytes.front() == ' ') { bytes = bytes.subspan(1); }

                sawTag = true;

                size_t next = 2;

                while (next + 1 < bytes.size() && !(bytes[next] == '{' && bytes[next + 1] == '#'))
                {
                    ++next;
                }

                if (next + 1 >= bytes.size())
                {
                    next = bytes.size();
                }

                size_t close = next;

                while (close > 2 && bytes[close - 1] != '}')
                {
                    --close;
                }

                auto const closed = close > 2;
                auto const body = bytes.subspan(2, (closed ? close - 1 : next) - 2);

                bytes = bytes.subspan(closed ? close : next);

                size_t equals = 0;

                while (equals < body.size() && body[equals] != '=')
                {
                    ++equals;
                }

                if (equals >= body.size())
                {
                    continue;
                }

                auto const name = TrimSpaces(body.first(equals));
                auto const value = TrimSpaces(body.subspan(equals + 1));

                std::string* destination{ nullptr };
                auto target = TextTarget::None;

                if (EqualsIgnoringAsciiCase(name, "title")) { destination = &sequence.SongInfo.Title; target = TextTarget::SongTitle; }
                else if (EqualsIgnoringAsciiCase(name, "composer")) { destination = &sequence.SongInfo.Composer; target = TextTarget::SongComposer; }
                else if (EqualsIgnoringAsciiCase(name, "lyrics")) { destination = &sequence.SongInfo.Lyricist; target = TextTarget::SongLyricist; }
                else if (EqualsIgnoringAsciiCase(name, "artist")) { destination = &sequence.SongInfo.Artist; target = TextTarget::SongArtist; }

                if (destination == nullptr || !destination->empty() || value.empty())
                {
                    continue;
                }

                bool decided{};
                auto const valueTruncated = truncated && !closed && bytes.empty();

                *destination = DecodeEventText(value, codeSet, false, valueTruncated, encodingDetector, decided);

                if (!decided && !destination->empty())
                {
                    encodingDetector.Record(value, valueTruncated, false, target, 0);
                }
            }

            // Spaces left over after the last tag are not a lyric.
            if (sawTag && TrimSpaces(bytes).empty())
            {
                return {};
            }

            return bytes;
        }

        EventKind KindFromStatus(uint8_t status) noexcept
        {
            if (status >= 0xF8)
            {
                return EventKind::SystemRealTime;
            }

            switch (status & 0xF0)
            {
            case 0x80: return EventKind::NoteOff;
            case 0x90: return EventKind::NoteOn;
            case 0xA0: return EventKind::PolyphonicPressure;
            case 0xB0: return EventKind::ControlChange;
            case 0xC0: return EventKind::ProgramChange;
            case 0xD0: return EventKind::ChannelPressure;
            case 0xE0: return EventKind::PitchBend;
            default:   return EventKind::SystemCommon;
            }
        }

        // How many data bytes follow a status byte inside a track. Getting this wrong for the
        // system statuses desynchronizes the rest of the track, because the reader would swallow
        // bytes belonging to the next delta time.
        uint32_t DataByteCountForStatus(uint8_t status) noexcept
        {
            if (status < 0xF0)
            {
                auto const highNibble = static_cast<uint8_t>(status & 0xF0);

                return (highNibble == 0xC0 || highNibble == 0xD0) ? 1u : 2u;
            }

            switch (status)
            {
            case 0xF1: return 1u;   // MIDI time code quarter frame
            case 0xF2: return 2u;   // song position pointer
            case 0xF3: return 1u;   // song select
            default:   return 0u;   // tune request, the undefined statuses and all of real time
            }
        }

        // Empty when the dump is not a chord symbol.
        std::string ReadChordSymbol(std::span<uint8_t const> message) noexcept
        {
            if (message.size() <= sizeof(ChordSymbolPrefix) + 1)
            {
                return {};
            }

            for (size_t index = 0; index < sizeof(ChordSymbolPrefix); ++index)
            {
                if (message[index] != ChordSymbolPrefix[index])
                {
                    return {};
                }
            }

            std::string name{};

            for (size_t index = sizeof(ChordSymbolPrefix); index < message.size(); ++index)
            {
                auto const byte = message[index];

                // The name ends at the line feed the format uses as a terminator, or at the end
                // of the dump.
                if (byte == 0x0A || byte == SystemExclusiveEndStatus)
                {
                    break;
                }

                // Anything outside printable ASCII means this is not the message it looked like.
                if (byte < 0x20 || byte > 0x7E || name.size() >= MaximumChordSymbolLength)
                {
                    return {};
                }

                name.push_back(static_cast<char>(byte));
            }

            return name;
        }

        uint32_t AddTicks(uint32_t tick, uint32_t delta) noexcept
        {
            if (delta > MaximumTick - tick)
            {
                return MaximumTick;
            }

            return tick + delta;
        }

        struct TrackParseState
        {
            uint32_t Tick{ 0 };
            uint8_t RunningStatus{ 0 };
            std::array<uint8_t, 16> BankMsb{};
            std::array<uint8_t, 16> BankLsb{};
            std::vector<uint8_t> PendingSystemExclusive{};
        };

        class SequenceWriter
        {
        public:
            SequenceWriter(MidiSequence& sequence, ReadLimits const& limits) noexcept :
                m_sequence(sequence), m_limits(limits)
            {
            }

            bool AtCapacity() const noexcept { return m_atCapacity; }

            bool Append(
                uint32_t tick,
                uint16_t trackIndex,
                EventKind kind,
                uint8_t channel,
                std::span<uint8_t const> bytes) noexcept
            {
                if (m_atCapacity || bytes.empty())
                {
                    return !m_atCapacity;
                }

                if (m_sequence.Events.size() >= m_limits.MaximumEvents ||
                    m_sequence.EventBytes.size() + bytes.size() > m_limits.MaximumEventBytes)
                {
                    m_atCapacity = true;
                    return false;
                }

                SequenceEvent event{};

                event.Tick = tick;
                event.ByteOffset = static_cast<uint32_t>(m_sequence.EventBytes.size());
                event.ByteCount = static_cast<uint32_t>(bytes.size());
                event.TrackIndex = trackIndex;
                event.Kind = kind;
                event.Channel = channel;

                m_sequence.EventBytes.insert(m_sequence.EventBytes.end(), bytes.begin(), bytes.end());
                m_sequence.Events.push_back(event);

                return true;
            }

        private:
            MidiSequence& m_sequence;
            ReadLimits const& m_limits;
            bool m_atCapacity{ false };
        };

        void HandleMetaEvent(
            MidiSequence& sequence,
            ReadLimits const& limits,
            uint16_t trackIndex,
            uint32_t tick,
            uint8_t metaType,
            std::span<uint8_t const> data,
            _Inout_ TextEncodingDetector& encodingDetector) noexcept
        {
            if (metaType >= 0x01 && metaType <= 0x0F)
            {
                auto const truncated = data.size() > limits.MaximumTextBytes;

                if (truncated)
                {
                    data = data.subspan(0, limits.MaximumTextBytes);
                }

                auto const kind = metaType <= 0x09
                    ? static_cast<TextKind>(metaType)
                    : TextKind::Text;

                // A karaoke file carries its words in plain text events, so both kinds keep their
                // spacing; everything else is a name or a note to a reader and is tidied up.
                auto const preserveWhitespace = kind == TextKind::Lyric || kind == TextKind::Text;

                // RP-026 code set and song information tags travel in lyric events. They are
                // instructions, not words to sing, so they are taken off the front of the lyric.
                auto codeSet = CharacterCodeSet::None;

                if (kind == TextKind::Lyric)
                {
                    codeSet = encodingDetector.CurrentCodeSet();

                    for (;;)
                    {
                        auto const before = data.size();

                        data = ReadCodeSetTags(data, codeSet);
                        encodingDetector.SetCurrentCodeSet(codeSet);

                        data = ReadSongInformationTags(sequence, data, codeSet, truncated, encodingDetector);

                        if (data.size() == before)
                        {
                            break;
                        }
                    }
                }

                // A text the file itself says how to read is decoded here and left out of the
                // file wide guess.
                bool decided{};
                auto text = DecodeEventText(data, codeSet, preserveWhitespace, truncated, encodingDetector, decided);

                if (text.empty())
                {
                    return;
                }

                bool recorded = false;

                auto record = [&](TextTarget target, size_t targetIndex) noexcept
                    {
                        if (!decided)
                        {
                            encodingDetector.Record(data, truncated, preserveWhitespace, target, targetIndex);
                        }

                        recorded = true;
                    };

                if (trackIndex < sequence.Tracks.size())
                {
                    auto& track = sequence.Tracks[trackIndex];

                    switch (kind)
                    {
                    case TextKind::TrackName:
                        if (track.Name.empty()) { track.Name = text; record(TextTarget::TrackName, trackIndex); }
                        break;

                    case TextKind::InstrumentName:
                        if (track.InstrumentName.empty()) { track.InstrumentName = text; record(TextTarget::InstrumentName, trackIndex); }
                        break;

                    case TextKind::DeviceName:
                        if (track.DeviceName.empty()) { track.DeviceName = text; record(TextTarget::DeviceName, trackIndex); }
                        break;

                    default:
                        break;
                    }
                }

                if (kind == TextKind::Copyright && sequence.Copyright.empty())
                {
                    sequence.Copyright = text;
                    record(TextTarget::Copyright, 0);
                }

                // A Soft Karaoke file announces itself before its first word, so from here on its
                // plain text events are lyrics and have to be kept.
                if (kind == TextKind::Text && !sequence.IsKaraoke && text.rfind("@K", 0) == 0)
                {
                    sequence.IsKaraoke = true;
                }

                // Only the kinds a display uses are kept, so a file full of editor comments does
                // not grow the model for nothing.
                if (kind == TextKind::Lyric || kind == TextKind::Marker || kind == TextKind::CuePoint ||
                    kind == TextKind::TrackName || kind == TextKind::ProgramName ||
                    (kind == TextKind::Text && sequence.IsKaraoke))
                {
                    if (sequence.TextEvents.size() < limits.MaximumTextEvents)
                    {
                        sequence.TextEvents.push_back(TextEvent{ tick, trackIndex, kind, std::move(text) });
                        record(TextTarget::TextEvent, sequence.TextEvents.size() - 1);
                    }
                }

                // Texts the model doesn't keep still help decide the file's encoding.
                if (!recorded)
                {
                    record(TextTarget::None, 0);
                }

                return;
            }

            switch (metaType)
            {
            case MetaSetTempo:
                if (data.size() >= 3)
                {
                    TempoChange change{};

                    change.Tick = tick;
                    change.MicrosecondsPerQuarterNote =
                        (static_cast<uint32_t>(data[0]) << 16) |
                        (static_cast<uint32_t>(data[1]) << 8) |
                        static_cast<uint32_t>(data[2]);

                    if (change.MicrosecondsPerQuarterNote < MinimumMicrosecondsPerQuarterNote)
                    {
                        change.MicrosecondsPerQuarterNote = MinimumMicrosecondsPerQuarterNote;
                    }

                    sequence.TempoMap.push_back(change);
                }
                break;

            case MetaTimeSignature:
                if (data.size() >= 4)
                {
                    TimeSignatureChange change{};

                    change.Tick = tick;
                    change.Numerator = data[0] == 0 ? uint8_t{ 4 } : data[0];
                    change.DenominatorPowerOfTwo = data[1];
                    change.ClocksPerClick = data[2];
                    change.ThirtySecondNotesPerQuarter = data[3];

                    sequence.TimeSignatureMap.push_back(change);
                }
                break;

            case MetaKeySignature:
                if (data.size() >= 2)
                {
                    KeySignatureChange change{};

                    change.Tick = tick;
                    change.Accidentals = static_cast<int8_t>(data[0]);
                    change.IsMinor = data[1] != 0;

                    sequence.KeySignatureMap.push_back(change);
                }
                break;

            default:
                break;
            }
        }

        // Returns false when the track ran out of bytes part way through an event.
        bool ParseTrack(
            ByteReader& reader,
            size_t trackEndOffset,
            MidiSequence& sequence,
            SequenceWriter& writer,
            ReadLimits const& limits,
            uint16_t trackIndex,
            uint32_t tickOffset,
            uint32_t& lastTick,
            _Inout_ TextEncodingDetector& encodingDetector) noexcept
        {
            TrackParseState state{};

            // A gap this long is never a musical rest, so it is used as the signal that the reader
            // has lost the event boundary. Generous on purpose: at a musical division this is about
            // twenty five minutes of silence at 120 beats per minute, and slower tempos stretch it
            // further still, so a real file cannot trip it.
            auto const maximumDelta = sequence.Division.IsSmpte
                ? static_cast<uint32_t>(sequence.Division.TicksPerSecond() * 1500.0)
                : static_cast<uint32_t>(sequence.Division.TicksPerQuarterNote) * 3000u;

            state.Tick = tickOffset;
            lastTick = tickOffset;

            while (reader.Offset() < trackEndOffset && !writer.AtCapacity())
            {
                uint32_t delta{};

                if (!reader.TryReadVariableLength(delta))
                {
                    return false;
                }

                if (delta > maximumDelta)
                {
                    // Nothing musical waits this long. The reader has lost the event boundary, and
                    // carrying on would invent a timeline hours or days long out of whatever bytes
                    // follow, so keep what is real and stop here.
                    return false;
                }

                state.Tick = AddTicks(state.Tick, delta);
                lastTick = state.Tick;

                uint8_t leadByte{};

                if (!reader.TryPeek(leadByte))
                {
                    return false;
                }

                if (leadByte == MetaEventStatus)
                {
                    state.RunningStatus = 0;

                    uint8_t metaType{};
                    uint32_t length{};

                    if (!reader.TryReadByte(leadByte) || !reader.TryReadByte(metaType) ||
                        !reader.TryReadVariableLength(length))
                    {
                        return false;
                    }

                    std::span<uint8_t const> data{};

                    if (!reader.TryReadSpan(length, data))
                    {
                        return false;
                    }

                    if (metaType == MetaEndOfTrack)
                    {
                        return true;
                    }

                    HandleMetaEvent(sequence, limits, trackIndex, state.Tick, metaType, data, encodingDetector);
                    continue;
                }

                if (leadByte == SystemExclusiveStatus || leadByte == SystemExclusiveEndStatus)
                {
                    state.RunningStatus = 0;

                    uint32_t length{};

                    if (!reader.TryReadByte(leadByte) || !reader.TryReadVariableLength(length))
                    {
                        return false;
                    }

                    if (length > limits.MaximumSingleMessageBytes)
                    {
                        // Not plausible as a real dump. Skip it rather than carry it around.
                        if (!reader.TrySkip(length))
                        {
                            return false;
                        }

                        state.PendingSystemExclusive.clear();
                        continue;
                    }

                    std::span<uint8_t const> data{};

                    if (!reader.TryReadSpan(length, data))
                    {
                        return false;
                    }

                    if (leadByte == SystemExclusiveStatus)
                    {
                        state.PendingSystemExclusive.clear();
                        state.PendingSystemExclusive.push_back(SystemExclusiveStatus);
                        state.PendingSystemExclusive.insert(
                            state.PendingSystemExclusive.end(), data.begin(), data.end());

                        // Read before the terminator check below, because these are written both
                        // with and without a closing F7 and the symbol is worth having either way.
                        auto chord = ReadChordSymbol(std::span<uint8_t const>{ state.PendingSystemExclusive });

                        if (!chord.empty() && sequence.TextEvents.size() < limits.MaximumTextEvents)
                        {
                            sequence.TextEvents.push_back(
                                TextEvent{ state.Tick, trackIndex, TextKind::ChordSymbol, std::move(chord) });
                        }
                    }
                    else if (!state.PendingSystemExclusive.empty())
                    {
                        // A continuation of a dump that was split across several events.
                        state.PendingSystemExclusive.insert(
                            state.PendingSystemExclusive.end(), data.begin(), data.end());
                    }
                    else
                    {
                        // An escape carrying raw bytes. Only a single short system message is
                        // worth passing on; anything else would be guesswork on the wire.
                        if (data.size() >= 1 && data.size() <= 3 && data[0] >= 0xF0)
                        {
                            auto const kind = data[0] >= 0xF8 ? EventKind::SystemRealTime : EventKind::SystemCommon;
                            writer.Append(state.Tick, trackIndex, kind, ChannelNone, data);
                        }

                        continue;
                    }

                    if (!state.PendingSystemExclusive.empty() &&
                        state.PendingSystemExclusive.back() == SystemExclusiveEndStatus)
                    {
                        writer.Append(
                            state.Tick,
                            trackIndex,
                            EventKind::SystemExclusive,
                            ChannelNone,
                            std::span<uint8_t const>{ state.PendingSystemExclusive });

                        state.PendingSystemExclusive.clear();
                    }

                    continue;
                }

                uint8_t status{};

                if (leadByte >= 0x80)
                {
                    if (!reader.TryReadByte(status))
                    {
                        return false;
                    }

                    // Real time may be interleaved anywhere and leaves running status alone;
                    // system common cancels it; only a channel status becomes the new one.
                    if (status < 0xF0)
                    {
                        state.RunningStatus = status;
                    }
                    else if (status < 0xF8)
                    {
                        state.RunningStatus = 0;
                    }
                }
                else
                {
                    status = state.RunningStatus;

                    if (status < 0x80)
                    {
                        // A data byte with no status in front of it. Nothing after this point in
                        // the track can be interpreted, so keep what was read and stop.
                        return false;
                    }
                }

                auto const dataByteCount = DataByteCountForStatus(status);

                std::array<uint8_t, 3> message{};

                message[0] = status;

                for (uint32_t index = 0; index < dataByteCount; ++index)
                {
                    uint8_t value{};

                    if (!reader.TryReadByte(value))
                    {
                        return false;
                    }

                    // Out of range, but files written by buggy tools do this consistently while
                    // staying perfectly aligned, so the value is clamped rather than distrusted.
                    message[index + 1] = static_cast<uint8_t>(value & 0x7F);
                }

                auto const kind = KindFromStatus(status);
                auto const channel = status < 0xF0 ? static_cast<uint8_t>(status & 0x0F) : ChannelNone;

                if (kind == EventKind::ControlChange)
                {
                    if (message[1] == ControllerBankSelectMsb) { state.BankMsb[channel] = message[2]; }
                    else if (message[1] == ControllerBankSelectLsb) { state.BankLsb[channel] = message[2]; }
                }
                else if (kind == EventKind::ProgramChange)
                {
                    ProgramChangeEvent change{};

                    change.Tick = state.Tick;
                    change.TrackIndex = trackIndex;
                    change.Channel = channel;
                    change.Program = message[1];
                    change.BankMsb = state.BankMsb[channel];
                    change.BankLsb = state.BankLsb[channel];

                    sequence.ProgramChanges.push_back(change);
                }

                writer.Append(
                    state.Tick,
                    trackIndex,
                    kind,
                    channel,
                    std::span<uint8_t const>{ message.data(), dataByteCount + 1 });
            }

            return true;
        }

        // .rmi files wrap a Standard MIDI File in a RIFF container. Unwrapping it here means the
        // rest of the reader never has to know.
        std::span<uint8_t const> UnwrapRiffMidi(std::span<uint8_t const> fileBytes) noexcept
        {
            if (fileBytes.size() < 20 || !ChunkIdIs(fileBytes, 0, "RIFF") || !ChunkIdIs(fileBytes, 8, "RMID"))
            {
                return fileBytes;
            }

            size_t offset = 12;

            while (offset + 8 <= fileBytes.size())
            {
                uint32_t const length =
                    static_cast<uint32_t>(fileBytes[offset + 4]) |
                    (static_cast<uint32_t>(fileBytes[offset + 5]) << 8) |
                    (static_cast<uint32_t>(fileBytes[offset + 6]) << 16) |
                    (static_cast<uint32_t>(fileBytes[offset + 7]) << 24);

                auto const payload = offset + 8;
                auto const available = fileBytes.size() - payload;
                auto const usable = static_cast<size_t>(length) > available ? available : static_cast<size_t>(length);

                if (ChunkIdIs(fileBytes, offset, "data"))
                {
                    return fileBytes.subspan(payload, usable);
                }

                // RIFF chunks are word aligned.
                offset = payload + usable + (usable & 1);
            }

            return fileBytes;
        }
    }

    _Use_decl_annotations_
    ReadResult ParseStandardMidiFile(
        std::span<uint8_t const> fileBytes,
        MidiSequence& sequence,
        ReadLimits const& limits) noexcept
    {
        ReadResult result{};

        sequence.Clear();

        try
        {
            auto const bytes = UnwrapRiffMidi(fileBytes);

            ByteReader reader{ bytes };

            if (bytes.size() < 14 || !ChunkIdIs(bytes, 0, "MThd"))
            {
                result.Status = ReadStatus::NotAMidiFile;
                return result;
            }

            reader.TrySkip(4);

            uint32_t headerLength{};
            uint16_t format{};
            uint16_t trackCount{};
            uint16_t division{};

            if (!reader.TryReadUInt32(headerLength) || headerLength < 6 ||
                !reader.TryReadUInt16(format) || !reader.TryReadUInt16(trackCount) ||
                !reader.TryReadUInt16(division))
            {
                result.Status = ReadStatus::CorruptHeader;
                result.ByteOffset = reader.Offset();
                return result;
            }

            // A header longer than six bytes is legal; the extra is reserved.
            if (headerLength > 6)
            {
                reader.TrySkip(headerLength - 6);
            }

            sequence.Format = format > 2
                ? SequenceFormat::MultiTrack
                : static_cast<SequenceFormat>(format);

            if ((division & 0x8000) != 0)
            {
                sequence.Division.IsSmpte = true;
                sequence.Division.FramesPerSecond = static_cast<uint8_t>(-static_cast<int8_t>(division >> 8));
                sequence.Division.TicksPerFrame = static_cast<uint8_t>(division & 0xFF);

                if (sequence.Division.FramesPerSecond == 0 || sequence.Division.TicksPerFrame == 0)
                {
                    sequence.Division = TimeDivision{};
                }
            }
            else
            {
                sequence.Division.IsSmpte = false;
                sequence.Division.TicksPerQuarterNote = (division & 0x7FFF) == 0
                    ? DefaultTicksPerQuarterNote
                    : static_cast<uint16_t>(division & 0x7FFF);
            }

            result.TracksDeclared = trackCount;

            auto const tracksToRead = trackCount > limits.MaximumTracks
                ? limits.MaximumTracks
                : static_cast<uint32_t>(trackCount);

            SequenceWriter writer{ sequence, limits };
            TextEncodingDetector encodingDetector{};

            // Independent sequences are laid end to end rather than played on top of each other.
            uint32_t sequentialOffset = 0;

            while (sequence.Tracks.size() < tracksToRead && !reader.AtEnd())
            {
                if (reader.Remaining() < 8)
                {
                    result.Truncated = true;
                    break;
                }

                auto const chunkStart = reader.Offset();
                auto const isTrack = ChunkIdIs(bytes, chunkStart, "MTrk");

                reader.TrySkip(4);

                uint32_t chunkLength{};

                if (!reader.TryReadUInt32(chunkLength))
                {
                    result.Truncated = true;
                    break;
                }

                auto const payloadStart = reader.Offset();
                auto const available = reader.Remaining();

                if (static_cast<size_t>(chunkLength) > available)
                {
                    // The declared length runs past the end of the file. Play what is there.
                    chunkLength = static_cast<uint32_t>(available);
                    result.Truncated = true;
                }

                if (!isTrack)
                {
                    // An unknown chunk between tracks is legal and must be skipped.
                    reader.TrySkip(chunkLength);
                    continue;
                }

                auto const trackIndex = static_cast<uint16_t>(sequence.Tracks.size());

                sequence.Tracks.push_back(Track{});

                uint32_t trackLastTick = sequentialOffset;

                if (!ParseTrack(
                    reader,
                    payloadStart + chunkLength,
                    sequence,
                    writer,
                    limits,
                    trackIndex,
                    sequence.Format == SequenceFormat::MultiSequence ? sequentialOffset : 0,
                    trackLastTick,
                    encodingDetector))
                {
                    result.Truncated = true;
                }

                if (sequence.Format == SequenceFormat::MultiSequence)
                {
                    sequentialOffset = trackLastTick;
                }

                ++result.TracksRead;

                if (writer.AtCapacity())
                {
                    result.Truncated = true;
                    break;
                }

                reader.SeekTo(payloadStart + chunkLength);
            }

            // Before Finalize, which sorts the text events and builds lyric lines from them.
            encodingDetector.Apply(sequence);

            if (sequence.Events.empty())
            {
                result.Status = ReadStatus::NoPlayableData;
                result.ByteOffset = reader.Offset();
                return result;
            }

            sequence.Finalize();

            return result;
        }
        catch (...)
        {
            // Only an allocation failure can get here; everything else is checked.
            sequence.Clear();

            result.Status = ReadStatus::TooMuchData;
            return result;
        }
    }

    _Use_decl_annotations_
    ReadResult ReadStandardMidiFile(
        std::wstring const& path,
        MidiSequence& sequence,
        ReadLimits const& limits) noexcept
    {
        ReadResult result{};

        sequence.Clear();

        HANDLE const file = ::CreateFileW(
            path.c_str(),
            GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
            nullptr);

        if (file == INVALID_HANDLE_VALUE)
        {
            auto const error = ::GetLastError();

            result.Status = (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND)
                ? ReadStatus::FileNotFound
                : (error == ERROR_ACCESS_DENIED ? ReadStatus::AccessDenied : ReadStatus::ReadFailed);

            return result;
        }

        LARGE_INTEGER size{};

        if (!::GetFileSizeEx(file, &size))
        {
            ::CloseHandle(file);

            result.Status = ReadStatus::ReadFailed;
            return result;
        }

        if (size.QuadPart <= 0)
        {
            ::CloseHandle(file);

            result.Status = ReadStatus::NotAMidiFile;
            return result;
        }

        if (static_cast<uint64_t>(size.QuadPart) > limits.MaximumFileBytes)
        {
            ::CloseHandle(file);

            result.Status = ReadStatus::FileTooLarge;
            return result;
        }

        std::vector<uint8_t> buffer{};

        try
        {
            buffer.resize(static_cast<size_t>(size.QuadPart));
        }
        catch (...)
        {
            ::CloseHandle(file);

            result.Status = ReadStatus::TooMuchData;
            return result;
        }

        size_t totalRead = 0;

        while (totalRead < buffer.size())
        {
            auto const remaining = buffer.size() - totalRead;
            auto const chunk = remaining > 0x10000000u ? 0x10000000u : static_cast<DWORD>(remaining);

            DWORD read = 0;

            if (!::ReadFile(file, buffer.data() + totalRead, chunk, &read, nullptr) || read == 0)
            {
                break;
            }

            totalRead += read;
        }

        ::CloseHandle(file);

        if (totalRead == 0)
        {
            result.Status = ReadStatus::ReadFailed;
            return result;
        }

        buffer.resize(totalRead);

        return ParseStandardMidiFile(std::span<uint8_t const>{ buffer }, sequence, limits);
    }

    _Use_decl_annotations_
    bool IsStandardMidiFileExtension(std::wstring const& path) noexcept
    {
        auto const dot = path.find_last_of(L'.');

        if (dot == std::wstring::npos)
        {
            return false;
        }

        std::wstring extension = path.substr(dot);

        std::transform(extension.begin(), extension.end(), extension.begin(),
            [](wchar_t value) noexcept { return static_cast<wchar_t>(::towlower(value)); });

        return extension == L".mid"
            || extension == L".midi"
            || extension == L".smf"
            || extension == L".rmi"
            || extension == L".kar";
    }
}
