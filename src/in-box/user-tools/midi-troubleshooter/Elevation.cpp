// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "Elevation.h"

namespace miditroubleshooter
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
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to check the elevation state of this process.")

        return false;
    }

    _Use_decl_annotations_
    bool TryRelaunchElevated(std::wstring const& extraArgument) noexcept
    {
        try
        {
            wchar_t modulePath[MAX_PATH]{};

            if (::GetModuleFileNameW(nullptr, modulePath, ARRAYSIZE(modulePath)) == 0)
            {
                return false;
            }

            // The original arguments are carried across so a relaunch lands on the same page
            // and keeps any development switches.
            std::wstring arguments{};

            int argumentCount{ 0 };
            wil::unique_hlocal_ptr<PWSTR[]> argumentValues{ ::CommandLineToArgvW(::GetCommandLineW(), &argumentCount) };

            if (argumentValues)
            {
                for (int i = 1; i < argumentCount; i++)
                {
                    arguments += L"\"";
                    arguments += argumentValues[i];
                    arguments += L"\" ";
                }
            }

            arguments += extraArgument;

            SHELLEXECUTEINFOW info{};

            info.cbSize = sizeof(info);
            info.fMask = SEE_MASK_NOASYNC | SEE_MASK_FLAG_NO_UI;
            info.lpVerb = L"runas";
            info.lpFile = modulePath;
            info.lpParameters = arguments.empty() ? nullptr : arguments.c_str();
            info.nShow = SW_SHOWNORMAL;

            return ::ShellExecuteExW(&info) != FALSE;
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to relaunch elevated.")

        return false;
    }
}
