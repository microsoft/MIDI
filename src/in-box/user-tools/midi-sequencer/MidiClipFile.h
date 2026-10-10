// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// MIDI Clip Files (M2-116-U), the format MIDI 2.0 calls SMF2: "SMF2CLIP", then Universal MIDI
// Packets, big endian, each timed by the Delta Clockstamp before it.
//
//   SMF2CLIP
//   Set Profile On messages, with no Delta Clockstamps
//   DCS(0) DCTPQ                        ticks per quarter note
//   DCS Set Tempo, DCS Set Time Signature, DCS set-up messages...
//   DCS Start of Clip
//   DCS message, DCS message...
//   DCS End of Clip
//
// !!! EVERYTHING THE READER READS IS UNTRUSTED. !!! Counts are capped, and a file that ends early
// keeps what was read.

#include <sal.h>

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "SequenceModel.h"

namespace midisequencer
{
    struct ClipFile
    {
        // Set Profile On messages, in the order they came. Sent before anything else.
        std::vector<ClipEvent> Profiles{};

        uint16_t TicksPerQuarterNote{ static_cast<uint16_t>(midisequencer::TicksPerQuarterNote) };

        // The Clip Configuration Header: tempo, time signature and set-up messages, timed from
        // the start of the header.
        std::vector<ClipEvent> Configuration{};

        // The Clip Sequence Data, timed from Start of Clip, without Start of Clip and End of Clip.
        std::vector<ClipEvent> Events{};

        // Where End of Clip is, from Start of Clip.
        int64_t EndTick{ 0 };
    };

    enum class ClipFileReadStatus : int32_t
    {
        Success = 0,
        NotAClipFile = 1,
        TooMuchData = 2,
    };

    struct ClipFileReadResult
    {
        ClipFileReadStatus Status{ ClipFileReadStatus::Success };

        // The file ended before End of Clip, or in the middle of a message.
        bool Truncated{ false };

        size_t SkippedMessages{ 0 };

        bool Succeeded() const noexcept { return Status == ClipFileReadStatus::Success; }
    };

    inline constexpr size_t MaximumClipFileMessages = 4000000;

    std::vector<uint8_t> WriteClipFile(_In_ ClipFile const& file);

    ClipFileReadResult ReadClipFile(
        _In_ std::span<uint8_t const> bytes,
        _Out_ ClipFile& file,
        _In_ size_t maximumMessages = MaximumClipFileMessages);

    // ---- messages a clip file needs ----

    // Flex Data Set Tempo: 10 nanosecond units per quarter note. Four words.
    void BuildSetTempo(_In_ double beatsPerMinute, _In_ uint8_t group, _Out_writes_(4) uint32_t* words) noexcept;
    bool ReadSetTempo(_In_ ClipEvent const& event, _Out_ double& beatsPerMinute) noexcept;

    // Flex Data Set Time Signature. The denominator is written as a power of two, so 4 is 2.
    void BuildSetTimeSignature(_In_ MeterChange const& meter, _In_ uint8_t group, _Out_writes_(4) uint32_t* words) noexcept;
    bool ReadSetTimeSignature(_In_ ClipEvent const& event, _Out_ MeterChange& meter) noexcept;

    // Flex Data text in status bank 1, the metadata text messages.
    namespace MetadataText
    {
        inline constexpr uint8_t Unknown = 0x00;        // the general one, used for tags
        inline constexpr uint8_t ProjectName = 0x01;
        inline constexpr uint8_t CompositionName = 0x02;
        inline constexpr uint8_t ClipName = 0x03;
        inline constexpr uint8_t Copyright = 0x04;
        inline constexpr uint8_t Composer = 0x05;
    }

    // UTF-8, 12 bytes per packet, as many packets as the text needs.
    void AppendMetadataText(
        _Inout_ std::vector<ClipEvent>& events,
        _In_ int64_t tick,
        _In_ uint8_t status,
        _In_ std::wstring_view text,
        _In_ uint8_t group);

    // ---- sequences ----

    struct ClipFileText
    {
        std::wstring SequenceName{};
        std::wstring Copyright{};
        std::wstring Composer{};
    };

    // One track as one clip file: its tempo and meter, its tags and the sequence's, its start-up
    // messages in the configuration header, and everything it plays on its timeline.
    ClipFile ExportTrackToClipFile(
        _In_ Sequence const& sequence,
        _In_ Track const& track,
        _In_ ClipFileText const& text);

    struct ImportedClipFile
    {
        midisequencer::Clip Content{};
        std::wstring Name{};
        std::vector<TempoPoint> Tempo{};
        std::vector<MeterChange> Meter{};
        std::vector<Tag> Tags{};
        std::vector<ClipEvent> Startup{};
    };

    // Notes are paired into the note list. MIDI 1.0 notes are scaled up to MIDI 2.0 velocity.
    // Tempo, time signatures and text become the sequence's, and set-up messages become start-up
    // messages.
    ImportedClipFile ImportClipFile(_In_ ClipFile const& file);
}
