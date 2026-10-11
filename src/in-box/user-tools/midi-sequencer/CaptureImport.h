// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Captures from MIDI Monitor and the console (design section 10). Both save CSV with the same first
// columns: Index, TimestampTicks, OffsetMicroseconds, DeltaMicroseconds, Group, Channel, WordCount
// and Word0 to Word3. The console writes words as 0x1234ABCD and MIDI Monitor as 1234ABCD. A
// capture has exact times, so a jam plays back exactly as it happened, one track for each group.
// No XAML: the tests compile this unchanged.

#include <sal.h>

#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "SequenceModel.h"

namespace midisequencer
{
    enum class CaptureStatus : uint8_t
    {
        Read,

        // No header with the columns a capture has.
        NotACapture,

        // More messages than a sequence holds.
        TooLarge,
    };

    struct CapturedMessage
    {
        // From the first message in the capture.
        double Seconds{ 0 };
        std::array<uint32_t, 4> Words{};
        uint8_t WordCount{ 0 };
    };

    struct ReadCapture
    {
        CaptureStatus Status{ CaptureStatus::Read };
        std::vector<CapturedMessage> Messages{};

        // Lines that aren't a message, or whose words don't make one. Notes MIDI Monitor adds to a
        // capture, such as where it was paused, aren't counted.
        size_t SkippedLines{ 0 };
    };

    // UTF-8 text, with or without a byte order mark.
    ReadCapture ReadCaptureCsv(_In_ std::string_view text);

    struct CaptureText
    {
        // A new track's name, from its group counted from 0.
        std::function<std::wstring(uint8_t group)> TrackName{};

        // Where the clips came from, such as the capture's file name.
        std::wstring OriginDetail{};
    };

    struct ImportedCapture
    {
        // One track for each group that has something a track records, in group order. Each keeps
        // its messages' own channels, and has its clip placed where it was played.
        std::vector<Track> Tracks{};
        std::vector<Clip> Clips{};

        // Messages no track records: stream and utility messages, clock and other system
        // messages. System exclusive is kept.
        size_t SkippedMessages{ 0 };
    };

    // Times become ticks through the tempo map, so playback at that tempo matches the capture.
    ImportedCapture ImportCapture(
        _In_ std::vector<CapturedMessage> const& messages,
        _In_ std::vector<TempoPoint> const& tempo,
        _In_ std::vector<MeterChange> const& meter,
        _In_ CaptureText const& text);

    // Messages copied in MIDI Monitor as UMP words: a message a line, its words in hex, with or
    // without 0x. A line can hold more than one message. Text that isn't whole messages is
    // skipped, a line at a time, and counted. The messages have no times.
    std::vector<CapturedMessage> ReadUmpWordText(_In_ std::wstring_view text, _Out_ size_t& skippedLines);

    struct PastedClip
    {
        ::midisequencer::Clip Clip{};

        // The start of the bar the first message is in, like a recorded take.
        int64_t PlacementTick{ 0 };
    };

    // Pasted messages laid out one a step from startTick, or all at startTick when stepTicks is
    // 0, in the order they were copied. Note ons and offs pair up into notes, as when recording.
    PastedClip ClipFromMessages(
        _In_ std::vector<CapturedMessage> const& messages,
        _In_ int64_t startTick,
        _In_ int64_t stepTicks,
        _In_ std::vector<MeterChange> const& meter,
        _In_ std::wstring name,
        _In_ std::wstring originDetail);
}
