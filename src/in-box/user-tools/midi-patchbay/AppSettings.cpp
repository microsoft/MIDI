// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "AppSettings.h"

namespace midipatchbay
{
    namespace
    {
        constexpr wchar_t SettingsKeyPath[] = LR"(Software\Microsoft\Windows MIDI Services\Tools\midipatchbay)";

        constexpr wchar_t ValueSortOrder[] = L"PatchSortOrder";
        constexpr wchar_t ValueMinimizeToNotificationArea[] = L"MinimizeToNotificationArea";
        constexpr wchar_t ValueStartMinimized[] = L"StartMinimized";
        constexpr wchar_t ValueWarnAboutLoops[] = L"WarnAboutLoops";
        constexpr wchar_t ValueConfirmCanvasRemove[] = L"ConfirmCanvasRemove";
        constexpr wchar_t ValueActivateSavedPatches[] = L"ActivateSavedPatchesAtStartup";

        constexpr wchar_t RunKeyPath[] = LR"(Software\Microsoft\Windows\CurrentVersion\Run)";
        constexpr wchar_t RunValueName[] = L"WindowsMidiServicesPatchbay";

        std::wstring CurrentExecutablePath() noexcept
        {
            std::wstring buffer(MAX_PATH, L'\0');

            for (;;)
            {
                auto const length = ::GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));

                if (length == 0)
                {
                    return {};
                }

                if (length < buffer.size())
                {
                    buffer.resize(length);
                    return buffer;
                }

                if (buffer.size() > 32768)
                {
                    return {};
                }

                buffer.resize(buffer.size() * 2);
            }
        }
    }

    AppSettings::AppSettings() noexcept :
        midiapp::MidiAppSettings(SettingsKeyPath)
    {
    }

    AppSettings& AppSettings::Current() noexcept
    {
        static AppSettings instance{};
        return instance;
    }

    void AppSettings::Load() noexcept
    {
        LoadShared();

        m_sortOrder = ReadDword(ValueSortOrder, 0) == 1 ? PatchSortOrder::Name : PatchSortOrder::Newest;
        m_minimizeToNotificationArea = ReadDword(ValueMinimizeToNotificationArea, 0) != 0;
        m_startMinimized = ReadDword(ValueStartMinimized, 0) != 0;
        m_warnAboutLoops = ReadDword(ValueWarnAboutLoops, 1) != 0;
        m_confirmCanvasRemove = ReadDword(ValueConfirmCanvasRemove, 1) != 0;
        m_activateSavedPatchesAtStartup = ReadDword(ValueActivateSavedPatches, 1) != 0;
    }

    _Use_decl_annotations_
    void AppSettings::SortOrder(PatchSortOrder value) noexcept
    {
        m_sortOrder = value;
        WriteDword(ValueSortOrder, value == PatchSortOrder::Name ? 1u : 0u);
    }

    _Use_decl_annotations_
    void AppSettings::MinimizeToNotificationArea(bool value) noexcept
    {
        m_minimizeToNotificationArea = value;
        WriteDword(ValueMinimizeToNotificationArea, value ? 1u : 0u);
    }

    _Use_decl_annotations_
    void AppSettings::StartMinimized(bool value) noexcept
    {
        m_startMinimized = value;
        WriteDword(ValueStartMinimized, value ? 1u : 0u);
    }

    _Use_decl_annotations_
    void AppSettings::WarnAboutLoops(bool value) noexcept
    {
        m_warnAboutLoops = value;
        WriteDword(ValueWarnAboutLoops, value ? 1u : 0u);
    }

    _Use_decl_annotations_
    void AppSettings::ConfirmCanvasRemove(bool value) noexcept
    {
        m_confirmCanvasRemove = value;
        WriteDword(ValueConfirmCanvasRemove, value ? 1u : 0u);
    }

    _Use_decl_annotations_
    void AppSettings::ActivateSavedPatchesAtStartup(bool value) noexcept
    {
        m_activateSavedPatchesAtStartup = value;
        WriteDword(ValueActivateSavedPatches, value ? 1u : 0u);
    }

    bool AppSettings::StartsWithWindows() noexcept
    {
        wil::unique_hkey key{};

        if (::RegOpenKeyExW(HKEY_CURRENT_USER, RunKeyPath, 0, KEY_QUERY_VALUE, key.put()) != ERROR_SUCCESS)
        {
            return false;
        }

        return ::RegQueryValueExW(key.get(), RunValueName, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS;
    }

    _Use_decl_annotations_
    bool AppSettings::TrySetStartsWithWindows(bool value) noexcept
    {
        wil::unique_hkey key{};

        if (::RegCreateKeyExW(
            HKEY_CURRENT_USER, RunKeyPath, 0, nullptr, 0,
            KEY_SET_VALUE, nullptr, key.put(), nullptr) != ERROR_SUCCESS)
        {
            return false;
        }

        if (!value)
        {
            auto const result = ::RegDeleteValueW(key.get(), RunValueName);
            return result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND;
        }

        auto const path = CurrentExecutablePath();

        if (path.empty())
        {
            return false;
        }

        // quoted, because the install path contains spaces and an unquoted Run entry would be
        // parsed as a different executable plus arguments
        auto const command = L'"' + path + L"\" --minimized";

        return ::RegSetValueExW(
            key.get(), RunValueName, 0, REG_SZ,
            reinterpret_cast<BYTE const*>(command.c_str()),
            static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t))) == ERROR_SUCCESS;
    }
}
