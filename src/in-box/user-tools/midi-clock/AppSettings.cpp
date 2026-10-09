// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "AppSettings.h"

namespace midiclock
{
    namespace
    {
        constexpr wchar_t SettingsKeyPath[] = LR"(Software\Microsoft\Windows MIDI Services\Tools\midiclock)";
        constexpr wchar_t ValueShowRunningTimeCode[] = L"ShowRunningTimeCode";
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

        m_showRunningTimeCode = ReadDword(ValueShowRunningTimeCode, 0) != 0;
    }

    _Use_decl_annotations_
    void AppSettings::ShowRunningTimeCode(bool value) noexcept
    {
        if (m_showRunningTimeCode == value)
        {
            return;
        }

        m_showRunningTimeCode = value;
        WriteDword(ValueShowRunningTimeCode, value ? 1 : 0);
    }
}
