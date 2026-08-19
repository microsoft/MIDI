// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "AppSettings.h"

namespace midi2monitor
{
    namespace
    {
        constexpr wchar_t SettingsKeyPath[] = LR"(Software\Microsoft\Windows MIDI Services\Tools\midi2monitor)";

        constexpr wchar_t ValueShowClock[] = L"ShowClockMessages";
        constexpr wchar_t ValueShowActiveSense[] = L"ShowActiveSenseMessages";
        constexpr wchar_t ValueAlwaysOnTop[] = L"AlwaysOnTop";
        constexpr wchar_t ValueTimestampFormat[] = L"TimestampFormat";
        constexpr wchar_t ValueTheme[] = L"Theme";
        constexpr wchar_t ValueRetainedMessageCount[] = L"RetainedMessageCount";
        constexpr wchar_t ValueShowChiclets[] = L"ShowMessageNameChiclets";
        constexpr wchar_t ValueAutoHideColumns[] = L"AutoHideColumnsWhenNarrow";
        constexpr wchar_t ValueColumnLayout[] = L"ColumnLayout";
        constexpr wchar_t ValueTableZoomPercent[] = L"TableZoomPercent";
        constexpr wchar_t ValueWindowX[] = L"WindowX";
        constexpr wchar_t ValueWindowY[] = L"WindowY";
        constexpr wchar_t ValueWindowWidth[] = L"WindowWidth";
        constexpr wchar_t ValueWindowHeight[] = L"WindowHeight";
        constexpr wchar_t ValueWindowMaximized[] = L"WindowMaximized";

        wil::unique_hkey OpenSettingsKeyForWrite() noexcept
        {
            HKEY raw{ nullptr };

            if (::RegCreateKeyExW(HKEY_CURRENT_USER, SettingsKeyPath, 0, nullptr,
                REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, nullptr, &raw, nullptr) != ERROR_SUCCESS)
            {
                return {};
            }

            return wil::unique_hkey{ raw };
        }

        wil::unique_hkey OpenSettingsKeyForRead() noexcept
        {
            HKEY raw{ nullptr };

            if (::RegOpenKeyExW(HKEY_CURRENT_USER, SettingsKeyPath, 0, KEY_QUERY_VALUE, &raw) != ERROR_SUCCESS)
            {
                return {};
            }

            return wil::unique_hkey{ raw };
        }
    }

    AppSettings& AppSettings::Current() noexcept
    {
        static AppSettings instance{};
        return instance;
    }

    void AppSettings::Load() noexcept
    {
        m_showClockMessages = ReadDword(ValueShowClock, 0) != 0;
        m_showActiveSenseMessages = ReadDword(ValueShowActiveSense, 0) != 0;
        m_alwaysOnTop = ReadDword(ValueAlwaysOnTop, 0) != 0;
        m_showMessageNameChiclets = ReadDword(ValueShowChiclets, 1) != 0;
        m_autoHideColumnsWhenNarrow = ReadDword(ValueAutoHideColumns, 1) != 0;

        auto const format = ReadDword(ValueTimestampFormat, static_cast<uint32_t>(TimestampDisplayFormat::Ticks));
        m_timestampFormat = (format <= static_cast<uint32_t>(TimestampDisplayFormat::Seconds))
            ? static_cast<TimestampDisplayFormat>(format)
            : TimestampDisplayFormat::Ticks;

        auto const theme = ReadDword(ValueTheme, static_cast<uint32_t>(AppTheme::System));
        m_theme = (theme <= static_cast<uint32_t>(AppTheme::Dark))
            ? static_cast<AppTheme>(theme)
            : AppTheme::System;

        auto const retained = ReadDword(ValueRetainedMessageCount, DefaultRetainedMessageCount);
        m_retainedMessageCount = std::clamp(retained, MinimumRetainedMessageCount, MaximumRetainedMessageCount);

        m_columnLayout = ReadString(ValueColumnLayout, L"");

        auto const zoom = ReadDword(ValueTableZoomPercent, DefaultZoomPercent);
        m_tableZoomPercent = std::clamp(zoom, MinimumZoomPercent, MaximumZoomPercent);

        m_windowPlacement.X = static_cast<int32_t>(ReadDword(ValueWindowX, 0));
        m_windowPlacement.Y = static_cast<int32_t>(ReadDword(ValueWindowY, 0));
        m_windowPlacement.Width = static_cast<int32_t>(ReadDword(ValueWindowWidth, 0));
        m_windowPlacement.Height = static_cast<int32_t>(ReadDword(ValueWindowHeight, 0));
        m_windowPlacement.Maximized = ReadDword(ValueWindowMaximized, 0) != 0;
        m_windowPlacement.Valid =
            m_windowPlacement.Width >= MinimumWindowWidth && m_windowPlacement.Height >= MinimumWindowHeight;
    }

    _Use_decl_annotations_
    void AppSettings::WindowPlacement(WindowPlacementInfo const& value) noexcept
    {
        m_windowPlacement = value;
        m_windowPlacement.Valid = value.Width >= MinimumWindowWidth && value.Height >= MinimumWindowHeight;

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

    void AppSettings::TableZoomPercent(uint32_t value) noexcept
    {
        m_tableZoomPercent = std::clamp(value, MinimumZoomPercent, MaximumZoomPercent);
        WriteDword(ValueTableZoomPercent, m_tableZoomPercent);
    }

    void AppSettings::ShowClockMessages(bool value) noexcept
    {
        m_showClockMessages = value;
        WriteDword(ValueShowClock, value ? 1u : 0u);
    }

    void AppSettings::ShowActiveSenseMessages(bool value) noexcept
    {
        m_showActiveSenseMessages = value;
        WriteDword(ValueShowActiveSense, value ? 1u : 0u);
    }

    void AppSettings::AlwaysOnTop(bool value) noexcept
    {
        m_alwaysOnTop = value;
        WriteDword(ValueAlwaysOnTop, value ? 1u : 0u);
    }

    void AppSettings::ShowMessageNameChiclets(bool value) noexcept
    {
        m_showMessageNameChiclets = value;
        WriteDword(ValueShowChiclets, value ? 1u : 0u);
    }

    void AppSettings::AutoHideColumnsWhenNarrow(bool value) noexcept
    {
        m_autoHideColumnsWhenNarrow = value;
        WriteDword(ValueAutoHideColumns, value ? 1u : 0u);
    }

    void AppSettings::TimestampFormat(TimestampDisplayFormat value) noexcept
    {
        m_timestampFormat = value;
        WriteDword(ValueTimestampFormat, static_cast<uint32_t>(value));
    }

    void AppSettings::Theme(AppTheme value) noexcept
    {
        m_theme = value;
        WriteDword(ValueTheme, static_cast<uint32_t>(value));
    }

    void AppSettings::RetainedMessageCount(uint32_t value) noexcept
    {
        m_retainedMessageCount = std::clamp(value, MinimumRetainedMessageCount, MaximumRetainedMessageCount);
        WriteDword(ValueRetainedMessageCount, m_retainedMessageCount);
    }

    void AppSettings::ColumnLayout(std::wstring const& value) noexcept
    {
        m_columnLayout = value;
        WriteString(ValueColumnLayout, value);
    }

    _Use_decl_annotations_
    void AppSettings::WriteDword(std::wstring_view valueName, uint32_t value) const noexcept
    {
        auto key = OpenSettingsKeyForWrite();

        if (!key)
        {
            return;
        }

        std::wstring const name{ valueName };
        DWORD const data{ value };

        LOG_IF_WIN32_ERROR(::RegSetValueExW(key.get(), name.c_str(), 0, REG_DWORD,
            reinterpret_cast<BYTE const*>(&data), sizeof(data)));
    }

    _Use_decl_annotations_
    void AppSettings::WriteString(std::wstring_view valueName, std::wstring const& value) const noexcept
    {
        auto key = OpenSettingsKeyForWrite();

        if (!key)
        {
            return;
        }

        std::wstring const name{ valueName };
        auto const byteCount = static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t));

        LOG_IF_WIN32_ERROR(::RegSetValueExW(key.get(), name.c_str(), 0, REG_SZ,
            reinterpret_cast<BYTE const*>(value.c_str()), byteCount));
    }

    _Use_decl_annotations_
    uint32_t AppSettings::ReadDword(std::wstring_view valueName, uint32_t defaultValue) const noexcept
    {
        auto key = OpenSettingsKeyForRead();

        if (!key)
        {
            return defaultValue;
        }

        std::wstring const name{ valueName };
        DWORD data{ 0 };
        DWORD size{ sizeof(data) };
        DWORD type{ 0 };

        if (::RegQueryValueExW(key.get(), name.c_str(), nullptr, &type,
            reinterpret_cast<BYTE*>(&data), &size) != ERROR_SUCCESS || type != REG_DWORD)
        {
            return defaultValue;
        }

        return data;
    }

    _Use_decl_annotations_
    std::wstring AppSettings::ReadString(std::wstring_view valueName, std::wstring const& defaultValue) const noexcept
    {
        auto key = OpenSettingsKeyForRead();

        if (!key)
        {
            return defaultValue;
        }

        std::wstring const name{ valueName };
        DWORD size{ 0 };
        DWORD type{ 0 };

        if (::RegQueryValueExW(key.get(), name.c_str(), nullptr, &type, nullptr, &size) != ERROR_SUCCESS ||
            type != REG_SZ || size == 0 || size > 8192)
        {
            return defaultValue;
        }

        std::wstring buffer(size / sizeof(wchar_t), L'\0');

        if (::RegQueryValueExW(key.get(), name.c_str(), nullptr, &type,
            reinterpret_cast<BYTE*>(buffer.data()), &size) != ERROR_SUCCESS)
        {
            return defaultValue;
        }

        auto const terminator = buffer.find(L'\0');

        if (terminator != std::wstring::npos)
        {
            buffer.resize(terminator);
        }

        return buffer;
    }
}
