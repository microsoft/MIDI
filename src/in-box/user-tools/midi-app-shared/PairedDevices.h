// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// The PCs this one is paired with: each one's key hash, the name it gave when it paired, and
// when it was last used. Kept per user, in a file the app names under %LOCALAPPDATA%, never in
// Documents or ProgramData. The file is read fresh every time, so two windows always agree.
// Pch-free.

#include <sal.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// Before windows.h, so the JSON projection is declared without the GetObject macro.
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Data.Json.h>

#include "DeviceLinkIdentity.h"

namespace midiapp
{
    constexpr size_t MaximumPairedDevices = 64;

    struct PairedDevice
    {
        DeviceKeyHash Key{};
        std::wstring Name{};

        // FILETIME values. 0 when never.
        int64_t PairedAt{ 0 };
        int64_t LastUsed{ 0 };
    };

    class PairedDeviceList
    {
    public:
        explicit PairedDeviceList(_In_ std::wstring filePath) noexcept : m_filePath(std::move(filePath)) {}

        // Most recently used first.
        std::vector<PairedDevice> Load() const noexcept;

        std::optional<PairedDevice> Find(_In_ DeviceKeyHash const& key) const noexcept;

        // Adds it, or renames it and marks it paired again. The oldest is dropped past the cap.
        bool Remember(_In_ DeviceKeyHash const& key, _In_ std::wstring const& name) noexcept;

        // Marks it used now, and takes the name it gave this time.
        bool Touch(_In_ DeviceKeyHash const& key, _In_ std::wstring const& name) noexcept;

        bool Forget(_In_ DeviceKeyHash const& key) noexcept;

        std::wstring const& FilePath() const noexcept { return m_filePath; }

    private:
        bool Save(_In_ std::vector<PairedDevice> const& devices) const noexcept;

        std::wstring m_filePath{};
    };

    // Now, as a FILETIME.
    int64_t CurrentFileTime() noexcept;
}
