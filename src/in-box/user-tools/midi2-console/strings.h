// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <format>
#include <tuple>

namespace midi2console
{
    // All user-facing text comes from the STRINGTABLE in Resources.rc so it can be localized.
    // Results are cached because option and command descriptions are fetched on every run.
    std::string const& ResourceString(_In_ UINT resourceId);

    // Parameterized strings use std::format {0} {1} placeholders, matching the rest of the repo.
    template <typename... TArgs>
    inline std::string FormatResourceString(_In_ UINT const resourceId, TArgs&&... args)
    {
        auto values = std::make_tuple(std::forward<TArgs>(args)...);

        return std::apply(
            [resourceId](auto&... unpacked)
            {
                return std::vformat(ResourceString(resourceId), std::make_format_args(unpacked...));
            },
            values);
    }
}
