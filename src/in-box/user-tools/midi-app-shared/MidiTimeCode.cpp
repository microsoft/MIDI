// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Deliberately free of pch.h, WinRT and XAML, so this compiles unchanged into the unit test
// project. See MidiTimeCode.h for the two rules this file exists to get right.

#include "MidiTimeCode.h"

#include <array>
#include <cwctype>
#include <format>

namespace midiapp
{
    namespace
    {
        constexpr uint8_t UniversalRealTimeId = 0x7F;
        constexpr uint8_t AllDevicesId = 0x7F;
        constexpr uint8_t MidiTimeCodeSubId = 0x01;
        constexpr uint8_t FullMessageSubId2 = 0x01;
    }

    _Use_decl_annotations_
    bool IsValidFrameRate(int32_t const value) noexcept
    {
        return value >= 0 && value <= 3;
    }

    _Use_decl_annotations_
    MidiTimeCodeFrameRate FrameRateFromValue(int32_t const value) noexcept
    {
        return IsValidFrameRate(value)
            ? static_cast<MidiTimeCodeFrameRate>(value)
            : MidiTimeCodeFrameRate::Frames30;
    }

    _Use_decl_annotations_
    uint8_t FramesPerSecondForCounting(MidiTimeCodeFrameRate const rate) noexcept
    {
        switch (rate)
        {
        case MidiTimeCodeFrameRate::Frames24:       return 24;
        case MidiTimeCodeFrameRate::Frames25:       return 25;
        case MidiTimeCodeFrameRate::Frames2997Drop: return 30;
        default:                                     return 30;
        }
    }

    _Use_decl_annotations_
    double SecondsPerFrame(MidiTimeCodeFrameRate const rate) noexcept
    {
        switch (rate)
        {
        case MidiTimeCodeFrameRate::Frames24:       return 1.0 / 24.0;
        case MidiTimeCodeFrameRate::Frames25:       return 1.0 / 25.0;

        // 30 slowed by exactly 1000/1001, which is where the "drop" comes from: the count runs
        // slightly fast against the wall, and skipping numbers pulls it back.
        case MidiTimeCodeFrameRate::Frames2997Drop: return 1001.0 / 30000.0;

        default:                                     return 1.0 / 30.0;
        }
    }

    _Use_decl_annotations_
    double QuarterFramesPerSecond(MidiTimeCodeFrameRate const rate) noexcept
    {
        return static_cast<double>(MidiTimeCodeQuarterFramesPerFrame) / SecondsPerFrame(rate);
    }

    _Use_decl_annotations_
    bool IsPositionValid(MidiTimeCodePosition const& position, MidiTimeCodeFrameRate const rate) noexcept
    {
        if (position.Hours > 23 || position.Minutes > 59 || position.Seconds > 59)
        {
            return false;
        }

        if (position.Frames >= FramesPerSecondForCounting(rate))
        {
            return false;
        }

        // The two numbers a drop frame minute skips are not positions anything can be at.
        if (rate == MidiTimeCodeFrameRate::Frames2997Drop &&
            position.Seconds == 0 && position.Frames < 2 && (position.Minutes % 10) != 0)
        {
            return false;
        }

        return true;
    }

    _Use_decl_annotations_
    MidiTimeCodePosition ClampPosition(MidiTimeCodePosition position, MidiTimeCodeFrameRate const rate) noexcept
    {
        if (position.Hours > 23)   { position.Hours = 23; }
        if (position.Minutes > 59) { position.Minutes = 59; }
        if (position.Seconds > 59) { position.Seconds = 59; }

        auto const framesPerSecond = FramesPerSecondForCounting(rate);

        if (position.Frames >= framesPerSecond)
        {
            position.Frames = static_cast<uint8_t>(framesPerSecond - 1);
        }

        if (rate == MidiTimeCodeFrameRate::Frames2997Drop &&
            position.Seconds == 0 && position.Frames < 2 && (position.Minutes % 10) != 0)
        {
            position.Frames = 2;
        }

        return position;
    }

    _Use_decl_annotations_
    void AdvanceOneFrame(MidiTimeCodePosition& position, MidiTimeCodeFrameRate const rate) noexcept
    {
        auto const framesPerSecond = FramesPerSecondForCounting(rate);

        position.Frames++;

        if (position.Frames < framesPerSecond)
        {
            return;
        }

        position.Frames = 0;
        position.Seconds++;

        if (position.Seconds >= 60)
        {
            position.Seconds = 0;
            position.Minutes++;

            if (position.Minutes >= 60)
            {
                position.Minutes = 0;
                position.Hours = position.Hours >= 23 ? uint8_t{ 0 } : static_cast<uint8_t>(position.Hours + 1);
            }

            // Frame numbers 0 and 1 do not exist at the top of a minute, except every tenth
            // minute where they are kept to stop the correction running away.
            if (rate == MidiTimeCodeFrameRate::Frames2997Drop && (position.Minutes % 10) != 0)
            {
                position.Frames = 2;
            }
        }
    }

    _Use_decl_annotations_
    void AdvanceFrames(
        MidiTimeCodePosition& position,
        MidiTimeCodeFrameRate const rate,
        uint32_t const frameCount) noexcept
    {
        for (uint32_t index = 0; index < frameCount; index++)
        {
            AdvanceOneFrame(position, rate);
        }
    }

    _Use_decl_annotations_
    uint8_t QuarterFrameDataByte(
        MidiTimeCodePosition const& position,
        MidiTimeCodeFrameRate const rate,
        uint8_t const pieceIndex) noexcept
    {
        auto const piece = static_cast<uint8_t>(pieceIndex & 0x07);

        uint8_t nibble{ 0 };

        switch (piece)
        {
        case 0: nibble = static_cast<uint8_t>(position.Frames & 0x0F); break;
        case 1: nibble = static_cast<uint8_t>((position.Frames >> 4) & 0x01); break;
        case 2: nibble = static_cast<uint8_t>(position.Seconds & 0x0F); break;
        case 3: nibble = static_cast<uint8_t>((position.Seconds >> 4) & 0x03); break;
        case 4: nibble = static_cast<uint8_t>(position.Minutes & 0x0F); break;
        case 5: nibble = static_cast<uint8_t>((position.Minutes >> 4) & 0x03); break;
        case 6: nibble = static_cast<uint8_t>(position.Hours & 0x0F); break;

        // The last piece carries the top bit of the hours and the rate, which is the only place
        // the format says what rate the rest of the numbers are counted in.
        case 7:
            nibble = static_cast<uint8_t>(((position.Hours >> 4) & 0x01) | ((static_cast<uint8_t>(rate) & 0x03) << 1));
            break;

        default: break;
        }

        return static_cast<uint8_t>((piece << 4) | (nibble & 0x0F));
    }

    _Use_decl_annotations_
    void FillFullFramePayload(
        MidiTimeCodePosition const& position,
        MidiTimeCodeFrameRate const rate,
        uint8_t* payload) noexcept
    {
        if (payload == nullptr)
        {
            return;
        }

        payload[0] = UniversalRealTimeId;
        payload[1] = AllDevicesId;
        payload[2] = MidiTimeCodeSubId;
        payload[3] = FullMessageSubId2;
        payload[4] = static_cast<uint8_t>(((static_cast<uint8_t>(rate) & 0x03) << 5) | (position.Hours & 0x1F));
        payload[5] = static_cast<uint8_t>(position.Minutes & 0x3F);
        payload[6] = static_cast<uint8_t>(position.Seconds & 0x3F);
        payload[7] = static_cast<uint8_t>(position.Frames & 0x1F);
    }

    _Use_decl_annotations_
    std::wstring FormatPosition(
        MidiTimeCodePosition const& position,
        MidiTimeCodeFrameRate const rate) noexcept
    {
        try
        {
            // A semicolon in front of the frames is how the industry writes drop frame, and it
            // is worth keeping because it is the only visible difference from 30.
            auto const separator = rate == MidiTimeCodeFrameRate::Frames2997Drop ? L';' : L':';

            return std::format(L"{:02}:{:02}:{:02}{}{:02}",
                position.Hours, position.Minutes, position.Seconds, separator, position.Frames);
        }
        catch (...)
        {
            return L"00:00:00:00";
        }
    }

    _Use_decl_annotations_
    bool TryParsePosition(
        std::wstring_view const text,
        MidiTimeCodeFrameRate const rate,
        MidiTimeCodePosition& position) noexcept
    {
        position = MidiTimeCodePosition{};

        std::array<uint32_t, 4> fields{};
        size_t fieldCount{ 0 };

        uint32_t value{ 0 };
        bool haveDigit{ false };

        auto const commit = [&]() noexcept
            {
                if (fieldCount < fields.size())
                {
                    fields[fieldCount] = value;
                }

                fieldCount++;
                value = 0;
                haveDigit = false;
            };

        for (auto const character : text)
        {
            if (character == L' ' || character == L'\t')
            {
                continue;
            }

            if (character >= L'0' && character <= L'9')
            {
                value = (value * 10) + static_cast<uint32_t>(character - L'0');

                if (value > 999)
                {
                    return false;
                }

                haveDigit = true;
                continue;
            }

            if (character == L':' || character == L';' || character == L'.')
            {
                commit();
                continue;
            }

            return false;
        }

        if (haveDigit || fieldCount > 0)
        {
            commit();
        }

        if (fieldCount == 0 || fieldCount > 4)
        {
            return false;
        }

        // Fewer fields than four are read from the right, so "1:30" is a minute and a half and
        // "12" is twelve frames. That is how a transport field behaves everywhere else.
        std::array<uint32_t, 4> parts{};

        for (size_t index = 0; index < fieldCount; index++)
        {
            parts[4 - fieldCount + index] = fields[index];
        }

        if (parts[0] > 23 || parts[1] > 59 || parts[2] > 59 ||
            parts[3] >= FramesPerSecondForCounting(rate))
        {
            return false;
        }

        position.Hours = static_cast<uint8_t>(parts[0]);
        position.Minutes = static_cast<uint8_t>(parts[1]);
        position.Seconds = static_cast<uint8_t>(parts[2]);
        position.Frames = static_cast<uint8_t>(parts[3]);

        return true;
    }

    _Use_decl_annotations_
    std::wstring FrameRateShortName(MidiTimeCodeFrameRate const rate) noexcept
    {
        switch (rate)
        {
        case MidiTimeCodeFrameRate::Frames24:       return L"24";
        case MidiTimeCodeFrameRate::Frames25:       return L"25";
        case MidiTimeCodeFrameRate::Frames2997Drop: return L"29.97 drop";
        default:                                     return L"30";
        }
    }

    _Use_decl_annotations_
    bool TryParseFrameRate(std::wstring_view const text, MidiTimeCodeFrameRate& rate) noexcept
    {
        rate = MidiTimeCodeFrameRate::Frames30;

        std::wstring value{};

        for (auto const character : text)
        {
            if (character == L' ' || character == L'\t' || character == L'-' || character == L'_')
            {
                continue;
            }

            value.push_back(static_cast<wchar_t>(::towlower(character)));
        }

        if (value == L"24")   { rate = MidiTimeCodeFrameRate::Frames24; return true; }
        if (value == L"25")   { rate = MidiTimeCodeFrameRate::Frames25; return true; }
        if (value == L"30")   { rate = MidiTimeCodeFrameRate::Frames30; return true; }

        if (value == L"29.97" || value == L"2997" || value == L"29.97drop" ||
            value == L"2997drop" || value == L"drop" || value == L"df")
        {
            rate = MidiTimeCodeFrameRate::Frames2997Drop;
            return true;
        }

        return false;
    }
}
