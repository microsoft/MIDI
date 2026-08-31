// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "AppSettings.h"

namespace midiscratchpad
{
    namespace
    {
        constexpr wchar_t SettingsKeyPath[] = LR"(Software\Microsoft\Windows MIDI Services\Tools\midiscratchpad)";

        constexpr wchar_t ValueMode[] = L"Mode";
        constexpr wchar_t ValueAllowRunningStatus[] = L"AllowRunningStatus";
        constexpr wchar_t ValueShowSendPreview[] = L"ShowSendPreview";
        constexpr wchar_t ValueEditorFontPercent[] = L"EditorFontPercent";
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

        auto const mode = ReadDword(ValueMode, static_cast<uint32_t>(ScratchPadMode::Midi1Bytes));
        m_mode = (mode <= static_cast<uint32_t>(ScratchPadMode::UmpWords))
            ? static_cast<ScratchPadMode>(mode)
            : ScratchPadMode::Midi1Bytes;

        m_allowRunningStatus = ReadDword(ValueAllowRunningStatus, 0) != 0;
        m_showSendPreview = ReadDword(ValueShowSendPreview, 0) != 0;

        auto const font = ReadDword(ValueEditorFontPercent, DefaultEditorFontPercent);
        m_editorFontPercent = std::clamp(font, MinimumEditorFontPercent, MaximumEditorFontPercent);
    }

    void AppSettings::Mode(ScratchPadMode value) noexcept
    {
        m_mode = value;
        WriteDword(ValueMode, static_cast<uint32_t>(value));
    }

    void AppSettings::AllowRunningStatus(bool value) noexcept
    {
        m_allowRunningStatus = value;
        WriteDword(ValueAllowRunningStatus, value ? 1u : 0u);
    }

    void AppSettings::ShowSendPreview(bool value) noexcept
    {
        m_showSendPreview = value;
        WriteDword(ValueShowSendPreview, value ? 1u : 0u);
    }

    void AppSettings::EditorFontPercent(uint32_t value) noexcept
    {
        m_editorFontPercent = std::clamp(value, MinimumEditorFontPercent, MaximumEditorFontPercent);
        WriteDword(ValueEditorFontPercent, m_editorFontPercent);
    }
}
