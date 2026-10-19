// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Where themes come from. An app carries its own themes inside its executable as RCDATA resources,
// so they can't go missing next to an unpackaged exe, the way the MIDI tools carry their icons.

#include <sal.h>

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

#include "ThemeDefinition.h"

namespace themeengine
{
    // UTF-8 (with or without a byte order mark) to UTF-16. Bytes that aren't UTF-8 become U+FFFD.
    std::wstring Utf8ToWide(_In_ std::string_view text);

    // A theme stored as an RCDATA resource in the executable (or in the module given).
    ThemeLoadResult LoadThemeFromResource(_In_ uint16_t resourceId, _In_opt_ void* module = nullptr);

    // The themes in the order given, leaving out any that can't be read. Each problem is passed
    // to the callback, if there is one, with the resource id.
    std::vector<std::shared_ptr<ThemeDefinition const>> LoadThemesFromResources(
        _In_ std::span<uint16_t const> resourceIds,
        _In_ std::function<void(uint16_t resourceId, std::wstring const& problem)> const& onProblem = {});

    // The theme with this id, or the first one, or null when there are none.
    std::shared_ptr<ThemeDefinition const> FindTheme(
        _In_ std::vector<std::shared_ptr<ThemeDefinition const>> const& themes,
        _In_ std::wstring_view id) noexcept;
}
