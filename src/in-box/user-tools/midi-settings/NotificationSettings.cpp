// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"

namespace midisettings
{
    _Use_decl_annotations_
    bool NotificationSettings::ReadFlag(PCWSTR const valueName, bool const defaultValue) noexcept
    {
        wil::unique_hkey key{};

        if (::RegOpenKeyExW(
                HKEY_CURRENT_USER,
                MIDI_NOTIFICATIONS_SETTINGS_REG_KEY,
                0,
                KEY_QUERY_VALUE,
                key.put()) != ERROR_SUCCESS)
        {
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

    _Use_decl_annotations_
    void NotificationSettings::WriteFlag(PCWSTR const valueName, bool const value) noexcept
    {
        wil::unique_hkey key{};

        if (::RegCreateKeyExW(
                HKEY_CURRENT_USER,
                MIDI_NOTIFICATIONS_SETTINGS_REG_KEY,
                0,
                nullptr,
                REG_OPTION_NON_VOLATILE,
                KEY_SET_VALUE,
                nullptr,
                key.put(),
                nullptr) != ERROR_SUCCESS)
        {
            return;
        }

        DWORD const data{ value ? 1u : 0u };

        LOG_IF_WIN32_ERROR(::RegSetValueExW(
            key.get(),
            valueName,
            0,
            REG_DWORD,
            reinterpret_cast<BYTE const*>(&data),
            sizeof(data)));
    }

    bool NotificationSettings::NotificationsEnabled() noexcept
    {
        return ReadFlag(MIDI_NOTIFICATIONS_VALUE_ENABLED, true);
    }

    _Use_decl_annotations_
    void NotificationSettings::NotificationsEnabled(bool const value) noexcept
    {
        WriteFlag(MIDI_NOTIFICATIONS_VALUE_ENABLED, value);
    }

    bool NotificationSettings::NetworkApprovalEnabled() noexcept
    {
        return ReadFlag(MIDI_NOTIFICATIONS_VALUE_NETWORK_APPROVAL, true);
    }

    _Use_decl_annotations_
    void NotificationSettings::NetworkApprovalEnabled(bool const value) noexcept
    {
        WriteFlag(MIDI_NOTIFICATIONS_VALUE_NETWORK_APPROVAL, value);
    }

    std::wstring NotificationSettings::AppPath() noexcept
    {
        try
        {
            wchar_t modulePath[MAX_PATH]{};

            if (::GetModuleFileNameW(nullptr, modulePath, ARRAYSIZE(modulePath)) == 0)
            {
                return {};
            }

            std::filesystem::path const path{ modulePath };
            std::error_code error{};

            // The tools are installed as siblings, one folder each.
            auto candidate =
                path.parent_path().parent_path() / L"Notifications" / L"midinotifications.exe";

            if (std::filesystem::exists(candidate, error))
            {
                return candidate.wstring();
            }

            // Development layout: out\<tool>\<platform>\<configuration>\<tool>.exe. Without this
            // every switch in the notifications dialog is disabled on a machine where the
            // installer has never run, which is every machine we develop on.
            auto const here = path.parent_path();

            candidate =
                here.parent_path().parent_path().parent_path() /
                L"midinotifications" / here.parent_path().filename() / here.filename() /
                L"midinotifications.exe";

            if (std::filesystem::exists(candidate, error))
            {
                return candidate.wstring();
            }

            return {};
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
            return {};
        }
    }

    bool NotificationSettings::StartsForAllUsers() noexcept
    {
        wil::unique_hkey key{};

        if (::RegOpenKeyExW(
                HKEY_LOCAL_MACHINE,
                MIDI_NOTIFICATIONS_RUN_REG_KEY,
                0,
                KEY_QUERY_VALUE,
                key.put()) != ERROR_SUCCESS)
        {
            return false;
        }

        return ::RegQueryValueExW(
            key.get(),
            MIDI_NOTIFICATIONS_RUN_VALUE_NAME,
            nullptr,
            nullptr,
            nullptr,
            nullptr) == ERROR_SUCCESS;
    }

    _Use_decl_annotations_
    bool NotificationSettings::TrySetStartsForAllUsers(bool const value) noexcept
    {
        return TrySetRunEntry(HKEY_LOCAL_MACHINE, value);
    }

    bool NotificationSettings::StartsAtSignIn() noexcept
    {
        wil::unique_hkey key{};

        if (::RegOpenKeyExW(
                HKEY_CURRENT_USER,
                MIDI_NOTIFICATIONS_RUN_REG_KEY,
                0,
                KEY_QUERY_VALUE,
                key.put()) != ERROR_SUCCESS)
        {
            return false;
        }

        return ::RegQueryValueExW(
            key.get(),
            MIDI_NOTIFICATIONS_RUN_VALUE_NAME,
            nullptr,
            nullptr,
            nullptr,
            nullptr) == ERROR_SUCCESS;
    }

    _Use_decl_annotations_
    bool NotificationSettings::TrySetStartsAtSignIn(bool const value) noexcept
    {
        return TrySetRunEntry(HKEY_CURRENT_USER, value);
    }

    _Use_decl_annotations_
    bool NotificationSettings::TrySetRunEntry(HKEY const root, bool const value) noexcept
    {
        wil::unique_hkey key{};

        // Opening HKLM for write is what fails for a standard user, and that is the signal the
        // caller turns into the offer to restart elevated.
        if (::RegCreateKeyExW(
                root,
                MIDI_NOTIFICATIONS_RUN_REG_KEY,
                0,
                nullptr,
                REG_OPTION_NON_VOLATILE,
                KEY_SET_VALUE,
                nullptr,
                key.put(),
                nullptr) != ERROR_SUCCESS)
        {
            return false;
        }

        if (!value)
        {
            auto const result = ::RegDeleteValueW(key.get(), MIDI_NOTIFICATIONS_RUN_VALUE_NAME);

            return result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND;
        }

        auto const path = AppPath();

        if (path.empty())
        {
            return false;
        }

        // Quoted, because the install path contains a space and Run splits on it.
        auto const value_ = L"\"" + path + L"\"";

        return ::RegSetValueExW(
            key.get(),
            MIDI_NOTIFICATIONS_RUN_VALUE_NAME,
            0,
            REG_SZ,
            reinterpret_cast<BYTE const*>(value_.c_str()),
            static_cast<DWORD>((value_.size() + 1) * sizeof(wchar_t))) == ERROR_SUCCESS;
    }

    void NotificationSettings::EnsureAppRunning() noexcept
    {
        auto const path = AppPath();

        if (path.empty())
        {
            return;
        }

        // Starting a copy which is already running is harmless: the app is single instance and
        // the second one exits immediately.
        ::ShellExecuteW(nullptr, L"open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    }
}
