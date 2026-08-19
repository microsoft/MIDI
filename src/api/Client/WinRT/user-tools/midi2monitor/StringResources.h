// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

namespace midi2monitor::resources
{
    // Looks up a string from Strings\<language>\Resources.resw. Never throws: if the
    // resource subsystem is unavailable the key itself is returned so the UI still renders.
    winrt::hstring GetString(std::wstring_view resourceKey) noexcept;

    // Convenience for the very common "label: value" and templated messages. The format
    // string comes from resources and uses std::format placeholders.
    template <typename... TArgs>
    winrt::hstring FormatString(std::wstring_view resourceKey, TArgs&&... args) noexcept
    {
        try
        {
            auto formatString = GetString(resourceKey);

            return winrt::hstring{ std::vformat(
                std::wstring_view{ formatString }, std::make_wformat_args(args...)) };
        }
        catch (...)
        {
            return GetString(resourceKey);
        }
    }
}
