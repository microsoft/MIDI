// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#ifndef MIDI_STABLE_STRING_HASH_H
#define MIDI_STABLE_STRING_HASH_H

#include <cstdint>
#include <string>
#include <string_view>

namespace WindowsMidiServicesInternal
{
    // Endpoint device instance ids built by the Kernel Streaming transports embed a hash of the
    // device they were created from, and a customer's stored name, image, port names and latency
    // are matched back to a device by that id. std::hash is only guaranteed to be consistent
    // within a single run of a program and the standard library implementation may change in any
    // ABI-breaking release, so a toolchain update could renumber every endpoint on every PC at
    // once and orphan every stored customization with it.
    //
    // This produces the same value the standard library does today, frozen so it cannot move.
    // It lives in a shared header because more than one transport has to arrive at the identical
    // number, and because the value is persisted configuration: changing it is a breaking change,
    // not an implementation detail.
    inline uint64_t StableWideStringHash(_In_ std::wstring_view const value) noexcept
    {
        constexpr uint64_t offsetBasis{ 14695981039346656037ULL };
        constexpr uint64_t prime{ 1099511628211ULL };

        uint64_t hash{ offsetBasis };

        if (value.empty())
        {
            return hash;
        }

        auto const* const bytes = reinterpret_cast<uint8_t const*>(value.data());
        auto const byteCount = value.size() * sizeof(wchar_t);

        for (size_t i = 0; i < byteCount; i++)
        {
            hash ^= static_cast<uint64_t>(bytes[i]);
            hash *= prime;
        }

        return hash;
    }

    inline std::wstring StableWideStringHashString(_In_ std::wstring_view const value) noexcept
    {
        return std::to_wstring(StableWideStringHash(value));
    }
}

#endif
