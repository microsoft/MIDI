// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Reads a mididiag report back into sections, records and fields, so a support tool can show
// it, search it or check it for known problems instead of a person reading the text.
//
// You're welcome to copy this file, mididiag_report_parser.cpp and mididiag_field_defs.h into
// your own tools. They need C++20 and the standard library, and nothing else. Nothing here is
// specific to Windows. The parser only reads text that it's given. It never opens files, runs
// programs or uses the network, so it's safe to use on a report a customer sent you.
//
// It reads both layouts mididiag has written:
//
//   Format 2 has a report_format_version field in its header section. Each section starts with
//   a line like "== os". The rules are at the top of mididiag_field_defs.h.
//
//   Format 1 is the older layout. It has a line of = signs above and below each section name,
//   and a line of - signs between records. Its values are never split into key=value pairs.
//
// A report that somebody edited, or copied only in part, still parses. A line that isn't a
// field is kept as text, and Report::Result says whether the report reached its end.

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// SAL annotations are for the Microsoft compiler's code analysis. Elsewhere they're empty.
#if defined(_MSC_VER)
#include <sal.h>
#endif

#ifndef _In_
#define _In_
#endif

#ifndef _Out_
#define _Out_
#endif

#ifndef _Inout_
#define _Inout_
#endif

#ifndef _Use_decl_annotations_
#define _Use_decl_annotations_
#endif

namespace mididiag::report
{
    // Neither a report file nor its text may be larger than this. The largest real report seen
    // so far is well under one megabyte, so anything this big is something else.
    constexpr std::size_t MaximumReportSize{ 64 * 1024 * 1024 };

    // One pair from a value such as: flow=out number=3 group=1 name="Port 1"
    struct ValuePart
    {
        std::wstring Key{};
        std::wstring Value{};
    };

    struct Field
    {
        // Empty for a line of free text, such as the lines at the top of a report.
        std::wstring Label{};

        // Everything after the separator, exactly as written. For a line of text, the line.
        std::wstring Value{};

        // The pairs in Value, when Value is written as key=value pairs. Otherwise empty.
        std::vector<ValuePart> Parts{};

        // Where the field is in the report text, counting from 1.
        std::size_t LineNumber{ 0 };

        // Format 1 only. True for the first field after a blank line inside a record, which
        // starts a group of related fields, such as one function block.
        bool StartsGroup{ false };

        bool IsText() const noexcept { return Label.empty(); }

        // An ERROR field, or the section_timed_out field that marks the section a report
        // stopped in.
        bool IsError() const noexcept;

        // The value of one pair, or nullptr when there is no pair with that key.
        std::wstring const* FindPart(_In_ std::wstring_view const key) const noexcept;
    };

    // One item in a section, such as one endpoint or one app.
    struct Record
    {
        std::vector<Field> Fields{};

        // The first field with this label, or nullptr.
        Field const* FindField(_In_ std::wstring_view const label) const noexcept;
    };

    struct Section
    {
        // As written in the report, such as "enum_ump_api_endpoints"
        std::wstring Name{};

        // Where the section starts in the report text, counting from 1.
        std::size_t LineNumber{ 0 };

        std::vector<Record> Records{};

        // How many fields in this section are errors.
        std::size_t ErrorCount{ 0 };

        // The first field with this label in any record, or nullptr.
        Field const* FindField(_In_ std::wstring_view const label) const noexcept;
    };

    // Something mididiag noticed that's likely to cause a problem, from the findings section.
    struct Finding
    {
        // Stable across versions and languages, such as "problem_device". Check this, not the text.
        std::wstring Id{};

        // A sentence for a person, in the language the report was written in.
        std::wstring Text{};
    };

    enum class Outcome
    {
        // Ended with successful_run and end_of_file.
        Finished,

        // Ended with aborted_run, or reached end_of_file without saying it finished. A section
        // that never returned, or a service that stopped answering, ends a report this way.
        StoppedEarly,

        // Never reached end_of_file. mididiag stopped suddenly, or only part of the report was copied.
        Incomplete
    };

    struct Report
    {
        // 1 for a report written before report_format_version existed.
        std::uint32_t FormatVersion{ 0 };

        // Lines before the first section, without blank lines.
        std::vector<std::wstring> Preamble{};

        std::vector<Section> Sections{};

        std::vector<Finding> Findings{};

        Outcome Result{ Outcome::Incomplete };

        // How many fields in the whole report are errors.
        std::size_t ErrorCount{ 0 };

        // The first section with this name, or nullptr.
        Section const* FindSection(_In_ std::wstring_view const name) const noexcept;

        // The first field with this label in the first section with this name, or nullptr.
        Field const* FindField(
            _In_ std::wstring_view const sectionName,
            _In_ std::wstring_view const label) const noexcept;
    };

    enum class ParseStatus
    {
        Succeeded,

        // Nothing in the text but blank lines.
        Empty,

        // Text that isn't a mididiag report.
        NotAReport,

        // Longer than MaximumReportSize.
        TooLarge
    };

    struct ParseResult
    {
        ParseStatus Status{ ParseStatus::NotAReport };

        // Filled in when Status is Succeeded.
        Report Parsed{};
    };

    // Reads report text. Malformed text is never an error: whatever can be read is kept. The
    // only exception this can throw is std::bad_alloc.
    ParseResult ParseReport(_In_ std::wstring_view const text);

    // Turns the bytes of a report file into text. mididiag writes UTF-8. A report saved by a
    // PowerShell 5.1 redirect is UTF-16, usually with a byte order mark, and this reads both.
    // Bytes that aren't valid become U+FFFD. Check the size against MaximumReportSize first.
    std::wstring DecodeReportBytes(_In_ std::span<std::byte const> const bytes);

    // Splits a value such as: flow=out number=3 name="Port 1". Returns false, and leaves
    // parts empty, when the value isn't written as key=value pairs.
    bool TryParseValueParts(_In_ std::wstring_view const value, _Out_ std::vector<ValuePart>& parts);

    // English display text for a label or key, for tools that show reports to people:
    // "parent_usb_vid" becomes "Parent USB VID", and "PROCESSOR_ARCHITECTURE" becomes
    // "Processor architecture".
    std::wstring ReadableName(_In_ std::wstring_view const name);
}
