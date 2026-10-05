// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "AppSettings.h"

#include "AudioOutput.h"

namespace midisoundfontsynth
{
    namespace
    {
        constexpr wchar_t SettingsKeyPath[] = LR"(Software\Microsoft\Windows MIDI Services\Tools\midisoundfontsynth)";

        constexpr wchar_t ValueAudioDeviceId[] = L"AudioDeviceId";
        constexpr wchar_t ValueExclusiveMode[] = L"AudioExclusiveMode";
        constexpr wchar_t ValueExclusiveBuffer[] = L"AudioExclusiveBufferMilliseconds";
        constexpr wchar_t ValueMinimizeToNotificationArea[] = L"MinimizeToNotificationArea";
        constexpr wchar_t ValueStartMinimized[] = L"StartMinimized";
        constexpr wchar_t ValueLastSoundFontFolder[] = L"LastSoundFontFolder";

        // The registry is per user and writable by the user, so what comes back is checked
        // rather than trusted to be something this app wrote.
        constexpr size_t MaximumStoredPathLength = 32767;
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

        m_audioDeviceId = ReadString(ValueAudioDeviceId, {});

        if (m_audioDeviceId.size() > MaximumStoredPathLength)
        {
            m_audioDeviceId.clear();
        }

        m_exclusiveMode = ReadDword(ValueExclusiveMode, 0) != 0;

        m_exclusiveBufferMilliseconds = (std::clamp)(
            ReadDword(ValueExclusiveBuffer, 10),
            SoundFontSynth::AudioOutputSettings::MinimumExclusiveBufferMilliseconds,
            SoundFontSynth::AudioOutputSettings::MaximumExclusiveBufferMilliseconds);

        m_minimizeToNotificationArea = ReadDword(ValueMinimizeToNotificationArea, 0) != 0;
        m_startMinimized = ReadDword(ValueStartMinimized, 0) != 0;

        m_lastSoundFontFolder = ReadString(ValueLastSoundFontFolder, {});

        if (m_lastSoundFontFolder.size() > MaximumStoredPathLength)
        {
            m_lastSoundFontFolder.clear();
        }
    }

    _Use_decl_annotations_
    void AppSettings::AudioDeviceId(std::wstring const& value) noexcept
    {
        try
        {
            m_audioDeviceId = value;
            WriteString(ValueAudioDeviceId, value);
        }
        catch (...)
        {
        }
    }

    _Use_decl_annotations_
    void AppSettings::ExclusiveMode(bool value) noexcept
    {
        m_exclusiveMode = value;
        WriteDword(ValueExclusiveMode, value ? 1u : 0u);
    }

    _Use_decl_annotations_
    void AppSettings::ExclusiveBufferMilliseconds(uint32_t value) noexcept
    {
        m_exclusiveBufferMilliseconds = (std::clamp)(
            value,
            SoundFontSynth::AudioOutputSettings::MinimumExclusiveBufferMilliseconds,
            SoundFontSynth::AudioOutputSettings::MaximumExclusiveBufferMilliseconds);

        WriteDword(ValueExclusiveBuffer, m_exclusiveBufferMilliseconds);
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
    void AppSettings::LastSoundFontFolder(std::wstring const& value) noexcept
    {
        try
        {
            m_lastSoundFontFolder = value;
            WriteString(ValueLastSoundFontFolder, value);
        }
        catch (...)
        {
        }
    }
}
