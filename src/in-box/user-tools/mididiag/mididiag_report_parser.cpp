// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// See mididiag_report_parser.h for what this reads and how to reuse it.

#include "mididiag_report_parser.h"
#include "mididiag_field_defs.h"

#include <algorithm>
#include <initializer_list>
#include <utility>

namespace mididiag::report
{
    namespace
    {
        constexpr std::wstring_view SectionHeaderPrefix{ MIDIDIAG_SECTION_HEADER_PREFIX };
        constexpr std::wstring_view FieldSeparator{ MIDIDIAG_FIELD_SEPARATOR };
        constexpr std::wstring_view ProductName{ MIDIDIAG_PRODUCT_NAME };

        constexpr std::wstring_view ErrorLabel{ MIDIDIAG_FIELD_LABEL_ERROR };
        constexpr std::wstring_view SectionTimedOutLabel{ MIDIDIAG_FIELD_LABEL_SECTION_TIMED_OUT };
        constexpr std::wstring_view FormatVersionLabel{ MIDIDIAG_FIELD_LABEL_REPORT_FORMAT_VERSION };
        constexpr std::wstring_view CurrentTimeLabel{ MIDIDIAG_FIELD_LABEL_CURRENT_TIME };
        constexpr std::wstring_view FindingLabel{ MIDIDIAG_FIELD_LABEL_FINDING };

        constexpr std::wstring_view HeaderSection{ MIDIDIAG_SECTION_LABEL_HEADER };
        constexpr std::wstring_view FindingsSection{ MIDIDIAG_SECTION_LABEL_FINDINGS };
        constexpr std::wstring_view SuccessfulRunSection{ MIDIDIAG_SECTION_LABEL_SUCCESSFUL_RUN };
        constexpr std::wstring_view AbortedRunSection{ MIDIDIAG_SECTION_LABEL_ABORTED_RUN };
        constexpr std::wstring_view EndOfFileSection{ MIDIDIAG_SECTION_LABEL_END_OF_FILE };

        // Sections every report has had since the first version. A report that lost its first
        // lines, and with them the product name and the header section, is still recognized
        // when it has a few of these.
        constexpr std::wstring_view WellKnownSections[]
        {
            MIDIDIAG_SECTION_LABEL_OS,
            MIDIDIAG_SECTION_LABEL_PROCESSOR_ENV,
            MIDIDIAG_SECTION_LABEL_NATIVE_SYSTEM_INFO,
            MIDIDIAG_SECTION_LABEL_ENUM_REGISTRY_DRIVERS32,
            MIDIDIAG_SECTION_LABEL_ENUM_TRANSPORTS,
            MIDIDIAG_SECTION_LABEL_MIDI2_API_ENDPOINTS,
            MIDIDIAG_SECTION_LABEL_WINMM_API_INPUT_ENDPOINTS,
            MIDIDIAG_SECTION_LABEL_WINMM_API_OUTPUT_ENDPOINTS,
            MIDIDIAG_SECTION_LABEL_SESSIONS,
            MIDIDIAG_SECTION_LABEL_PING_TEST,
            MIDIDIAG_SECTION_LABEL_END_OF_FILE,
        };

        constexpr std::size_t WellKnownSectionsNeeded{ 3 };

        // Format 1 rules are 120 characters long. A line at least this long, made of nothing
        // but one character, is a rule and not a value.
        constexpr std::size_t MinimumRuleLength{ 20 };

        constexpr wchar_t ByteOrderMark{ 0xFEFF };
        constexpr wchar_t ReplacementCharacter{ 0xFFFD };

        bool IsBlank(_In_ wchar_t const character) noexcept
        {
            return character == L' ' || character == L'\t';
        }

        std::wstring_view TrimRight(_In_ std::wstring_view text) noexcept
        {
            while (!text.empty() && IsBlank(text.back()))
            {
                text.remove_suffix(1);
            }

            return text;
        }

        std::wstring_view Trim(_In_ std::wstring_view text) noexcept
        {
            while (!text.empty() && IsBlank(text.front()))
            {
                text.remove_prefix(1);
            }

            return TrimRight(text);
        }

        bool IsAsciiLetter(_In_ wchar_t const character) noexcept
        {
            return (character >= L'a' && character <= L'z') || (character >= L'A' && character <= L'Z');
        }

        bool IsAsciiDigit(_In_ wchar_t const character) noexcept
        {
            return character >= L'0' && character <= L'9';
        }

        wchar_t ToAsciiLower(_In_ wchar_t const character) noexcept
        {
            return (character >= L'A' && character <= L'Z') ?
                static_cast<wchar_t>(character - L'A' + L'a') : character;
        }

        wchar_t ToAsciiUpper(_In_ wchar_t const character) noexcept
        {
            return (character >= L'a' && character <= L'z') ?
                static_cast<wchar_t>(character - L'a' + L'A') : character;
        }

        // Labels and keys are written by mididiag, and are only ever letters, digits and underscores.
        bool IsName(_In_ std::wstring_view const text) noexcept
        {
            return !text.empty() &&
                !IsAsciiDigit(text.front()) &&
                std::all_of(text.begin(), text.end(), [](wchar_t const character)
                    {
                        return IsAsciiLetter(character) || IsAsciiDigit(character) || character == L'_';
                    });
        }

        bool IsRule(_In_ std::wstring_view const line, _In_ wchar_t const character) noexcept
        {
            auto const trimmed = Trim(line);

            return trimmed.size() >= MinimumRuleLength &&
                std::all_of(trimmed.begin(), trimmed.end(), [character](wchar_t const c) { return c == character; });
        }

        // "label : value". The first separator ends the label, because labels have no spaces.
        bool TrySplitField(
            _In_ std::wstring_view const line,
            _Out_ std::wstring_view& label,
            _Out_ std::wstring_view& value) noexcept
        {
            label = {};
            value = {};

            if (auto const separator = line.find(FieldSeparator); separator != std::wstring_view::npos)
            {
                label = Trim(line.substr(0, separator));
                value = line.substr(separator + FieldSeparator.size());

                return IsName(label);
            }

            // An editor that strips trailing spaces turns an empty "label : " into "label :".
            auto const trimmed = TrimRight(line);

            if (trimmed.size() > 2 && trimmed.ends_with(L" :"))
            {
                label = Trim(trimmed.substr(0, trimmed.size() - 2));

                return IsName(label);
            }

            return false;
        }

        // Splits on \r\n, \n or \r. A newline at the very end doesn't add an empty last line.
        std::vector<std::wstring_view> SplitLines(_In_ std::wstring_view const text)
        {
            std::vector<std::wstring_view> lines{};

            std::size_t start{ 0 };

            for (std::size_t index = 0; index < text.size(); ++index)
            {
                auto const character = text[index];

                if (character != L'\r' && character != L'\n')
                {
                    continue;
                }

                lines.push_back(text.substr(start, index - start));

                if (character == L'\r' && index + 1 < text.size() && text[index + 1] == L'\n')
                {
                    ++index;
                }

                start = index + 1;
            }

            if (start < text.size())
            {
                lines.push_back(text.substr(start));
            }

            return lines;
        }

        // 0 when the text isn't a plain number.
        std::uint32_t ParseFormatVersion(_In_ std::wstring_view text) noexcept
        {
            text = Trim(text);

            // nine digits can't overflow 32 bits
            if (text.empty() || text.size() > 9)
            {
                return 0;
            }

            std::uint32_t value{ 0 };

            for (auto const character : text)
            {
                if (!IsAsciiDigit(character))
                {
                    return 0;
                }

                value = value * 10 + static_cast<std::uint32_t>(character - L'0');
            }

            return value;
        }

        class ReportBuilder
        {
        public:
            ReportBuilder(_Inout_ Report& report, _In_ bool const formatOne) noexcept :
                m_report(report),
                m_formatOne(formatOne)
            {
            }

            void AddLine(_In_ std::wstring_view const line, _In_ std::size_t const lineNumber)
            {
                if (m_formatOne)
                {
                    AddFormatOneLine(line, lineNumber);
                }
                else
                {
                    AddFormatTwoLine(line, lineNumber);
                }
            }

            void Finish()
            {
                for (auto& section : m_report.Sections)
                {
                    while (!section.Records.empty() && section.Records.back().Fields.empty())
                    {
                        section.Records.pop_back();
                    }

                    for (auto const& record : section.Records)
                    {
                        section.ErrorCount += static_cast<std::size_t>(std::count_if(
                            record.Fields.begin(), record.Fields.end(),
                            [](Field const& field) { return field.IsError(); }));
                    }

                    m_report.ErrorCount += section.ErrorCount;
                }
            }

        private:
            enum class RuleState
            {
                None,
                AwaitingName,
                AfterName
            };

            void AddFormatTwoLine(_In_ std::wstring_view const line, _In_ std::size_t const lineNumber)
            {
                if (line.starts_with(SectionHeaderPrefix))
                {
                    StartSection(Trim(line.substr(SectionHeaderPrefix.size())), lineNumber);
                    return;
                }

                if (Trim(line).empty())
                {
                    EndRecord();
                    return;
                }

                AddContent(line, lineNumber);
            }

            void AddFormatOneLine(_In_ std::wstring_view const line, _In_ std::size_t const lineNumber)
            {
                // A section name sits between two rules of = signs.
                if (IsRule(line, L'='))
                {
                    m_ruleState = m_ruleState == RuleState::AfterName ? RuleState::None : RuleState::AwaitingName;
                    return;
                }

                auto const trimmed = Trim(line);

                if (m_ruleState == RuleState::AwaitingName)
                {
                    if (!trimmed.empty())
                    {
                        StartSection(trimmed, lineNumber);
                        m_ruleState = RuleState::AfterName;
                    }

                    return;
                }

                // The closing rule is missing. Carry on, and read this line as content.
                m_ruleState = RuleState::None;

                if (IsRule(line, L'-'))
                {
                    EndRecord();
                    return;
                }

                if (trimmed.empty())
                {
                    m_groupPending = HasOpenRecord();
                    return;
                }

                AddContent(line, lineNumber);
            }

            void StartSection(_In_ std::wstring_view const name, _In_ std::size_t const lineNumber)
            {
                Section section{};

                section.Name = std::wstring{ name };
                section.LineNumber = lineNumber;

                m_report.Sections.push_back(std::move(section));

                m_groupPending = false;
            }

            bool HasOpenRecord() const noexcept
            {
                return !m_report.Sections.empty() &&
                    !m_report.Sections.back().Records.empty() &&
                    !m_report.Sections.back().Records.back().Fields.empty();
            }

            void EndRecord()
            {
                m_groupPending = false;

                if (HasOpenRecord())
                {
                    m_report.Sections.back().Records.emplace_back();
                }
            }

            void AddContent(_In_ std::wstring_view const line, _In_ std::size_t const lineNumber)
            {
                if (m_report.Sections.empty())
                {
                    m_report.Preamble.emplace_back(Trim(line));
                    return;
                }

                Field field{};

                field.LineNumber = lineNumber;
                field.StartsGroup = m_groupPending;

                m_groupPending = false;

                std::wstring_view label{};
                std::wstring_view value{};

                if (TrySplitField(line, label, value))
                {
                    field.Label = std::wstring{ label };
                    field.Value = std::wstring{ value };

                    // Format 1 never wrote pairs, so a value there that happens to look like
                    // one is left whole. An error message is a sentence, never pairs.
                    if (!m_formatOne && field.Label != ErrorLabel)
                    {
                        TryParseValueParts(field.Value, field.Parts);
                    }
                }
                else
                {
                    field.Value = std::wstring{ TrimRight(line) };
                }

                auto& records = m_report.Sections.back().Records;

                if (records.empty())
                {
                    records.emplace_back();
                }

                records.back().Fields.push_back(std::move(field));
            }

            Report& m_report;
            bool const m_formatOne;

            RuleState m_ruleState{ RuleState::None };

            // format 1: a blank line inside a record starts a group of related fields
            bool m_groupPending{ false };
        };

        bool HasProductName(_In_ Report const& report) noexcept
        {
            auto const isProductName = [](std::wstring_view const text) { return Trim(text) == ProductName; };

            if (std::any_of(report.Preamble.begin(), report.Preamble.end(), isProductName))
            {
                return true;
            }

            // format 1 puts it inside the header section, as a line of text
            if (auto const* const header = report.FindSection(HeaderSection))
            {
                for (auto const& record : header->Records)
                {
                    for (auto const& field : record.Fields)
                    {
                        if (field.IsText() && isProductName(field.Value))
                        {
                            return true;
                        }
                    }
                }
            }

            return false;
        }

        bool LooksLikeReport(_In_ Report const& report) noexcept
        {
            if (HasProductName(report) ||
                report.FindField(HeaderSection, FormatVersionLabel) != nullptr ||
                report.FindField(HeaderSection, CurrentTimeLabel) != nullptr)
            {
                return true;
            }

            auto const found = std::count_if(
                std::begin(WellKnownSections), std::end(WellKnownSections),
                [&report](std::wstring_view const name) { return report.FindSection(name) != nullptr; });

            return static_cast<std::size_t>(found) >= WellKnownSectionsNeeded;
        }

        void CollectFindings(_Inout_ Report& report)
        {
            for (auto const& section : report.Sections)
            {
                if (section.Name != FindingsSection)
                {
                    continue;
                }

                for (auto const& record : section.Records)
                {
                    for (auto const& field : record.Fields)
                    {
                        if (field.Label != FindingLabel)
                        {
                            continue;
                        }

                        Finding finding{};

                        if (auto const* const id = field.FindPart(L"id"))
                        {
                            finding.Id = *id;
                        }

                        auto const* const text = field.FindPart(L"text");

                        finding.Text = text != nullptr ? *text : field.Value;

                        report.Findings.push_back(std::move(finding));
                    }
                }
            }
        }

        Outcome DecideOutcome(_In_ Report const& report) noexcept
        {
            if (report.FindSection(EndOfFileSection) == nullptr)
            {
                return Outcome::Incomplete;
            }

            if (report.FindSection(AbortedRunSection) == nullptr &&
                report.FindSection(SuccessfulRunSection) != nullptr)
            {
                return Outcome::Finished;
            }

            return Outcome::StoppedEarly;
        }

        void AppendCodePoint(_Inout_ std::wstring& text, _In_ char32_t const codePoint)
        {
            if constexpr (sizeof(wchar_t) == 2)
            {
                if (codePoint >= 0x10000)
                {
                    auto const offset = codePoint - 0x10000;

                    text.push_back(static_cast<wchar_t>(0xD800 + (offset >> 10)));
                    text.push_back(static_cast<wchar_t>(0xDC00 + (offset & 0x3FF)));

                    return;
                }
            }

            text.push_back(static_cast<wchar_t>(codePoint));
        }

        std::wstring DecodeUtf8(_In_ std::span<std::byte const> const bytes)
        {
            std::wstring text{};
            text.reserve(bytes.size());

            std::size_t index{ 0 };

            while (index < bytes.size())
            {
                auto const lead = std::to_integer<unsigned int>(bytes[index]);

                if (lead < 0x80)
                {
                    text.push_back(static_cast<wchar_t>(lead));
                    ++index;
                    continue;
                }

                std::size_t length{ 0 };
                char32_t codePoint{ 0 };
                char32_t minimum{ 0 };

                if ((lead & 0xE0) == 0xC0)
                {
                    length = 2;
                    codePoint = lead & 0x1F;
                    minimum = 0x80;
                }
                else if ((lead & 0xF0) == 0xE0)
                {
                    length = 3;
                    codePoint = lead & 0x0F;
                    minimum = 0x800;
                }
                else if ((lead & 0xF8) == 0xF0)
                {
                    length = 4;
                    codePoint = lead & 0x07;
                    minimum = 0x10000;
                }

                auto valid = length != 0 && length <= bytes.size() - index;

                for (std::size_t offset = 1; valid && offset < length; ++offset)
                {
                    auto const next = std::to_integer<unsigned int>(bytes[index + offset]);

                    if ((next & 0xC0) != 0x80)
                    {
                        valid = false;
                    }
                    else
                    {
                        codePoint = (codePoint << 6) | (next & 0x3F);
                    }
                }

                // overlong forms, surrogates and values past the last code point are not text
                if (valid && (codePoint < minimum || codePoint > 0x10FFFF || (codePoint >= 0xD800 && codePoint <= 0xDFFF)))
                {
                    valid = false;
                }

                if (!valid)
                {
                    text.push_back(ReplacementCharacter);
                    ++index;
                    continue;
                }

                AppendCodePoint(text, codePoint);
                index += length;
            }

            return text;
        }

        std::wstring DecodeUtf16(_In_ std::span<std::byte const> const bytes, _In_ bool const littleEndian)
        {
            std::wstring text{};
            text.reserve(bytes.size() / 2);

            auto const unitAt = [&bytes, littleEndian](std::size_t const index) noexcept
                {
                    auto const first = std::to_integer<char16_t>(bytes[index]);
                    auto const second = std::to_integer<char16_t>(bytes[index + 1]);

                    return static_cast<char16_t>(littleEndian ? (first | (second << 8)) : ((first << 8) | second));
                };

            for (std::size_t index = 0; index + 1 < bytes.size(); index += 2)
            {
                auto const unit = unitAt(index);

                if constexpr (sizeof(wchar_t) == 2)
                {
                    text.push_back(static_cast<wchar_t>(unit));
                }
                else
                {
                    // wchar_t holds whole code points here, so a surrogate pair becomes one
                    if (unit >= 0xD800 && unit <= 0xDBFF && index + 3 < bytes.size())
                    {
                        auto const low = unitAt(index + 2);

                        if (low >= 0xDC00 && low <= 0xDFFF)
                        {
                            AppendCodePoint(text, 0x10000 + ((static_cast<char32_t>(unit) - 0xD800) << 10) + (low - 0xDC00));
                            index += 2;
                            continue;
                        }
                    }

                    text.push_back((unit >= 0xD800 && unit <= 0xDFFF) ? ReplacementCharacter : static_cast<wchar_t>(unit));
                }
            }

            return text;
        }

        // Report text is almost all ASCII, so UTF-16 without a byte order mark has a zero in
        // nearly every other byte. UTF-8 text never has a zero byte at all.
        bool LooksLikeUtf16(_In_ std::span<std::byte const> const bytes, _Out_ bool& littleEndian) noexcept
        {
            littleEndian = true;

            auto const sampleSize = std::min<std::size_t>(bytes.size(), 4096) & ~std::size_t{ 1 };

            if (sampleSize < 4)
            {
                return false;
            }

            std::size_t evenZeros{ 0 };
            std::size_t oddZeros{ 0 };

            for (std::size_t index = 0; index < sampleSize; ++index)
            {
                if (bytes[index] == std::byte{ 0 })
                {
                    ++((index % 2 == 0) ? evenZeros : oddZeros);
                }
            }

            auto const units = sampleSize / 2;

            if (oddZeros * 10 >= units * 7 && evenZeros * 10 <= units)
            {
                littleEndian = true;
                return true;
            }

            if (evenZeros * 10 >= units * 7 && oddZeros * 10 <= units)
            {
                littleEndian = false;
                return true;
            }

            return false;
        }

        struct WordReplacement
        {
            std::wstring_view Word;
            std::wstring_view Replacement;
        };

        // Labels that read badly word by word.
        constexpr WordReplacement LabelReplacements[]
        {
            { MIDIDIAG_HEADER_FIELD_LABEL_VERSION_BUILD_SOURCE, L"Build source" },
            { MIDIDIAG_HEADER_FIELD_LABEL_VERSION_NAME, L"Build name" },
            { MIDIDIAG_HEADER_FIELD_LABEL_VERSION_FULL, L"Build version" },
        };

        // Abbreviations a label uses, as a person would write them.
        constexpr WordReplacement WordReplacements[]
        {
            { L"api", L"API" },
            { L"ble", L"BLE" },
            { L"cc", L"CC" },
            { L"clsid", L"CLSID" },
            { L"dbm", L"dBm" },
            { L"desc", L"description" },
            { L"dll", L"DLL" },
            { L"drivers32", L"Drivers32" },
            { L"drivers32wow", L"Drivers32 WOW" },
            { L"exe", L"EXE" },
            { L"gtb", L"GTB" },
            { L"guid", L"GUID" },
            { L"hresult", L"HRESULT" },
            { L"id", L"ID" },
            { L"ids", L"IDs" },
            { L"inf", L"INF" },
            { L"ip", L"IP" },
            { L"jr", L"JR" },
            { L"ks", L"KS" },
            { L"ksa", L"KSA" },
            { L"mid", L"MID" },
            { L"midi", L"MIDI" },
            { L"midi1", L"MIDI 1.0" },
            { L"midi2", L"MIDI 2.0" },
            { L"mmcss", L"MMCSS" },
            { L"mpe", L"MPE" },
            { L"num", L"number" },
            { L"os", L"OS" },
            { L"pid", L"PID" },
            { L"pnp", L"PnP" },
            { L"reg", L"registry" },
            { L"rssi", L"RSSI" },
            { L"sdk", L"SDK" },
            { L"swd", L"SWD" },
            { L"sysex", L"SysEx" },
            { L"ui", L"UI" },
            { L"ump", L"UMP" },
            { L"usb", L"USB" },
            { L"utf8", L"UTF-8" },
            { L"ver", L"version" },
            { L"vid", L"VID" },
            { L"winmm", L"WinMM" },
            { L"winrt", L"WinRT" },
            { L"wow", L"WOW" },
        };

        std::vector<std::wstring> SplitNameIntoWords(_In_ std::wstring_view const name)
        {
            std::vector<std::wstring> words{};

            auto const hasUnderscore = name.find(L'_') != std::wstring_view::npos;
            auto const hasLower = std::any_of(name.begin(), name.end(), [](wchar_t const c) { return c >= L'a' && c <= L'z'; });
            auto const hasUpper = std::any_of(name.begin(), name.end(), [](wchar_t const c) { return c >= L'A' && c <= L'Z'; });

            // camelCase, as the transport capability keys are written
            auto const splitOnCase = !hasUnderscore && hasLower && hasUpper;

            std::wstring word{};

            for (auto const character : name)
            {
                if (character == L'_' || character == L' ' || character == L'-')
                {
                    if (!word.empty())
                    {
                        words.push_back(std::move(word));
                        word.clear();
                    }

                    continue;
                }

                if (splitOnCase && character >= L'A' && character <= L'Z' && !word.empty())
                {
                    words.push_back(std::move(word));
                    word.clear();
                }

                word.push_back(ToAsciiLower(character));
            }

            if (!word.empty())
            {
                words.push_back(std::move(word));
            }

            return words;
        }
    }

    bool Field::IsError() const noexcept
    {
        return Label == ErrorLabel || Label == SectionTimedOutLabel;
    }

    _Use_decl_annotations_
    std::wstring const* Field::FindPart(std::wstring_view const key) const noexcept
    {
        for (auto const& part : Parts)
        {
            if (part.Key == key)
            {
                return &part.Value;
            }
        }

        return nullptr;
    }

    _Use_decl_annotations_
    Field const* Record::FindField(std::wstring_view const label) const noexcept
    {
        for (auto const& field : Fields)
        {
            if (field.Label == label)
            {
                return &field;
            }
        }

        return nullptr;
    }

    _Use_decl_annotations_
    Field const* Section::FindField(std::wstring_view const label) const noexcept
    {
        for (auto const& record : Records)
        {
            if (auto const* const field = record.FindField(label))
            {
                return field;
            }
        }

        return nullptr;
    }

    _Use_decl_annotations_
    Section const* Report::FindSection(std::wstring_view const name) const noexcept
    {
        for (auto const& section : Sections)
        {
            if (section.Name == name)
            {
                return &section;
            }
        }

        return nullptr;
    }

    _Use_decl_annotations_
    Field const* Report::FindField(std::wstring_view const sectionName, std::wstring_view const label) const noexcept
    {
        auto const* const section = FindSection(sectionName);

        return section != nullptr ? section->FindField(label) : nullptr;
    }

    // Annotated in full here. With _Use_decl_annotations_ instead, code analysis reports
    // warning C28251 for this one function.
    ParseResult ParseReport(_In_ std::wstring_view const text)
    {
        ParseResult result{};

        if (text.size() > MaximumReportSize)
        {
            result.Status = ParseStatus::TooLarge;
            return result;
        }

        auto body = text;

        if (!body.empty() && body.front() == ByteOrderMark)
        {
            body.remove_prefix(1);
        }

        auto const lines = SplitLines(body);

        if (std::all_of(lines.begin(), lines.end(), [](std::wstring_view const line) { return Trim(line).empty(); }))
        {
            result.Status = ParseStatus::Empty;
            return result;
        }

        // Format 2 never writes a rule, so one rule anywhere means the older layout.
        auto const formatOne = std::any_of(lines.begin(), lines.end(),
            [](std::wstring_view const line) { return IsRule(line, L'='); });

        auto& report = result.Parsed;

        ReportBuilder builder{ report, formatOne };

        for (std::size_t index = 0; index < lines.size(); ++index)
        {
            builder.AddLine(lines[index], index + 1);
        }

        builder.Finish();

        if (!LooksLikeReport(report))
        {
            result.Status = ParseStatus::NotAReport;
            result.Parsed = Report{};

            return result;
        }

        if (auto const* const version = report.FindField(HeaderSection, FormatVersionLabel))
        {
            report.FormatVersion = ParseFormatVersion(version->Value);
        }

        // A report cut short before its header still shows which layout it uses.
        if (report.FormatVersion == 0)
        {
            report.FormatVersion = formatOne ? 1 : 2;
        }

        CollectFindings(report);

        report.Result = DecideOutcome(report);

        result.Status = ParseStatus::Succeeded;

        return result;
    }

    _Use_decl_annotations_
    std::wstring DecodeReportBytes(std::span<std::byte const> const bytes)
    {
        auto const startsWith = [&bytes](std::initializer_list<unsigned int> const prefix) noexcept
            {
                if (bytes.size() < prefix.size())
                {
                    return false;
                }

                std::size_t index{ 0 };

                for (auto const expected : prefix)
                {
                    if (std::to_integer<unsigned int>(bytes[index++]) != expected)
                    {
                        return false;
                    }
                }

                return true;
            };

        if (startsWith({ 0xEF, 0xBB, 0xBF }))
        {
            return DecodeUtf8(bytes.subspan(3));
        }

        if (startsWith({ 0xFF, 0xFE }))
        {
            return DecodeUtf16(bytes.subspan(2), true);
        }

        if (startsWith({ 0xFE, 0xFF }))
        {
            return DecodeUtf16(bytes.subspan(2), false);
        }

        if (bool littleEndian{ true }; LooksLikeUtf16(bytes, littleEndian))
        {
            return DecodeUtf16(bytes, littleEndian);
        }

        return DecodeUtf8(bytes);
    }

    _Use_decl_annotations_
    bool TryParseValueParts(std::wstring_view const value, std::vector<ValuePart>& parts)
    {
        parts.clear();

        auto const fail = [&parts]()
            {
                parts.clear();
                return false;
            };

        std::size_t index{ 0 };

        while (index < value.size())
        {
            if (value[index] == L' ')
            {
                ++index;
                continue;
            }

            auto const keyStart = index;

            while (index < value.size() && value[index] != L'=' && value[index] != L' ')
            {
                ++index;
            }

            if (index >= value.size() || value[index] != L'=')
            {
                return fail();
            }

            auto const key = value.substr(keyStart, index - keyStart);

            if (!IsName(key))
            {
                return fail();
            }

            // past the equals sign
            ++index;

            ValuePart part{};
            part.Key = std::wstring{ key };

            if (index < value.size() && value[index] == L'"')
            {
                ++index;

                auto closed = false;

                while (index < value.size())
                {
                    if (value[index] == L'"')
                    {
                        // a quote inside a value is written twice, as in a CSV file
                        if (index + 1 < value.size() && value[index + 1] == L'"')
                        {
                            part.Value.push_back(L'"');
                            index += 2;
                            continue;
                        }

                        ++index;
                        closed = true;
                        break;
                    }

                    part.Value.push_back(value[index]);
                    ++index;
                }

                if (!closed || (index < value.size() && value[index] != L' '))
                {
                    return fail();
                }
            }
            else
            {
                auto const valueStart = index;

                while (index < value.size() && value[index] != L' ')
                {
                    ++index;
                }

                auto const text = value.substr(valueStart, index - valueStart);

                // mididiag quotes a value that is empty or holds an equals sign or a quote, so
                // a bare one like that means this is ordinary text, not pairs
                if (text.empty() || text.find_first_of(L"=\"") != std::wstring_view::npos)
                {
                    return fail();
                }

                part.Value = std::wstring{ text };
            }

            parts.push_back(std::move(part));
        }

        return !parts.empty();
    }

    _Use_decl_annotations_
    std::wstring ReadableName(std::wstring_view const name)
    {
        for (auto const& replacement : LabelReplacements)
        {
            if (replacement.Word == name)
            {
                return std::wstring{ replacement.Replacement };
            }
        }

        auto const words = SplitNameIntoWords(name);

        std::wstring readable{};

        for (auto const& word : words)
        {
            auto const replacement = std::find_if(
                std::begin(WordReplacements), std::end(WordReplacements),
                [&word](WordReplacement const& candidate) { return candidate.Word == word; });

            if (!readable.empty())
            {
                readable.push_back(L' ');
            }

            readable += replacement != std::end(WordReplacements) ? replacement->Replacement : std::wstring_view{ word };
        }

        if (!readable.empty())
        {
            readable.front() = ToAsciiUpper(readable.front());
        }

        return readable;
    }
}
