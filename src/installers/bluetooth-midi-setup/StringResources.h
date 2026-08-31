// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

namespace midibluetoothsetup::resources
{
    // Looks up a string from Strings\<language>\Resources.resw. Never throws: if the
    // resource subsystem is unavailable the key itself is returned so the UI still renders.
    winrt::hstring GetString(std::wstring_view resourceKey) noexcept;

    namespace details
    {
        // std::format has no formatter for winrt::hstring, and most values here are one
        inline std::wstring AsFormattable(winrt::hstring const& value) noexcept
        {
            return std::wstring{ value };
        }

        template <typename TValue>
        inline TValue AsFormattable(TValue const& value) noexcept
        {
            return value;
        }
    }

    // Convenience for the very common "label: value" and templated messages. The format
    // string comes from resources and uses std::format placeholders.
    template <typename... TArgs>
    winrt::hstring FormatString(std::wstring_view resourceKey, TArgs&&... args) noexcept
    {
        try
        {
            auto formatString = GetString(resourceKey);

            // materialized first, because make_wformat_args needs lvalues
            auto values = std::make_tuple(details::AsFormattable(std::forward<TArgs>(args))...);

            return std::apply(
                [&formatString](auto&... unpacked)
                {
                    return winrt::hstring{ std::vformat(
                        std::wstring_view{ formatString }, std::make_wformat_args(unpacked...)) };
                },
                values);
        }
        catch (...)
        {
            return GetString(resourceKey);
        }
    }
}
