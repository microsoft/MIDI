// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "Elevation.h"

namespace midisettings
{
    bool IsProcessElevated() noexcept
    {
        try
        {
            wil::unique_handle token;

            if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, token.put()))
            {
                return false;
            }

            TOKEN_ELEVATION elevation{};
            DWORD returnedSize{ 0 };

            if (!::GetTokenInformation(token.get(), TokenElevation, &elevation, sizeof(elevation), &returnedSize))
            {
                return false;
            }

            return elevation.TokenIsElevated != 0;
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to check the elevation state of this process.")

        return false;
    }

    bool TryRelaunchElevated() noexcept
    {
        try
        {
            wchar_t modulePath[MAX_PATH]{};

            if (::GetModuleFileNameW(nullptr, modulePath, ARRAYSIZE(modulePath)) == 0)
            {
                return false;
            }

            SHELLEXECUTEINFOW info{};

            info.cbSize = sizeof(info);
            info.fMask = SEE_MASK_NOASYNC | SEE_MASK_FLAG_NO_UI;
            info.lpVerb = L"runas";
            info.lpFile = modulePath;
            info.nShow = SW_SHOWNORMAL;

            return ::ShellExecuteExW(&info) != FALSE;
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to relaunch elevated.")

        return false;
    }
}
