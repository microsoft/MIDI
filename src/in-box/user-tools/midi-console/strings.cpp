// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"

#include <unordered_map>

#include "console_output.h"
#include "strings.h"

namespace midi2console
{
    std::string const& ResourceString(_In_ UINT resourceId)
    {
        static std::unordered_map<UINT, std::string> cache;

        auto const existing = cache.find(resourceId);

        if (existing != cache.end())
        {
            return existing->second;
        }

        // LoadStringW with a zero buffer size hands back a pointer to the read-only resource
        // data plus its length, so there is no guessing at a buffer size.
        PCWSTR resourceText{ nullptr };

        auto const characterCount = LoadStringW(
            GetModuleHandleW(nullptr),
            resourceId,
            reinterpret_cast<LPWSTR>(&resourceText),
            0);

        std::string value;

        if (characterCount > 0 && resourceText != nullptr)
        {
            value = ToUtf8(std::wstring_view{ resourceText, static_cast<size_t>(characterCount) });
        }
        else
        {
            // A missing id is a build error, not a runtime condition. Make it loud rather than
            // shipping an empty column header.
            value = std::format("<missing string {}>", resourceId);
        }

        return cache.emplace(resourceId, std::move(value)).first->second;
    }
}
