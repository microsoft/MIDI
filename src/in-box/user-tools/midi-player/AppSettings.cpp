// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "AppSettings.h"

namespace midiplayer
{
    namespace
    {
        constexpr wchar_t SettingsKeyPath[] = LR"(Software\Microsoft\Windows MIDI Services\Tools\midiplayer)";

        constexpr wchar_t ValueLastEndpointDeviceId[] = L"LastEndpointDeviceId";
        constexpr wchar_t ValueGroupIndex[] = L"GroupIndex";
        constexpr wchar_t ValueRepeatQueue[] = L"RepeatQueue";
        constexpr wchar_t ValueShowQueue[] = L"ShowQueue";
        constexpr wchar_t ValueKeyboardView[] = L"KeyboardView";
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

        m_lastEndpointDeviceId = ReadString(ValueLastEndpointDeviceId, L"");

        auto const group = ReadDword(ValueGroupIndex, 0);
        m_groupIndex = group > 15 ? uint8_t{ 0 } : static_cast<uint8_t>(group);

        m_repeatQueue = ReadDword(ValueRepeatQueue, 0) != 0;
        m_showQueue = ReadDword(ValueShowQueue, 1) != 0;
        m_keyboardView = ReadDword(ValueKeyboardView, 0) != 0;
    }

    _Use_decl_annotations_
    void AppSettings::LastEndpointDeviceId(std::wstring const& value) noexcept
    {
        if (m_lastEndpointDeviceId == value)
        {
            return;
        }

        m_lastEndpointDeviceId = value;
        WriteString(ValueLastEndpointDeviceId, value);
    }

    void AppSettings::GroupIndex(uint8_t value) noexcept
    {
        if (value > 15 || m_groupIndex == value)
        {
            return;
        }

        m_groupIndex = value;
        WriteDword(ValueGroupIndex, value);
    }

    void AppSettings::RepeatQueue(bool value) noexcept
    {
        if (m_repeatQueue == value)
        {
            return;
        }

        m_repeatQueue = value;
        WriteDword(ValueRepeatQueue, value ? 1 : 0);
    }

    void AppSettings::ShowQueue(bool value) noexcept
    {
        if (m_showQueue == value)
        {
            return;
        }

        m_showQueue = value;
        WriteDword(ValueShowQueue, value ? 1 : 0);
    }

    void AppSettings::KeyboardView(bool value) noexcept
    {
        if (m_keyboardView == value)
        {
            return;
        }

        m_keyboardView = value;
        WriteDword(ValueKeyboardView, value ? 1 : 0);
    }
}
