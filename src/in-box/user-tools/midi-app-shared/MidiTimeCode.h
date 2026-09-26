// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <sal.h>

#include <cstdint>
#include <string>
#include <string_view>

// The arithmetic behind MIDI Time Code, on its own so it can be unit tested without a MIDI
// endpoint, a thread or WinRT. The sending lives in TimeCodeGenerator.
//
// Two things here are easy to get wrong and are the reason this is a separate file:
//
//  - **29.97 counts thirty frames a second and throws numbers away.** It does not count
//    twenty-nine. Two frame numbers are skipped at the top of every minute except every tenth
//    minute, which is what keeps the count in step with the clock on the wall. A mistake here is
//    invisible for the first minute of a run, so it needs a test rather than a listen.
//  - **A full timecode takes eight quarter frame messages, which is two frames.** The value that
//    goes on the wire is the time at the START of those eight, and a receiver adds two frames
//    when it has assembled them. Sending the current time in each piece would be two frames fast
//    and would jitter.
namespace midiapp
{
    // The value of each of these IS the two bit rate code the format puts in the hours byte, so
    // nothing has to translate between them.
    enum class MidiTimeCodeFrameRate : int32_t
    {
        Frames24 = 0,
        Frames25 = 1,
        Frames2997Drop = 2,
        Frames30 = 3
    };

    struct MidiTimeCodePosition
    {
        uint8_t Hours{ 0 };
        uint8_t Minutes{ 0 };
        uint8_t Seconds{ 0 };
        uint8_t Frames{ 0 };
    };

    // Quarter frame messages in one complete timecode, which is two frames' worth.
    inline constexpr uint8_t MidiTimeCodePiecesPerSequence = 8;
    inline constexpr uint8_t MidiTimeCodeQuarterFramesPerFrame = 4;

    // The payload of a full frame message, between its F0 and its F7:
    // 7F 7F 01 01 hh mm ss ff
    inline constexpr size_t MidiTimeCodeFullFramePayloadSize = 8;

    bool IsValidFrameRate(_In_ int32_t const value) noexcept;
    MidiTimeCodeFrameRate FrameRateFromValue(_In_ int32_t const value) noexcept;

    // How many frame numbers a second is counted in. 29.97 counts thirty of them and skips two
    // numbers a minute, so this is thirty for that rate too.
    uint8_t FramesPerSecondForCounting(_In_ MidiTimeCodeFrameRate const rate) noexcept;

    // How long a frame really lasts. Only 29.97 is not simply one over the counting rate.
    double SecondsPerFrame(_In_ MidiTimeCodeFrameRate const rate) noexcept;

    double QuarterFramesPerSecond(_In_ MidiTimeCodeFrameRate const rate) noexcept;

    bool IsPositionValid(
        _In_ MidiTimeCodePosition const& position,
        _In_ MidiTimeCodeFrameRate const rate) noexcept;

    // Brings an out of range or dropped position back to something the format can carry.
    MidiTimeCodePosition ClampPosition(
        _In_ MidiTimeCodePosition position,
        _In_ MidiTimeCodeFrameRate const rate) noexcept;

    void AdvanceOneFrame(
        _Inout_ MidiTimeCodePosition& position,
        _In_ MidiTimeCodeFrameRate const rate) noexcept;

    void AdvanceFrames(
        _Inout_ MidiTimeCodePosition& position,
        _In_ MidiTimeCodeFrameRate const rate,
        _In_ uint32_t const frameCount) noexcept;

    // The single data byte of one quarter frame message: the piece number in the high nibble and
    // four bits of the timecode in the low one. pieceIndex runs 0 through 7.
    uint8_t QuarterFrameDataByte(
        _In_ MidiTimeCodePosition const& position,
        _In_ MidiTimeCodeFrameRate const rate,
        _In_ uint8_t const pieceIndex) noexcept;

    // The eight bytes between the F0 and the F7 of a full frame message.
    void FillFullFramePayload(
        _In_ MidiTimeCodePosition const& position,
        _In_ MidiTimeCodeFrameRate const rate,
        _Out_writes_(MidiTimeCodeFullFramePayloadSize) uint8_t* payload) noexcept;

    // "01:02:03:04", or "01:02:03;04" at 29.97, which is how the industry writes drop frame.
    std::wstring FormatPosition(
        _In_ MidiTimeCodePosition const& position,
        _In_ MidiTimeCodeFrameRate const rate) noexcept;

    // Accepts either separator and a missing field or two, so "1:30" reads as one minute thirty.
    bool TryParsePosition(
        _In_ std::wstring_view const text,
        _In_ MidiTimeCodeFrameRate const rate,
        _Out_ MidiTimeCodePosition& position) noexcept;

    // "29.97 drop", for a picker or a console field.
    std::wstring FrameRateShortName(_In_ MidiTimeCodeFrameRate const rate) noexcept;

    bool TryParseFrameRate(_In_ std::wstring_view const text, _Out_ MidiTimeCodeFrameRate& rate) noexcept;
}
