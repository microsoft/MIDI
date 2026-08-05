// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "pch.h"

#include "console_tools_shared.h"



// see https://aka.ms/midiapimode for a description of each mode


#define API_MODE_MINIMUM        0
#define API_MODE_MAXIMUM        2
#define API_MODE_DEFAULT        0




std::wstring GetModeName(_In_ uint32_t const mode)
{
    switch (mode)
    {
    case 0:     return internal::ResourceGetWString(IDS_MODE_0_NAME);
    case 1:     return internal::ResourceGetWString(IDS_MODE_1_NAME);
    case 2:     return internal::ResourceGetWString(IDS_MODE_2_NAME);
    default:    return internal::ResourceGetWString(IDS_MODE_UNKNOWN_NAME);
    }
}

std::wstring GetModeDescription(_In_ uint32_t const mode)
{
    switch (mode)
    {
    case 0:     return internal::ResourceGetWString(IDS_MODE_0_DESCRIPTION);
    case 1:     return internal::ResourceGetWString(IDS_MODE_1_DESCRIPTION);
    case 2:     return internal::ResourceGetWString(IDS_MODE_2_DESCRIPTION);
    default:    return std::wstring{ };
    }
}


bool TrySetCurrentApiMode(_In_ RegistryApiMode const mode)
{
    wil::unique_hkey key;

    auto openResult = RegCreateKeyExW(
        HKEY_LOCAL_MACHINE,
        drivers32HklmKey.c_str(),
        0,
        nullptr,
        REG_OPTION_NON_VOLATILE,
        KEY_SET_VALUE,
        nullptr,
        key.put(),
        nullptr);

    if (openResult != ERROR_SUCCESS)
    {
        return false;
    }

    DWORD value{ static_cast<DWORD>(mode) };

    auto setResult = RegSetValueExW(
        key.get(),
        legacyMidiRegistryValueName.c_str(),
        0,
        REG_DWORD,
        reinterpret_cast<const BYTE*>(&value),
        sizeof(value));

    return setResult == ERROR_SUCCESS;
}


//bool IsRunningAsAdministrator()
//{
//    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
//    PSID administratorsGroup{ nullptr };
//
//    if (!AllocateAndInitializeSid(
//        &ntAuthority,
//        2,
//        SECURITY_BUILTIN_DOMAIN_RID,
//        DOMAIN_ALIAS_RID_ADMINS,
//        0, 0, 0, 0, 0, 0,
//        &administratorsGroup))
//    {
//        return false;
//    }
//
//    auto cleanup = wil::scope_exit([&]() { FreeSid(administratorsGroup); });
//
//    BOOL isMember{ FALSE };
//
//    if (!CheckTokenMembership(nullptr, administratorsGroup, &isMember))
//    {
//        return false;
//    }
//
//    return isMember ? true : false;
//}


bool TryRestartComputer()
{
    wil::unique_handle token;

    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, token.put()))
    {
        return false;
    }

    TOKEN_PRIVILEGES privileges{ };
    privileges.PrivilegeCount = 1;
    privileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    if (!LookupPrivilegeValueW(nullptr, SE_SHUTDOWN_NAME, &privileges.Privileges[0].Luid))
    {
        return false;
    }

    if (!AdjustTokenPrivileges(token.get(), FALSE, &privileges, 0, nullptr, nullptr))
    {
        return false;
    }

    if (GetLastError() != ERROR_SUCCESS)
    {
        return false;
    }

    return ExitWindowsEx(
        EWX_REBOOT | EWX_RESTARTAPPS,
        SHTDN_REASON_MAJOR_OPERATINGSYSTEM | SHTDN_REASON_MINOR_RECONFIG | SHTDN_REASON_FLAG_PLANNED) ? true : false;
}





void DisplayMode(_In_ uint32_t const mode)
{
    fmt::println(L"{} {}",
        fmt::styled(fmt::format(L"{}:", mode), fmt::fg(fmt::color::golden_rod)),
        fmt::styled(GetModeName(mode), highlightTextStyle));

    fmt::println(L"   {}", fmt::styled(GetModeDescription(mode), normalTextStyle));

    WriteBlankLine();
}

void DisplayHelp()
{
    WriteInfoLine(internal::ResourceGetWString(IDS_HELP_USAGE));
    WriteBlankLine();

    WriteInfoLine(internal::ResourceGetWString(IDS_HELP_MODES_HEADER));
    WriteSingleSeparatorLine();

    for (uint32_t mode = API_MODE_MINIMUM; mode <= API_MODE_MAXIMUM; mode++)
    {
        DisplayMode(mode);
    }

    WriteInfoLine(internal::ResourceGetWString(IDS_HELP_MORE_INFO));
    WriteHighlightLine(internal::ResourceGetWString(IDS_HELP_MORE_INFO_URL));
    WriteBlankLine();

    WriteInfoLine(internal::ResourceGetWString(IDS_HELP_ADMIN_REQUIRED));
    WriteBlankLine();
}

void DisplayCurrentMode(_In_ uint32_t const currentMode)
{
    fmt::println(L"{} {} {}",
        fmt::styled(internal::ResourceGetWString(IDS_LABEL_CURRENT_MODE), normalTextStyle),
        fmt::styled(currentMode, fmt::fg(fmt::color::golden_rod)),
        fmt::styled(GetModeName(currentMode), highlightTextStyle));

    WriteBlankLine();
}


bool TryParseRequestedMode(_In_ char* const parameter, _Out_ RegistryApiMode& requestedMode)
{
    requestedMode = RegistryApiMode::UseMidiServices;

    try
    {
        auto value = std::stoi(std::string(parameter));

        if (value < API_MODE_MINIMUM || value > API_MODE_MAXIMUM)
        {
            return false;
        }

        requestedMode = static_cast<RegistryApiMode>(value);

        return true;
    }
    catch (...)
    {
        return false;
    }
}


int __cdecl main(_In_ int argc, _In_ char* argv[])
{
    if (!TrySetConsoleTextMode())
    {
        return RETURN_ERROR_SETTING_CONSOLE_MODE;
    }

    WriteDoubleSeparatorLine();
    WriteInfoLine(internal::ResourceGetWString(IDS_BANNER_TOOL_INFO));
    WriteInfoLine(internal::ResourceGetWString(IDS_BANNER_COPYRIGHT));
    WriteInfoLine(internal::ResourceGetWString(IDS_BANNER_INFO_URL));
    WriteDoubleSeparatorLine();
    WriteInfoLine(internal::ResourceGetWString(IDS_BANNER_DESCRIPTION));
    WriteDoubleSeparatorLine();
    WriteBlankLine();

    auto currentMode = GetRegistryApiMode();

    RegistryApiMode requestedMode{ UseMidiServices };
    bool requestedModeProvided{ false };

    if (argc > 1)
    {
        requestedModeProvided = TryParseRequestedMode(argv[1], requestedMode);

        if (!requestedModeProvided)
        {
            WriteErrorLine(internal::ResourceGetWString(IDS_ERROR_INVALID_MODE_PARAMETER));
            fmt::println(L"");
        }
    }

    if (!requestedModeProvided)
    {
        DisplayHelp();
        DisplayCurrentMode(currentMode);

        return argc > 1 ? RETURN_INVALID_MODE : RETURN_SUCCESS;
    }

    DisplayCurrentMode(currentMode);

    if (requestedMode == currentMode)
    {
        WriteInfoLine(internal::ResourceGetWString(IDS_MODE_ALREADY_SET));
        return RETURN_SUCCESS;
    }

    if (!CheckForAdminPermissions())
    {
        WriteErrorLine(internal::ResourceGetWString(IDS_ERROR_NOT_ADMINISTRATOR));
        return RETURN_INSUFFICIENT_PERMISSIONS;
    }

    WriteInfoLine(internal::ResourceGetWString(IDS_LABEL_REQUESTED_MODE));
    DisplayMode(requestedMode);

    WritePromptLine(internal::ResourceGetWString(IDS_PROMPT_CONFIRM_CHANGE));

    if (!PromptForYes(internal::ResourceGetWString(IDS_PROMPT_YES_NO_KEYS)))
    {
        fmt::println(L"");
        WriteInfoLine(internal::ResourceGetWString(IDS_STATUS_CANCELLED));

        return RETURN_SUCCESS;
    }

    WriteBlankLine();

    if (!TrySetCurrentApiMode(requestedMode))
    {
        WriteErrorLine(internal::ResourceGetWString(IDS_ERROR_UNABLE_TO_WRITE_REGISTRY));
        return RETURN_REGISTRY_WRITE_FAILED;
    }

    WriteHighlightLine(internal::ResourceGetWString(IDS_STATUS_MODE_CHANGED));
    fmt::println(L"");

    WritePromptLine(internal::ResourceGetWString(IDS_PROMPT_CONFIRM_RESTART));

    if (PromptForYes(internal::ResourceGetWString(IDS_PROMPT_YES_NO_KEYS)))
    {
        WriteBlankLine();
        WriteWarningLine(internal::ResourceGetWString(IDS_STATUS_RESTARTING));

        if (!TryRestartComputer())
        {
            WriteErrorLine(internal::ResourceGetWString(IDS_ERROR_UNABLE_TO_RESTART));
            WriteWarningLine(internal::ResourceGetWString(IDS_STATUS_RESTART_LATER));
        }
    }
    else
    {
        fmt::println(L"");
        WriteWarningLine(internal::ResourceGetWString(IDS_STATUS_RESTART_LATER));
    }

    return RETURN_SUCCESS;
}
