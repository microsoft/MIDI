// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Deliberately free of pch.h, WinRT and XAML, so the unit tests compile it unchanged.

#include "CalcTimecode.h"

#include <algorithm>
#include <cmath>
#include <cwctype>
#include <format>
#include <vector>

namespace midicalc
{
    namespace
    {
        // frames a second as a fraction, so conversions between rates are exact
        struct Ratio
        {
            int64_t Numerator{ 1 };
            int64_t Denominator{ 1 };
        };

        Ratio RateRatio(_In_ FrameRate rate) noexcept
        {
            switch (rate)
            {
            case FrameRate::Fps23976: return { 24000, 1001 };
            case FrameRate::Fps25: return { 25, 1 };
            case FrameRate::Fps2997Drop:
            case FrameRate::Fps2997: return { 30000, 1001 };
            case FrameRate::Fps30: return { 30, 1 };
            case FrameRate::Fps50: return { 50, 1 };
            case FrameRate::Fps5994Drop:
            case FrameRate::Fps5994: return { 60000, 1001 };
            case FrameRate::Fps60: return { 60, 1 };
            default: return { 24, 1 };
            }
        }

        // frame numbers skipped at the top of each minute that isn't a tenth one
        int64_t DroppedPerMinute(_In_ FrameRate rate) noexcept
        {
            switch (rate)
            {
            case FrameRate::Fps2997Drop: return 2;
            case FrameRate::Fps5994Drop: return 4;
            default: return 0;
            }
        }

        // floor division that also works for a negative numerator
        int64_t FloorDivide(_In_ int64_t numerator, _In_ int64_t denominator) noexcept
        {
            auto quotient = numerator / denominator;

            if ((numerator % denominator != 0) && ((numerator < 0) != (denominator < 0)))
            {
                --quotient;
            }

            return quotient;
        }

        bool IsSeparator(_In_ wchar_t ch) noexcept
        {
            return ch == L':' || ch == L';' || ch == L'.';
        }
    }

    _Use_decl_annotations_
    int32_t NominalFramesPerSecond(FrameRate rate) noexcept
    {
        switch (rate)
        {
        case FrameRate::Fps25: return 25;
        case FrameRate::Fps2997Drop:
        case FrameRate::Fps2997:
        case FrameRate::Fps30: return 30;
        case FrameRate::Fps50: return 50;
        case FrameRate::Fps5994Drop:
        case FrameRate::Fps5994:
        case FrameRate::Fps60: return 60;
        default: return 24;
        }
    }

    _Use_decl_annotations_
    double FramesPerSecond(FrameRate rate) noexcept
    {
        auto const ratio = RateRatio(rate);
        return static_cast<double>(ratio.Numerator) / static_cast<double>(ratio.Denominator);
    }

    _Use_decl_annotations_
    bool IsDropFrame(FrameRate rate) noexcept
    {
        return DroppedPerMinute(rate) != 0;
    }

    _Use_decl_annotations_
    bool IsSkippedFrameNumber(Timecode const& timecode, FrameRate rate) noexcept
    {
        auto const dropped = DroppedPerMinute(rate);

        return dropped != 0 &&
            timecode.Seconds == 0 &&
            timecode.Frames < static_cast<uint32_t>(dropped) &&
            timecode.Minutes % 10 != 0;
    }

    _Use_decl_annotations_
    bool IsValidTimecode(Timecode const& timecode, FrameRate rate) noexcept
    {
        return timecode.Hours <= MaximumTimecodeHours &&
            timecode.Minutes < 60 &&
            timecode.Seconds < 60 &&
            timecode.Frames < static_cast<uint32_t>(NominalFramesPerSecond(rate)) &&
            !IsSkippedFrameNumber(timecode, rate);
    }

    _Use_decl_annotations_
    int64_t TimecodeToFrames(Timecode const& timecode, FrameRate rate) noexcept
    {
        int64_t const nominal = NominalFramesPerSecond(rate);
        int64_t const totalMinutes = static_cast<int64_t>(timecode.Hours) * 60 + timecode.Minutes;

        auto frames = ((totalMinutes * 60) + timecode.Seconds) * nominal + timecode.Frames;
        frames -= DroppedPerMinute(rate) * (totalMinutes - totalMinutes / 10);

        return timecode.Negative ? -frames : frames;
    }

    _Use_decl_annotations_
    Timecode FramesToTimecode(int64_t frames, FrameRate rate) noexcept
    {
        Timecode timecode{};
        timecode.Negative = frames < 0;

        int64_t const nominal = NominalFramesPerSecond(rate);
        auto const dropped = DroppedPerMinute(rate);
        auto count = frames < 0 ? -frames : frames;

        if (dropped != 0)
        {
            // put the skipped numbers back, then count as if nothing were skipped
            auto const perTenMinutes = nominal * 600 - dropped * 9;
            auto const perMinute = nominal * 60 - dropped;
            auto const tens = count / perTenMinutes;
            auto const rest = count % perTenMinutes;

            count += dropped * 9 * tens;

            if (rest > dropped)
            {
                count += dropped * ((rest - dropped) / perMinute);
            }
        }

        timecode.Frames = static_cast<uint32_t>(count % nominal);
        count /= nominal;
        timecode.Seconds = static_cast<uint32_t>(count % 60);
        count /= 60;
        timecode.Minutes = static_cast<uint32_t>(count % 60);
        count /= 60;
        timecode.Hours = static_cast<uint32_t>(count);

        return timecode;
    }

    _Use_decl_annotations_
    std::wstring FormatTimecode(Timecode const& timecode, FrameRate rate)
    {
        return std::format(L"{}{:02}:{:02}:{:02}{}{:02}",
            timecode.Negative ? L"-" : L"",
            timecode.Hours, timecode.Minutes, timecode.Seconds,
            IsDropFrame(rate) ? L';' : L':',
            timecode.Frames);
    }

    _Use_decl_annotations_
    std::optional<Timecode> ParseTimecode(std::wstring_view text, FrameRate rate) noexcept
    {
        try
        {
            while (!text.empty() && std::iswspace(text.front()))
            {
                text.remove_prefix(1);
            }

            while (!text.empty() && std::iswspace(text.back()))
            {
                text.remove_suffix(1);
            }

            bool negative{ false };

            // a hyphen or the minus sign
            if (!text.empty() && (text.front() == L'-' || text.front() == L'\u2212'))
            {
                negative = true;
                text.remove_prefix(1);
            }

            if (text.empty())
            {
                return std::nullopt;
            }

            // the fields, last one first: frames, seconds, minutes, hours
            std::vector<uint32_t> fields{};

            if (std::all_of(text.begin(), text.end(), [](wchar_t ch) { return ch >= L'0' && ch <= L'9'; }))
            {
                // digits on their own fill two at a time from the right
                if (text.size() > 9)
                {
                    return std::nullopt;
                }

                auto end = text.size();

                while (end > 0)
                {
                    auto const start = (end >= 2 && fields.size() < 3) ? end - 2 : 0;
                    uint32_t value{ 0 };

                    for (auto i = start; i < end; ++i)
                    {
                        value = value * 10 + static_cast<uint32_t>(text[i] - L'0');
                    }

                    fields.push_back(value);
                    end = start;
                }
            }
            else
            {
                size_t start{ 0 };

                while (true)
                {
                    auto end = start;

                    while (end < text.size() && !IsSeparator(text[end]))
                    {
                        ++end;
                    }

                    auto const field = text.substr(start, end - start);

                    if (field.empty() || field.size() > 3 ||
                        !std::all_of(field.begin(), field.end(), [](wchar_t ch) { return ch >= L'0' && ch <= L'9'; }))
                    {
                        return std::nullopt;
                    }

                    uint32_t value{ 0 };

                    for (auto ch : field)
                    {
                        value = value * 10 + static_cast<uint32_t>(ch - L'0');
                    }

                    fields.insert(fields.begin(), value);

                    if (end >= text.size())
                    {
                        break;
                    }

                    start = end + 1;
                }
            }

            if (fields.empty() || fields.size() > 4)
            {
                return std::nullopt;
            }

            Timecode timecode{};
            timecode.Negative = negative;
            timecode.Frames = fields[0];
            timecode.Seconds = fields.size() > 1 ? fields[1] : 0;
            timecode.Minutes = fields.size() > 2 ? fields[2] : 0;
            timecode.Hours = fields.size() > 3 ? fields[3] : 0;

            if (!IsValidTimecode(timecode, rate))
            {
                return std::nullopt;
            }

            // no such thing as minus zero
            if (TimecodeToFrames(timecode, rate) == 0)
            {
                timecode.Negative = false;
            }

            return timecode;
        }
        catch (...)
        {
            return std::nullopt;
        }
    }

    _Use_decl_annotations_
    double FramesToSeconds(int64_t frames, FrameRate rate) noexcept
    {
        auto const ratio = RateRatio(rate);
        return static_cast<double>(frames) * static_cast<double>(ratio.Denominator) / static_cast<double>(ratio.Numerator);
    }

    _Use_decl_annotations_
    FrameMoment SecondsToFrames(double seconds, FrameRate rate) noexcept
    {
        if (!std::isfinite(seconds))
        {
            return {};
        }

        auto const exact = seconds * FramesPerSecond(rate);
        // a hair of tolerance, so 1 second at 25 is 25 frames and not 24 and 0.99999
        auto const whole = std::floor(exact + 1e-9);

        return { static_cast<int64_t>(whole), std::max(0.0, exact - whole) };
    }

    _Use_decl_annotations_
    FrameMoment ConvertFrames(int64_t frames, FrameRate from, FrameRate to) noexcept
    {
        // frames * (from's seconds a frame) * (to's frames a second), in whole numbers: the
        // largest product, 999 hours of 60 fps times 1001 times 60000, is well inside 64 bits
        auto const source = RateRatio(from);
        auto const target = RateRatio(to);

        auto const numerator = frames * source.Denominator * target.Numerator;
        auto const denominator = source.Numerator * target.Denominator;
        auto const whole = FloorDivide(numerator, denominator);

        return { whole, static_cast<double>(numerator - whole * denominator) / static_cast<double>(denominator) };
    }

    _Use_decl_annotations_
    std::optional<BarPosition> BarPositionAt(double seconds, double beatsPerMinute, int32_t meterNumerator, int32_t meterDenominator) noexcept
    {
        if (!std::isfinite(seconds) || seconds < 0.0 || !std::isfinite(beatsPerMinute) || beatsPerMinute <= 0.0 ||
            meterNumerator <= 0 || meterDenominator <= 0)
        {
            return std::nullopt;
        }

        auto const beats = seconds * beatsPerMinute / 60.0 * meterDenominator / 4.0;
        auto const bars = std::floor(beats / meterNumerator + 1e-9);
        auto const inBar = std::clamp(beats - bars * meterNumerator, 0.0, static_cast<double>(meterNumerator));
        auto const beat = std::min(std::floor(inBar + 1e-9), static_cast<double>(meterNumerator - 1));

        BarPosition position{};
        position.Bar = static_cast<uint64_t>(bars) + 1;
        position.Beat = static_cast<uint32_t>(beat) + 1;
        position.BeatFraction = std::clamp(inBar - beat, 0.0, 0.999999);

        return position;
    }

    _Use_decl_annotations_
    double BufferMilliseconds(uint32_t bufferSamples, double sampleRate) noexcept
    {
        if (!(sampleRate > 0.0))
        {
            return 0.0;
        }

        return static_cast<double>(bufferSamples) * 1000.0 / sampleRate;
    }
}
