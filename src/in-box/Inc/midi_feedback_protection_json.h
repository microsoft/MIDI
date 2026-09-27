// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <string>

// Reading and writing the loopback feedback protection setting and status. Needs json_defs.h,
// wstring_util.h and the json namespace alias first, which every consumer already has.

namespace WindowsMidiServicesInternal
{
    inline bool TryParseFeedbackProtectionValue(_In_ std::wstring const& text, _Out_ bool& enabled) noexcept
    {
        enabled = true;

        try
        {
            auto const clean = ToLowerTrimmedWStringCopy(text);

            if (clean == MIDI_CONFIG_JSON_ENDPOINT_COMMON_FEEDBACK_PROTECTION_VALUE_MUTE)
            {
                return true;
            }

            if (clean == MIDI_CONFIG_JSON_ENDPOINT_COMMON_FEEDBACK_PROTECTION_VALUE_OFF)
            {
                enabled = false;
                return true;
            }
        }
        catch (...)
        {
        }

        return false;
    }

    // Missing, the wrong type and a value nobody recognizes all leave protection on. A hand edit
    // that goes wrong should leave the customer protected rather than exposed.
    inline bool ReadFeedbackProtectionEnabled(_In_ json::JsonObject const& parent) noexcept
    {
        try
        {
            if (parent == nullptr || !parent.HasKey(MIDI_CONFIG_JSON_ENDPOINT_COMMON_FEEDBACK_PROTECTION_PROPERTY))
            {
                return true;
            }

            auto const value = parent.Lookup(MIDI_CONFIG_JSON_ENDPOINT_COMMON_FEEDBACK_PROTECTION_PROPERTY);

            if (value == nullptr || value.ValueType() != json::JsonValueType::String)
            {
                return true;
            }

            bool enabled{ true };
            TryParseFeedbackProtectionValue(std::wstring{ value.GetString() }, enabled);

            return enabled;
        }
        catch (...)
        {
            return true;
        }
    }

    inline PCWSTR FeedbackProtectionJsonValue(_In_ bool const enabled) noexcept
    {
        return enabled ?
            MIDI_CONFIG_JSON_ENDPOINT_COMMON_FEEDBACK_PROTECTION_VALUE_MUTE :
            MIDI_CONFIG_JSON_ENDPOINT_COMMON_FEEDBACK_PROTECTION_VALUE_OFF;
    }

    // A json number is a double and cannot hold a FILETIME exactly.
    inline std::wstring FileTimeToDecimalString(_In_ FILETIME const& time)
    {
        ULARGE_INTEGER value{};
        value.LowPart = time.dwLowDateTime;
        value.HighPart = time.dwHighDateTime;

        return std::to_wstring(value.QuadPart);
    }

    inline bool TryParseDecimalFileTime(_In_ std::wstring const& text, _Out_ int64_t& ticks) noexcept
    {
        ticks = 0;

        if (text.empty() || text.size() > 19)
        {
            return false;
        }

        uint64_t value{ 0 };

        for (auto const c : text)
        {
            if (c < L'0' || c > L'9')
            {
                return false;
            }

            value = value * 10 + static_cast<uint64_t>(c - L'0');
        }

        if (value > static_cast<uint64_t>(INT64_MAX))
        {
            return false;
        }

        ticks = static_cast<int64_t>(value);

        return true;
    }
}
