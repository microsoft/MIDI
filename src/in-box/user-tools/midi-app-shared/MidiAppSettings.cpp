// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiAppSettings.h"

namespace midiapp
{
    namespace
    {
        constexpr wchar_t ValueTheme[] = L"Theme";
        constexpr wchar_t ValueBackdrop[] = L"WindowBackdrop";
        constexpr wchar_t ValueUseCustomBackgroundColor[] = L"UseCustomBackgroundColor";
        constexpr wchar_t ValueBackgroundColor[] = L"BackgroundColorArgb";
        constexpr wchar_t ValueAlwaysOnTop[] = L"AlwaysOnTop";
        constexpr wchar_t ValueWindowX[] = L"WindowX";
        constexpr wchar_t ValueWindowY[] = L"WindowY";
        constexpr wchar_t ValueWindowWidth[] = L"WindowWidth";
        constexpr wchar_t ValueWindowHeight[] = L"WindowHeight";
        constexpr wchar_t ValueWindowMaximized[] = L"WindowMaximized";
    }

    void MidiAppSettings::LoadShared() noexcept
    {
        auto const theme = ReadDword(ValueTheme, static_cast<uint32_t>(AppTheme::System));
        m_theme = (theme <= static_cast<uint32_t>(AppTheme::Dark))
            ? static_cast<AppTheme>(theme)
            : AppTheme::System;

        auto const backdrop = ReadDword(ValueBackdrop, static_cast<uint32_t>(WindowBackdrop::Solid));
        m_backdrop = (backdrop <= static_cast<uint32_t>(WindowBackdrop::Acrylic))
            ? static_cast<WindowBackdrop>(backdrop)
            : WindowBackdrop::Solid;

        m_useCustomBackgroundColor = ReadDword(ValueUseCustomBackgroundColor, 0) != 0;
        m_backgroundColorArgb = ReadDword(ValueBackgroundColor, DefaultBackgroundColorArgb);
        m_alwaysOnTop = ReadDword(ValueAlwaysOnTop, 0) != 0;

        m_windowPlacement.X = static_cast<int32_t>(ReadDword(ValueWindowX, 0));
        m_windowPlacement.Y = static_cast<int32_t>(ReadDword(ValueWindowY, 0));
        m_windowPlacement.Width = static_cast<int32_t>(ReadDword(ValueWindowWidth, 0));
        m_windowPlacement.Height = static_cast<int32_t>(ReadDword(ValueWindowHeight, 0));
        m_windowPlacement.Maximized = ReadDword(ValueWindowMaximized, 0) != 0;
        m_windowPlacement.Valid =
            m_windowPlacement.Width >= MinimumWindowWidth && m_windowPlacement.Height >= MinimumWindowHeight;
    }

    void MidiAppSettings::Theme(AppTheme value) noexcept
    {
        m_theme = value;
        WriteDword(ValueTheme, static_cast<uint32_t>(value));
    }

    void MidiAppSettings::Backdrop(WindowBackdrop value) noexcept
    {
        m_backdrop = value;
        WriteDword(ValueBackdrop, static_cast<uint32_t>(value));
    }

    void MidiAppSettings::UseCustomBackgroundColor(bool value) noexcept
    {
        m_useCustomBackgroundColor = value;
        WriteDword(ValueUseCustomBackgroundColor, value ? 1u : 0u);
    }

    void MidiAppSettings::BackgroundColorArgb(uint32_t value) noexcept
    {
        m_backgroundColorArgb = value;
        WriteDword(ValueBackgroundColor, value);
    }

    void MidiAppSettings::AlwaysOnTop(bool value) noexcept
    {
        m_alwaysOnTop = value;
        WriteDword(ValueAlwaysOnTop, value ? 1u : 0u);
    }

    void MidiAppSettings::WindowPlacement(WindowPlacementInfo const& value) noexcept
    {
        m_windowPlacement = value;
        m_windowPlacement.Valid = value.Width >= MinimumWindowWidth && value.Height >= MinimumWindowHeight;

        // a collapsed or zero sized window is not worth restoring to
        if (!m_windowPlacement.Valid)
        {
            return;
        }

        WriteDword(ValueWindowX, static_cast<uint32_t>(value.X));
        WriteDword(ValueWindowY, static_cast<uint32_t>(value.Y));
        WriteDword(ValueWindowWidth, static_cast<uint32_t>(value.Width));
        WriteDword(ValueWindowHeight, static_cast<uint32_t>(value.Height));
        WriteDword(ValueWindowMaximized, value.Maximized ? 1u : 0u);
    }

    void MidiAppSettings::WriteDword(std::wstring_view valueName, uint32_t value) const noexcept
    {
        HKEY raw{ nullptr };

        if (::RegCreateKeyExW(HKEY_CURRENT_USER, m_settingsKeyPath.c_str(), 0, nullptr,
            REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, nullptr, &raw, nullptr) != ERROR_SUCCESS)
        {
            return;
        }

        wil::unique_hkey key{ raw };

        ::RegSetValueExW(key.get(), std::wstring{ valueName }.c_str(), 0, REG_DWORD,
            reinterpret_cast<BYTE const*>(&value), sizeof(value));
    }

    void MidiAppSettings::WriteString(std::wstring_view valueName, std::wstring const& value) const noexcept
    {
        HKEY raw{ nullptr };

        if (::RegCreateKeyExW(HKEY_CURRENT_USER, m_settingsKeyPath.c_str(), 0, nullptr,
            REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, nullptr, &raw, nullptr) != ERROR_SUCCESS)
        {
            return;
        }

        wil::unique_hkey key{ raw };

        ::RegSetValueExW(key.get(), std::wstring{ valueName }.c_str(), 0, REG_SZ,
            reinterpret_cast<BYTE const*>(value.c_str()),
            static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t)));
    }

    uint32_t MidiAppSettings::ReadDword(std::wstring_view valueName, uint32_t defaultValue) const noexcept
    {
        HKEY raw{ nullptr };

        if (::RegOpenKeyExW(HKEY_CURRENT_USER, m_settingsKeyPath.c_str(), 0, KEY_QUERY_VALUE, &raw) != ERROR_SUCCESS)
        {
            return defaultValue;
        }

        wil::unique_hkey key{ raw };

        DWORD value{ 0 };
        DWORD size{ sizeof(value) };
        DWORD type{ 0 };

        if (::RegQueryValueExW(key.get(), std::wstring{ valueName }.c_str(), nullptr, &type,
            reinterpret_cast<BYTE*>(&value), &size) != ERROR_SUCCESS || type != REG_DWORD)
        {
            return defaultValue;
        }

        return value;
    }

    std::wstring MidiAppSettings::ReadString(std::wstring_view valueName, std::wstring const& defaultValue) const noexcept
    {
        HKEY raw{ nullptr };

        if (::RegOpenKeyExW(HKEY_CURRENT_USER, m_settingsKeyPath.c_str(), 0, KEY_QUERY_VALUE, &raw) != ERROR_SUCCESS)
        {
            return defaultValue;
        }

        wil::unique_hkey key{ raw };

        DWORD size{ 0 };
        DWORD type{ 0 };

        if (::RegQueryValueExW(key.get(), std::wstring{ valueName }.c_str(), nullptr, &type,
            nullptr, &size) != ERROR_SUCCESS || type != REG_SZ || size == 0)
        {
            return defaultValue;
        }

        std::wstring value(size / sizeof(wchar_t), L'\0');

        if (::RegQueryValueExW(key.get(), std::wstring{ valueName }.c_str(), nullptr, &type,
            reinterpret_cast<BYTE*>(value.data()), &size) != ERROR_SUCCESS)
        {
            return defaultValue;
        }

        while (!value.empty() && value.back() == L'\0')
        {
            value.pop_back();
        }

        return value;
    }
}
