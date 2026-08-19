// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "TimestampFormatter.h"
#include "StringResources.h"

namespace midi2monitor
{
    namespace
    {
        std::wstring GetLocaleSeparator(LCTYPE type, std::wstring_view fallback) noexcept
        {
            wchar_t buffer[8]{};

            if (::GetLocaleInfoEx(LOCALE_NAME_USER_DEFAULT, type, buffer, ARRAYSIZE(buffer)) > 0)
            {
                return buffer;
            }

            return std::wstring{ fallback };
        }

        std::wstring const& GroupSeparator() noexcept
        {
            static std::wstring const separator{ GetLocaleSeparator(LOCALE_STHOUSAND, L",") };
            return separator;
        }

        std::wstring const& DecimalSeparator() noexcept
        {
            static std::wstring const separator{ GetLocaleSeparator(LOCALE_SDECIMAL, L".") };
            return separator;
        }

        std::wstring ApplyDecimalSeparator(std::wstring value)
        {
            auto const position = value.find(L'.');

            if (position != std::wstring::npos)
            {
                value.replace(position, 1, DecimalSeparator());
            }

            return value;
        }

        std::wstring FormatGroupedInteger(uint64_t value)
        {
            auto digits = std::format(L"{}", value);

            if (digits.size() <= 3)
            {
                return digits;
            }

            auto const& separator = GroupSeparator();

            std::wstring result{};
            result.reserve(digits.size() + (digits.size() / 3) * separator.size());

            auto const leading = digits.size() % 3;
            size_t position{ 0 };

            if (leading > 0)
            {
                result.append(digits, 0, leading);
                position = leading;
            }

            while (position < digits.size())
            {
                if (!result.empty())
                {
                    result.append(separator);
                }

                result.append(digits, position, 3);
                position += 3;
            }

            return result;
        }

        std::wstring FormatFixed(double value, int precision)
        {
            return ApplyDecimalSeparator(std::format(L"{:.{}f}", value, precision));
        }

        struct UnitLabels
        {
            winrt::hstring Microseconds;
            winrt::hstring Milliseconds;
            winrt::hstring Seconds;
            winrt::hstring Minutes;
            winrt::hstring Hours;
        };

        UnitLabels const& Units() noexcept
        {
            static UnitLabels units
            {
                resources::GetString(L"UnitAbbreviationMicroseconds"),
                resources::GetString(L"UnitAbbreviationMilliseconds"),
                resources::GetString(L"UnitAbbreviationSeconds"),
                resources::GetString(L"UnitAbbreviationMinutes"),
                resources::GetString(L"UnitAbbreviationHours")
            };

            return units;
        }
    }

    double TimestampFormatter::TimestampFrequency() noexcept
    {
        static double const frequency = []() noexcept -> double
            {
                try
                {
                    auto const ticksPerSecond = midi2::MidiClock::TimestampFrequency();

                    if (ticksPerSecond > 0)
                    {
                        return static_cast<double>(ticksPerSecond);
                    }
                }
                MIDI_MONITOR_CATCH_AND_LOG(L"Unable to read the MIDI clock frequency. Falling back to QPC.")

                LARGE_INTEGER performanceFrequency{};

                if (::QueryPerformanceFrequency(&performanceFrequency) && performanceFrequency.QuadPart > 0)
                {
                    return static_cast<double>(performanceFrequency.QuadPart);
                }

                return 10000000.0;
            }();

        return frequency;
    }

    _Use_decl_annotations_
    FormattedTime TimestampFormatter::FormatTimestamp(uint64_t timestampTicks, TimestampDisplayFormat format) noexcept
    {
        try
        {
            auto const seconds = static_cast<double>(timestampTicks) / TimestampFrequency();

            switch (format)
            {
            case TimestampDisplayFormat::Microseconds:
                return { winrt::hstring{ FormatFixed(seconds * 1000000.0, 3) }, Units().Microseconds };

            case TimestampDisplayFormat::Milliseconds:
                return { winrt::hstring{ FormatFixed(seconds * 1000.0, 3) }, Units().Milliseconds };

            case TimestampDisplayFormat::Seconds:
                return { winrt::hstring{ FormatFixed(seconds, 3) }, Units().Seconds };

            case TimestampDisplayFormat::Ticks:
            default:
                // raw clock ticks carry no unit suffix
                return { winrt::hstring{ FormatGroupedInteger(timestampTicks) }, {} };
            }
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to format the message timestamp.")

        return {};
    }

    _Use_decl_annotations_
    FormattedTime TimestampFormatter::FormatDelta(uint64_t deltaTicks) noexcept
    {
        try
        {
            auto const microseconds = (static_cast<double>(deltaTicks) / TimestampFrequency()) * 1000000.0;

            if (microseconds < 1000.0)
            {
                return { winrt::hstring{ FormatFixed(microseconds, 2) }, Units().Microseconds };
            }

            if (microseconds < 1000000.0)
            {
                return { winrt::hstring{ FormatFixed(microseconds / 1000.0, 2) }, Units().Milliseconds };
            }

            if (microseconds < 60000000.0)
            {
                return { winrt::hstring{ FormatFixed(microseconds / 1000000.0, 3) }, Units().Seconds };
            }

            if (microseconds < 3600000000.0)
            {
                return { winrt::hstring{ FormatFixed(microseconds / 60000000.0, 2) }, Units().Minutes };
            }

            return { winrt::hstring{ FormatFixed(microseconds / 3600000000.0, 2) }, Units().Hours };
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to format the message delta time.")

        return {};
    }
}
