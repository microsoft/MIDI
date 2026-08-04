// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "pch.h"

#include <io.h>
#include <fcntl.h>

#include <fmt/base.h>
#include <fmt/xchar.h>
#include <fmt/format.h>
#include <fmt/color.h>


// see https://aka.ms/midiapimode for a description of each mode

#define REG_KEY_DRIVERS32       L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Drivers32"
#define REG_VALUE_API_MODE      L"UseLegacyMidi"

#define API_MODE_MINIMUM        0
#define API_MODE_MAXIMUM        2
#define API_MODE_DEFAULT        0

#define LINE_LENGTH 79

#define RETURN_SUCCESS                  0
#define RETURN_INVALID_MODE             1
#define RETURN_NOT_ADMINISTRATOR        2
#define RETURN_REGISTRY_READ_FAILED     3
#define RETURN_REGISTRY_WRITE_FAILED    4

#define KEY_UPPERCASE_Y     0x59
#define KEY_LOWERCASE_Y     0x79


const auto infoTextStyle = fmt::fg(fmt::color::steel_blue);
const auto errorTextStyle = fmt::fg(fmt::color::pink);
const auto normalTextStyle = fmt::fg(fmt::color::light_gray);
const auto separatorTextStyle = fmt::fg(fmt::color::gray);
const auto highlightTextStyle = fmt::fg(fmt::color::aqua);
const auto promptTextStyle = fmt::fg(fmt::color::light_green);
const auto warningTextStyle = fmt::fg(fmt::color::light_yellow);


void WriteInfo(_In_ std::wstring const& info)
{
    fmt::println(L"{}", fmt::styled(info, infoTextStyle));
}

void WriteNormal(_In_ std::wstring const& text)
{
    fmt::println(L"{}", fmt::styled(text, normalTextStyle));
}

void WriteHighlight(_In_ std::wstring const& text)
{
    fmt::println(L"{}", fmt::styled(text, highlightTextStyle));
}

void WriteWarning(_In_ std::wstring const& text)
{
    fmt::println(L"{}", fmt::styled(text, warningTextStyle));
}

void WritePrompt(_In_ std::wstring const& text)
{
    fmt::println(L"{}", fmt::styled(text, promptTextStyle));
}

void WriteError(_In_ std::wstring const& error)
{
    fmt::println(L"{}", fmt::styled(error, errorTextStyle));
}

void WriteDoubleSeparator()
{
    fmt::println(L"{}", fmt::styled(std::wstring(LINE_LENGTH, L'='), separatorTextStyle));
}

void WriteSingleSeparator()
{
    fmt::println(L"{}", fmt::styled(std::wstring(LINE_LENGTH, L'-'), separatorTextStyle));
}


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


// returns false only if the value exists but could not be read

bool TryGetCurrentApiMode(_Out_ uint32_t& mode, _Out_ bool& valueExistsInRegistry)
{
    mode = API_MODE_DEFAULT;
    valueExistsInRegistry = false;

    wil::unique_hkey key;

    auto openResult = RegOpenKeyExW(
        HKEY_LOCAL_MACHINE,
        REG_KEY_DRIVERS32,
        0,
        KEY_QUERY_VALUE,
        key.put());

    if (openResult != ERROR_SUCCESS)
    {
        // the key should always be present. If it isn't, we can't report a mode
        return false;
    }

    DWORD value{ 0 };
    DWORD valueSize{ sizeof(value) };
    DWORD valueType{ 0 };

    auto queryResult = RegQueryValueExW(
        key.get(),
        REG_VALUE_API_MODE,
        nullptr,
        &valueType,
        reinterpret_cast<LPBYTE>(&value),
        &valueSize);

    if (queryResult == ERROR_FILE_NOT_FOUND)
    {
        // not set, so the default mode is in effect
        return true;
    }

    if (queryResult != ERROR_SUCCESS || valueType != REG_DWORD)
    {
        return false;
    }

    valueExistsInRegistry = true;
    mode = static_cast<uint32_t>(value);

    return true;
}

bool TrySetCurrentApiMode(_In_ uint32_t const mode)
{
    wil::unique_hkey key;

    auto openResult = RegCreateKeyExW(
        HKEY_LOCAL_MACHINE,
        REG_KEY_DRIVERS32,
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
        REG_VALUE_API_MODE,
        0,
        REG_DWORD,
        reinterpret_cast<const BYTE*>(&value),
        sizeof(value));

    return setResult == ERROR_SUCCESS;
}


bool IsRunningAsAdministrator()
{
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
    PSID administratorsGroup{ nullptr };

    if (!AllocateAndInitializeSid(
        &ntAuthority,
        2,
        SECURITY_BUILTIN_DOMAIN_RID,
        DOMAIN_ALIAS_RID_ADMINS,
        0, 0, 0, 0, 0, 0,
        &administratorsGroup))
    {
        return false;
    }

    auto cleanup = wil::scope_exit([&]() { FreeSid(administratorsGroup); });

    BOOL isMember{ FALSE };

    if (!CheckTokenMembership(nullptr, administratorsGroup, &isMember))
    {
        return false;
    }

    return isMember ? true : false;
}


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


bool PromptForYes()
{
    WritePrompt(internal::ResourceGetWString(IDS_PROMPT_YES_NO_KEYS));

    auto ch = _getch();

    return (ch == KEY_UPPERCASE_Y || ch == KEY_LOWERCASE_Y);
}


void DisplayMode(_In_ uint32_t const mode)
{
    fmt::println(L"{} {}",
        fmt::styled(fmt::format(L"{}:", mode), fmt::fg(fmt::color::golden_rod)),
        fmt::styled(GetModeName(mode), highlightTextStyle));

    fmt::println(L"   {}", fmt::styled(GetModeDescription(mode), normalTextStyle));

    fmt::println(L"");
}

void DisplayHelp()
{
    WriteInfo(internal::ResourceGetWString(IDS_HELP_USAGE));
    fmt::println(L"");

    WriteInfo(internal::ResourceGetWString(IDS_HELP_MODES_HEADER));
    WriteSingleSeparator();

    for (uint32_t mode = API_MODE_MINIMUM; mode <= API_MODE_MAXIMUM; mode++)
    {
        DisplayMode(mode);
    }

    WriteInfo(internal::ResourceGetWString(IDS_HELP_MORE_INFO));
    WriteHighlight(internal::ResourceGetWString(IDS_HELP_MORE_INFO_URL));
    fmt::println(L"");

    WriteInfo(internal::ResourceGetWString(IDS_HELP_ADMIN_REQUIRED));
    fmt::println(L"");
}

void DisplayCurrentMode(_In_ uint32_t const currentMode, _In_ bool const valueExistsInRegistry)
{
    fmt::println(L"{} {} {}",
        fmt::styled(internal::ResourceGetWString(IDS_LABEL_CURRENT_MODE), normalTextStyle),
        fmt::styled(currentMode, fmt::fg(fmt::color::golden_rod)),
        fmt::styled(GetModeName(currentMode), highlightTextStyle));

    if (!valueExistsInRegistry)
    {
        WriteNormal(internal::ResourceGetWString(IDS_MODE_NOT_SET_USING_DEFAULT));
    }

    fmt::println(L"");
}


bool TryParseRequestedMode(_In_ char* const parameter, _Out_ uint32_t& requestedMode)
{
    requestedMode = API_MODE_DEFAULT;

    try
    {
        auto value = std::stoi(std::string(parameter));

        if (value < API_MODE_MINIMUM || value > API_MODE_MAXIMUM)
        {
            return false;
        }

        requestedMode = static_cast<uint32_t>(value);

        return true;
    }
    catch (...)
    {
        return false;
    }
}


int __cdecl main(_In_ int argc, _In_ char* argv[])
{
    auto setModeResult = _setmode(_fileno(stdout), _O_U16TEXT);  // _O_WTEXT

    if (setModeResult == -1)
    {
        perror("Unable to set stdout to UTF-16 mode. ");
        return 1;
    }

    WriteDoubleSeparator();
    WriteInfo(internal::ResourceGetWString(IDS_BANNER_TOOL_INFO));
    WriteInfo(internal::ResourceGetWString(IDS_BANNER_COPYRIGHT));
    WriteInfo(internal::ResourceGetWString(IDS_BANNER_INFO_URL));
    WriteDoubleSeparator();
    WriteInfo(internal::ResourceGetWString(IDS_BANNER_DESCRIPTION));
    WriteDoubleSeparator();
    fmt::println(L"");

    uint32_t currentMode{ API_MODE_DEFAULT };
    bool valueExistsInRegistry{ false };

    if (!TryGetCurrentApiMode(currentMode, valueExistsInRegistry))
    {
        WriteError(internal::ResourceGetWString(IDS_ERROR_UNABLE_TO_READ_REGISTRY));
        return RETURN_REGISTRY_READ_FAILED;
    }

    uint32_t requestedMode{ API_MODE_DEFAULT };
    bool requestedModeProvided{ false };

    if (argc > 1)
    {
        requestedModeProvided = TryParseRequestedMode(argv[1], requestedMode);

        if (!requestedModeProvided)
        {
            WriteError(internal::ResourceGetWString(IDS_ERROR_INVALID_MODE_PARAMETER));
            fmt::println(L"");
        }
    }

    if (!requestedModeProvided)
    {
        DisplayHelp();
        DisplayCurrentMode(currentMode, valueExistsInRegistry);

        return argc > 1 ? RETURN_INVALID_MODE : RETURN_SUCCESS;
    }

    DisplayCurrentMode(currentMode, valueExistsInRegistry);

    if (requestedMode == currentMode && valueExistsInRegistry)
    {
        WriteInfo(internal::ResourceGetWString(IDS_MODE_ALREADY_SET));
        return RETURN_SUCCESS;
    }

    if (!IsRunningAsAdministrator())
    {
        WriteError(internal::ResourceGetWString(IDS_ERROR_NOT_ADMINISTRATOR));
        return RETURN_NOT_ADMINISTRATOR;
    }

    WriteInfo(internal::ResourceGetWString(IDS_LABEL_REQUESTED_MODE));
    DisplayMode(requestedMode);

    WritePrompt(internal::ResourceGetWString(IDS_PROMPT_CONFIRM_CHANGE));

    if (!PromptForYes())
    {
        fmt::println(L"");
        WriteInfo(internal::ResourceGetWString(IDS_STATUS_CANCELLED));

        return RETURN_SUCCESS;
    }

    fmt::println(L"");

    if (!TrySetCurrentApiMode(requestedMode))
    {
        WriteError(internal::ResourceGetWString(IDS_ERROR_UNABLE_TO_WRITE_REGISTRY));
        return RETURN_REGISTRY_WRITE_FAILED;
    }

    WriteHighlight(internal::ResourceGetWString(IDS_STATUS_MODE_CHANGED));
    fmt::println(L"");

    WritePrompt(internal::ResourceGetWString(IDS_PROMPT_CONFIRM_RESTART));

    if (PromptForYes())
    {
        fmt::println(L"");
        WriteWarning(internal::ResourceGetWString(IDS_STATUS_RESTARTING));

        if (!TryRestartComputer())
        {
            WriteError(internal::ResourceGetWString(IDS_ERROR_UNABLE_TO_RESTART));
            WriteWarning(internal::ResourceGetWString(IDS_STATUS_RESTART_LATER));
        }
    }
    else
    {
        fmt::println(L"");
        WriteWarning(internal::ResourceGetWString(IDS_STATUS_RESTART_LATER));
    }

    return RETURN_SUCCESS;
}
