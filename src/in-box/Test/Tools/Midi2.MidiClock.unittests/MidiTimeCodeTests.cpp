// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "MidiTimeCodeTests.h"

#include "MidiTimeCode.h"

#include <array>
#include <cmath>

using namespace midiapp;

namespace
{
    MidiTimeCodePosition At(uint8_t hours, uint8_t minutes, uint8_t seconds, uint8_t frames)
    {
        MidiTimeCodePosition position{};

        position.Hours = hours;
        position.Minutes = minutes;
        position.Seconds = seconds;
        position.Frames = frames;

        return position;
    }

    void VerifyPosition(
        MidiTimeCodePosition const& actual,
        uint8_t hours, uint8_t minutes, uint8_t seconds, uint8_t frames)
    {
        VERIFY_ARE_EQUAL(hours, actual.Hours);
        VERIFY_ARE_EQUAL(minutes, actual.Minutes);
        VERIFY_ARE_EQUAL(seconds, actual.Seconds);
        VERIFY_ARE_EQUAL(frames, actual.Frames);
    }

    // Rebuilds a position the way a receiver does, from the eight quarter frame data bytes.
    MidiTimeCodePosition Reassemble(std::array<uint8_t, 8> const& dataBytes, uint8_t& rateCode)
    {
        MidiTimeCodePosition position{};

        rateCode = 0;

        for (auto const dataByte : dataBytes)
        {
            auto const piece = static_cast<uint8_t>((dataByte >> 4) & 0x07);
            auto const nibble = static_cast<uint8_t>(dataByte & 0x0F);

            switch (piece)
            {
            case 0: position.Frames = static_cast<uint8_t>(position.Frames | nibble); break;
            case 1: position.Frames = static_cast<uint8_t>(position.Frames | ((nibble & 0x01) << 4)); break;
            case 2: position.Seconds = static_cast<uint8_t>(position.Seconds | nibble); break;
            case 3: position.Seconds = static_cast<uint8_t>(position.Seconds | ((nibble & 0x03) << 4)); break;
            case 4: position.Minutes = static_cast<uint8_t>(position.Minutes | nibble); break;
            case 5: position.Minutes = static_cast<uint8_t>(position.Minutes | ((nibble & 0x03) << 4)); break;
            case 6: position.Hours = static_cast<uint8_t>(position.Hours | nibble); break;
            case 7:
                position.Hours = static_cast<uint8_t>(position.Hours | ((nibble & 0x01) << 4));
                rateCode = static_cast<uint8_t>((nibble >> 1) & 0x03);
                break;
            default: break;
            }
        }

        return position;
    }

    std::array<uint8_t, 8> AllPieces(MidiTimeCodePosition const& position, MidiTimeCodeFrameRate rate)
    {
        std::array<uint8_t, 8> dataBytes{};

        for (uint8_t piece = 0; piece < 8; piece++)
        {
            dataBytes[piece] = QuarterFrameDataByte(position, rate, piece);
        }

        return dataBytes;
    }

    constexpr MidiTimeCodeFrameRate AllRates[]
    {
        MidiTimeCodeFrameRate::Frames24,
        MidiTimeCodeFrameRate::Frames25,
        MidiTimeCodeFrameRate::Frames2997Drop,
        MidiTimeCodeFrameRate::Frames30
    };
}

void MidiTimeCodeTests::ReportsTheCountingRateForEachFrameRate()
{
    VERIFY_ARE_EQUAL(uint8_t{ 24 }, FramesPerSecondForCounting(MidiTimeCodeFrameRate::Frames24));
    VERIFY_ARE_EQUAL(uint8_t{ 25 }, FramesPerSecondForCounting(MidiTimeCodeFrameRate::Frames25));
    VERIFY_ARE_EQUAL(uint8_t{ 30 }, FramesPerSecondForCounting(MidiTimeCodeFrameRate::Frames30));

    // The one everybody gets wrong: 29.97 counts thirty numbers a second and throws two of them
    // away a minute. It does not count twenty-nine.
    VERIFY_ARE_EQUAL(uint8_t{ 30 }, FramesPerSecondForCounting(MidiTimeCodeFrameRate::Frames2997Drop));
}

void MidiTimeCodeTests::OnlyDropFrameIsSlowerThanItsCountingRate()
{
    for (auto const rate : AllRates)
    {
        auto const nominal = 1.0 / static_cast<double>(FramesPerSecondForCounting(rate));
        auto const actual = SecondsPerFrame(rate);

        if (rate == MidiTimeCodeFrameRate::Frames2997Drop)
        {
            VERIFY_IS_TRUE(actual > nominal);
            VERIFY_IS_TRUE(std::abs(actual - (1001.0 / 30000.0)) < 1e-12);
        }
        else
        {
            VERIFY_IS_TRUE(std::abs(actual - nominal) < 1e-12);
        }
    }
}

void MidiTimeCodeTests::QuarterFrameRateIsFourTimesTheFrameRate()
{
    for (auto const rate : AllRates)
    {
        auto const expected = 4.0 / SecondsPerFrame(rate);

        VERIFY_IS_TRUE(std::abs(QuarterFramesPerSecond(rate) - expected) < 1e-9);
    }
}

void MidiTimeCodeTests::AdvancesFramesSecondsMinutesAndHours()
{
    auto position = At(0, 0, 0, 28);

    AdvanceOneFrame(position, MidiTimeCodeFrameRate::Frames30);
    VerifyPosition(position, 0, 0, 0, 29);

    AdvanceOneFrame(position, MidiTimeCodeFrameRate::Frames30);
    VerifyPosition(position, 0, 0, 1, 0);

    position = At(0, 0, 59, 29);
    AdvanceOneFrame(position, MidiTimeCodeFrameRate::Frames30);
    VerifyPosition(position, 0, 1, 0, 0);

    position = At(0, 59, 59, 29);
    AdvanceOneFrame(position, MidiTimeCodeFrameRate::Frames30);
    VerifyPosition(position, 1, 0, 0, 0);
}

void MidiTimeCodeTests::WrapsAtTwentyFourHours()
{
    auto position = At(23, 59, 59, 29);

    AdvanceOneFrame(position, MidiTimeCodeFrameRate::Frames30);

    VerifyPosition(position, 0, 0, 0, 0);
}

void MidiTimeCodeTests::CountsAWholeSecondAtEveryRate()
{
    for (auto const rate : AllRates)
    {
        auto position = At(0, 0, 0, 0);

        AdvanceFrames(position, rate, FramesPerSecondForCounting(rate));

        VerifyPosition(position, 0, 0, 1, 0);
    }
}

void MidiTimeCodeTests::DropFrameSkipsTwoNumbersAtTheTopOfAMinute()
{
    auto position = At(0, 0, 59, 29);

    AdvanceOneFrame(position, MidiTimeCodeFrameRate::Frames2997Drop);

    // Frames 0 and 1 of minute 1 do not exist, so the count goes straight to 2.
    VerifyPosition(position, 0, 1, 0, 2);
}

void MidiTimeCodeTests::DropFrameKeepsThemOnEveryTenthMinute()
{
    for (uint8_t minute : { uint8_t{ 9 }, uint8_t{ 19 }, uint8_t{ 29 }, uint8_t{ 59 } })
    {
        auto position = At(0, minute, 59, 29);

        AdvanceOneFrame(position, MidiTimeCodeFrameRate::Frames2997Drop);

        auto const expectedMinute = static_cast<uint8_t>((minute + 1) % 60);

        // Minute 10, 20, 30 and the top of the hour keep their first two frame numbers, which is
        // what stops the correction from running away.
        VERIFY_ARE_EQUAL(expectedMinute, position.Minutes);
        VERIFY_ARE_EQUAL(uint8_t{ 0 }, position.Seconds);
        VERIFY_ARE_EQUAL(uint8_t{ 0 }, position.Frames);
    }
}

void MidiTimeCodeTests::DropFrameStaysWithATenthOfASecondOfTheWallClockOverAnHour()
{
    // The whole reason drop frame exists. Count an hour of frames, work out what the timecode
    // says, and compare it against how long those frames really took.
    auto position = At(0, 0, 0, 0);

    uint32_t frameCount{ 0 };

    // One hour of real time at 29.97.
    auto const framesInAnHour = static_cast<uint32_t>(std::llround(3600.0 / SecondsPerFrame(MidiTimeCodeFrameRate::Frames2997Drop)));

    for (uint32_t index = 0; index < framesInAnHour; index++)
    {
        AdvanceOneFrame(position, MidiTimeCodeFrameRate::Frames2997Drop);
        frameCount++;
    }

    auto const realSeconds = static_cast<double>(frameCount) * SecondsPerFrame(MidiTimeCodeFrameRate::Frames2997Drop);

    auto const displayedSeconds =
        (static_cast<double>(position.Hours) * 3600.0) +
        (static_cast<double>(position.Minutes) * 60.0) +
        static_cast<double>(position.Seconds) +
        (static_cast<double>(position.Frames) / 30.0);

    VERIFY_IS_TRUE(std::abs(realSeconds - displayedSeconds) < 0.1);
}

void MidiTimeCodeTests::DropFrameRejectsPositionsThatCannotExist()
{
    VERIFY_IS_FALSE(IsPositionValid(At(0, 1, 0, 0), MidiTimeCodeFrameRate::Frames2997Drop));
    VERIFY_IS_FALSE(IsPositionValid(At(0, 1, 0, 1), MidiTimeCodeFrameRate::Frames2997Drop));
    VERIFY_IS_TRUE(IsPositionValid(At(0, 1, 0, 2), MidiTimeCodeFrameRate::Frames2997Drop));

    // Every tenth minute keeps them.
    VERIFY_IS_TRUE(IsPositionValid(At(0, 10, 0, 0), MidiTimeCodeFrameRate::Frames2997Drop));
    VERIFY_IS_TRUE(IsPositionValid(At(0, 0, 0, 0), MidiTimeCodeFrameRate::Frames2997Drop));

    // The same numbers are perfectly ordinary at 30.
    VERIFY_IS_TRUE(IsPositionValid(At(0, 1, 0, 0), MidiTimeCodeFrameRate::Frames30));

    auto const clamped = ClampPosition(At(0, 1, 0, 0), MidiTimeCodeFrameRate::Frames2997Drop);

    VerifyPosition(clamped, 0, 1, 0, 2);
}

void MidiTimeCodeTests::OtherRatesNeverSkipANumber()
{
    for (auto const rate : { MidiTimeCodeFrameRate::Frames24, MidiTimeCodeFrameRate::Frames25, MidiTimeCodeFrameRate::Frames30 })
    {
        auto position = At(0, 0, 59, static_cast<uint8_t>(FramesPerSecondForCounting(rate) - 1));

        AdvanceOneFrame(position, rate);

        VerifyPosition(position, 0, 1, 0, 0);
    }
}

void MidiTimeCodeTests::QuarterFrameCarriesThePieceNumberInTheHighNibble()
{
    auto const position = At(1, 2, 3, 4);

    for (uint8_t piece = 0; piece < 8; piece++)
    {
        auto const dataByte = QuarterFrameDataByte(position, MidiTimeCodeFrameRate::Frames30, piece);

        VERIFY_ARE_EQUAL(piece, static_cast<uint8_t>((dataByte >> 4) & 0x07));

        // It is a MIDI data byte, so the top bit is always clear.
        VERIFY_ARE_EQUAL(uint8_t{ 0 }, static_cast<uint8_t>(dataByte & 0x80));
    }
}

void MidiTimeCodeTests::EightQuarterFramesRebuildThePosition()
{
    auto const position = At(23, 59, 58, 27);

    uint8_t rateCode{ 0 };

    auto const rebuilt = Reassemble(AllPieces(position, MidiTimeCodeFrameRate::Frames30), rateCode);

    VerifyPosition(rebuilt, 23, 59, 58, 27);
}

void MidiTimeCodeTests::TheLastQuarterFrameCarriesTheFrameRate()
{
    for (auto const rate : AllRates)
    {
        auto const position = At(12, 34, 56, 7);

        uint8_t rateCode{ 0 };

        auto const rebuilt = Reassemble(AllPieces(position, rate), rateCode);

        VERIFY_ARE_EQUAL(static_cast<uint8_t>(rate), rateCode);
        VerifyPosition(rebuilt, 12, 34, 56, 7);
    }
}

void MidiTimeCodeTests::FullFrameCarriesTheUniversalRealTimeHeader()
{
    std::array<uint8_t, MidiTimeCodeFullFramePayloadSize> payload{};

    FillFullFramePayload(At(1, 2, 3, 4), MidiTimeCodeFrameRate::Frames25, payload.data());

    // F0 7F <device> 01 01 hh mm ss ff F7, with the F0 and F7 added by the sender.
    VERIFY_ARE_EQUAL(uint8_t{ 0x7F }, payload[0]);
    VERIFY_ARE_EQUAL(uint8_t{ 0x7F }, payload[1]);
    VERIFY_ARE_EQUAL(uint8_t{ 0x01 }, payload[2]);
    VERIFY_ARE_EQUAL(uint8_t{ 0x01 }, payload[3]);
    VERIFY_ARE_EQUAL(uint8_t{ 2 }, payload[5]);
    VERIFY_ARE_EQUAL(uint8_t{ 3 }, payload[6]);
    VERIFY_ARE_EQUAL(uint8_t{ 4 }, payload[7]);

    for (auto const byte : payload)
    {
        VERIFY_ARE_EQUAL(uint8_t{ 0 }, static_cast<uint8_t>(byte & 0x80));
    }
}

void MidiTimeCodeTests::FullFramePacksTheRateIntoTheHoursByte()
{
    for (auto const rate : AllRates)
    {
        std::array<uint8_t, MidiTimeCodeFullFramePayloadSize> payload{};

        FillFullFramePayload(At(21, 0, 0, 0), rate, payload.data());

        VERIFY_ARE_EQUAL(uint8_t{ 21 }, static_cast<uint8_t>(payload[4] & 0x1F));
        VERIFY_ARE_EQUAL(static_cast<uint8_t>(rate), static_cast<uint8_t>((payload[4] >> 5) & 0x03));
    }
}

void MidiTimeCodeTests::FormatsWithASemicolonOnlyForDropFrame()
{
    VERIFY_ARE_EQUAL(
        std::wstring{ L"01:02:03:04" },
        FormatPosition(At(1, 2, 3, 4), MidiTimeCodeFrameRate::Frames30));

    VERIFY_ARE_EQUAL(
        std::wstring{ L"01:02:03;04" },
        FormatPosition(At(1, 2, 3, 4), MidiTimeCodeFrameRate::Frames2997Drop));
}

void MidiTimeCodeTests::ParsesAFullPosition()
{
    MidiTimeCodePosition position{};

    VERIFY_IS_TRUE(TryParsePosition(L"01:02:03:04", MidiTimeCodeFrameRate::Frames30, position));
    VerifyPosition(position, 1, 2, 3, 4);

    // The drop frame semicolon reads the same as a colon.
    VERIFY_IS_TRUE(TryParsePosition(L"01:02:03;04", MidiTimeCodeFrameRate::Frames2997Drop, position));
    VerifyPosition(position, 1, 2, 3, 4);
}

void MidiTimeCodeTests::ParsesAShortPositionFromTheRight()
{
    MidiTimeCodePosition position{};

    // A timecode field fills from the right, the way a transport does, so a bare number is
    // frames rather than hours.
    VERIFY_IS_TRUE(TryParsePosition(L"12", MidiTimeCodeFrameRate::Frames30, position));
    VerifyPosition(position, 0, 0, 0, 12);

    VERIFY_IS_TRUE(TryParsePosition(L"1:20", MidiTimeCodeFrameRate::Frames30, position));
    VerifyPosition(position, 0, 0, 1, 20);

    VERIFY_IS_TRUE(TryParsePosition(L"2:30:00", MidiTimeCodeFrameRate::Frames30, position));
    VerifyPosition(position, 0, 2, 30, 0);

    // Which is also why this is refused: the last field is frames, and there is no frame 30 at
    // thirty frames a second.
    VERIFY_IS_FALSE(TryParsePosition(L"1:30", MidiTimeCodeFrameRate::Frames30, position));
}

void MidiTimeCodeTests::RejectsAPositionOutOfRange()
{
    MidiTimeCodePosition position{};

    VERIFY_IS_FALSE(TryParsePosition(L"24:00:00:00", MidiTimeCodeFrameRate::Frames30, position));
    VERIFY_IS_FALSE(TryParsePosition(L"00:60:00:00", MidiTimeCodeFrameRate::Frames30, position));
    VERIFY_IS_FALSE(TryParsePosition(L"00:00:60:00", MidiTimeCodeFrameRate::Frames30, position));

    // 25 is not a frame number at 25 frames per second, because they are counted from zero.
    VERIFY_IS_FALSE(TryParsePosition(L"00:00:00:25", MidiTimeCodeFrameRate::Frames25, position));
    VERIFY_IS_TRUE(TryParsePosition(L"00:00:00:24", MidiTimeCodeFrameRate::Frames25, position));

    VERIFY_IS_FALSE(TryParsePosition(L"not a time", MidiTimeCodeFrameRate::Frames30, position));
    VERIFY_IS_FALSE(TryParsePosition(L"", MidiTimeCodeFrameRate::Frames30, position));
}

void MidiTimeCodeTests::RoundTripsEveryFormattedPosition()
{
    for (auto const rate : AllRates)
    {
        auto position = At(0, 0, 0, 0);

        // Two minutes of frames at every rate, which crosses a drop frame boundary and a minute
        // boundary in both directions.
        for (uint32_t index = 0; index < FramesPerSecondForCounting(rate) * 120u; index++)
        {
            MidiTimeCodePosition parsed{};

            VERIFY_IS_TRUE(TryParsePosition(FormatPosition(position, rate), rate, parsed));

            VERIFY_ARE_EQUAL(position.Hours, parsed.Hours);
            VERIFY_ARE_EQUAL(position.Minutes, parsed.Minutes);
            VERIFY_ARE_EQUAL(position.Seconds, parsed.Seconds);
            VERIFY_ARE_EQUAL(position.Frames, parsed.Frames);

            VERIFY_IS_TRUE(IsPositionValid(position, rate));

            AdvanceOneFrame(position, rate);
        }
    }
}

void MidiTimeCodeTests::ParsesFrameRateNames()
{
    MidiTimeCodeFrameRate rate{};

    VERIFY_IS_TRUE(TryParseFrameRate(L"24", rate));
    VERIFY_ARE_EQUAL(static_cast<int32_t>(MidiTimeCodeFrameRate::Frames24), static_cast<int32_t>(rate));

    VERIFY_IS_TRUE(TryParseFrameRate(L"25", rate));
    VERIFY_ARE_EQUAL(static_cast<int32_t>(MidiTimeCodeFrameRate::Frames25), static_cast<int32_t>(rate));

    VERIFY_IS_TRUE(TryParseFrameRate(L"30", rate));
    VERIFY_ARE_EQUAL(static_cast<int32_t>(MidiTimeCodeFrameRate::Frames30), static_cast<int32_t>(rate));

    for (auto const* const text : { L"29.97", L"2997", L"29.97 drop", L"drop", L"DF" })
    {
        VERIFY_IS_TRUE(TryParseFrameRate(text, rate));
        VERIFY_ARE_EQUAL(static_cast<int32_t>(MidiTimeCodeFrameRate::Frames2997Drop), static_cast<int32_t>(rate));
    }

    VERIFY_IS_FALSE(TryParseFrameRate(L"60", rate));
}
