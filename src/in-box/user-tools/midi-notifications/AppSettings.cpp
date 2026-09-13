// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"

bool AppSettings::NotificationsEnabled() noexcept
{
    return ReadFlag(MIDI_NOTIFICATIONS_VALUE_ENABLED, true);
}

bool AppSettings::NetworkApprovalNotificationsEnabled() noexcept
{
    return ReadFlag(MIDI_NOTIFICATIONS_VALUE_NETWORK_APPROVAL, true);
}

_Use_decl_annotations_
bool AppSettings::ReadFlag(PCWSTR const valueName, bool const defaultValue) noexcept
{
    wil::unique_hkey key{ };

    if (::RegOpenKeyExW(
            HKEY_CURRENT_USER,
            MIDI_NOTIFICATIONS_SETTINGS_REG_KEY,
            0,
            KEY_QUERY_VALUE,
            key.put()) != ERROR_SUCCESS)
    {
        // Never configured. A customer who has not been asked gets the default.
        return defaultValue;
    }

    DWORD value{ 0 };
    DWORD valueSize{ sizeof(value) };
    DWORD valueType{ 0 };

    if (::RegQueryValueExW(
            key.get(),
            valueName,
            nullptr,
            &valueType,
            reinterpret_cast<LPBYTE>(&value),
            &valueSize) != ERROR_SUCCESS ||
        valueType != REG_DWORD)
    {
        return defaultValue;
    }

    return value != 0;
}
