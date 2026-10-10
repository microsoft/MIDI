// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "AppSettings.h"

namespace midisequencer
{
    namespace
    {
        constexpr wchar_t SettingsKeyPath[] = LR"(Software\Microsoft\Windows MIDI Services\Tools\midisequencer)";

        constexpr wchar_t ValueMetronomeEnabled[] = L"MetronomeEnabled";
        constexpr wchar_t ValueMetronomeOnlyWhileRecording[] = L"MetronomeOnlyWhileRecording";
        constexpr wchar_t ValueMetronomeEndpointId[] = L"MetronomeEndpointId";
        constexpr wchar_t ValueMetronomeEndpointName[] = L"MetronomeEndpointName";
        constexpr wchar_t ValueMetronomeGroup[] = L"MetronomeGroup";
        constexpr wchar_t ValueMetronomeChannel[] = L"MetronomeChannel";
        constexpr wchar_t ValueMetronomeNote[] = L"MetronomeNote";
        constexpr wchar_t ValueShowLauncher[] = L"ShowLauncher";
        constexpr wchar_t ValueBarWidth[] = L"BarWidthTenths";
        constexpr wchar_t ValueSnapTicks[] = L"SnapTicks";
        constexpr wchar_t ValueLaunchQuantizeTicks[] = L"LaunchQuantizeTicks";
        constexpr wchar_t ValueSlotRecordBars[] = L"SlotRecordBars";
        constexpr wchar_t ValueEditorHeight[] = L"EditorHeight";
        constexpr wchar_t ValueValuesAs[] = L"ValuesAs";
        constexpr wchar_t ValueLastFolder[] = L"LastFolder";

        // Snap and launch values a person can pick. Anything else in the registry is ignored.
        bool IsKnownGrid(uint32_t ticks) noexcept
        {
            switch (ticks)
            {
            case 0: case 60: case 80: case 120: case 160: case 240: case 320: case 480: case 640:
            case 960: case 1920: case 3840: case 7680: case 15360:
                return true;
            default:
                return false;
            }
        }

        bool IsKnownBarCount(uint32_t bars) noexcept
        {
            return bars == 1 || bars == 2 || bars == 4 || bars == 8 || bars == 16;
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

        m_metronomeEnabled = ReadDword(ValueMetronomeEnabled, 0) != 0;
        m_metronomeOnlyWhileRecording = ReadDword(ValueMetronomeOnlyWhileRecording, 0) != 0;
        m_metronomeEndpointId = ReadString(ValueMetronomeEndpointId, L"");
        m_metronomeEndpointName = ReadString(ValueMetronomeEndpointName, L"");
        m_metronomeGroup = static_cast<uint8_t>(std::min<uint32_t>(ReadDword(ValueMetronomeGroup, 0), 15));
        m_metronomeChannel = static_cast<uint8_t>(std::min<uint32_t>(ReadDword(ValueMetronomeChannel, 9), 15));
        m_metronomeNote = static_cast<uint8_t>(std::min<uint32_t>(ReadDword(ValueMetronomeNote, 37), 127));
        m_showLauncher = ReadDword(ValueShowLauncher, 1) != 0;

        auto const tenths = ReadDword(ValueBarWidth, static_cast<uint32_t>(DefaultBarWidth * 10));
        m_barWidth = std::clamp(tenths / 10.0, MinimumBarWidth, MaximumBarWidth);

        auto const snap = ReadDword(ValueSnapTicks, 240);
        m_snapTicks = IsKnownGrid(snap) ? snap : 240;

        auto const launch = ReadDword(ValueLaunchQuantizeTicks, 3840);
        m_launchQuantizeTicks = IsKnownGrid(launch) ? launch : 3840;

        auto const bars = ReadDword(ValueSlotRecordBars, 4);
        m_slotRecordBars = IsKnownBarCount(bars) ? bars : 4;

        m_editorHeight = std::clamp(static_cast<double>(ReadDword(ValueEditorHeight, 300)), 120.0, 2000.0);

        auto const valuesAs = ReadDword(ValueValuesAs, 0);
        m_valuesAs = valuesAs <= 2 ? static_cast<ValueDisplay>(valuesAs) : ValueDisplay::Midi2;

        m_lastFolder = ReadString(ValueLastFolder, L"");
    }

    void AppSettings::MetronomeEnabled(bool value) noexcept
    {
        m_metronomeEnabled = value;
        WriteDword(ValueMetronomeEnabled, value ? 1 : 0);
    }

    void AppSettings::MetronomeOnlyWhileRecording(bool value) noexcept
    {
        m_metronomeOnlyWhileRecording = value;
        WriteDword(ValueMetronomeOnlyWhileRecording, value ? 1 : 0);
    }

    _Use_decl_annotations_
    void AppSettings::MetronomeEndpoint(std::wstring const& id, std::wstring const& name) noexcept
    {
        m_metronomeEndpointId = id;
        m_metronomeEndpointName = name;
        WriteString(ValueMetronomeEndpointId, id);
        WriteString(ValueMetronomeEndpointName, name);
    }

    void AppSettings::MetronomeGroup(uint8_t value) noexcept
    {
        m_metronomeGroup = std::min<uint8_t>(value, 15);
        WriteDword(ValueMetronomeGroup, m_metronomeGroup);
    }

    void AppSettings::MetronomeChannel(uint8_t value) noexcept
    {
        m_metronomeChannel = std::min<uint8_t>(value, 15);
        WriteDword(ValueMetronomeChannel, m_metronomeChannel);
    }

    void AppSettings::MetronomeNote(uint8_t value) noexcept
    {
        m_metronomeNote = std::min<uint8_t>(value, 127);
        WriteDword(ValueMetronomeNote, m_metronomeNote);
    }

    void AppSettings::ShowLauncher(bool value) noexcept
    {
        m_showLauncher = value;
        WriteDword(ValueShowLauncher, value ? 1 : 0);
    }

    void AppSettings::BarWidth(double value) noexcept
    {
        m_barWidth = std::clamp(value, MinimumBarWidth, MaximumBarWidth);
        WriteDword(ValueBarWidth, static_cast<uint32_t>(std::lround(m_barWidth * 10)));
    }

    void AppSettings::SnapTicks(uint32_t value) noexcept
    {
        if (IsKnownGrid(value))
        {
            m_snapTicks = value;
            WriteDword(ValueSnapTicks, value);
        }
    }

    void AppSettings::LaunchQuantizeTicks(uint32_t value) noexcept
    {
        if (IsKnownGrid(value))
        {
            m_launchQuantizeTicks = value;
            WriteDword(ValueLaunchQuantizeTicks, value);
        }
    }

    void AppSettings::SlotRecordBars(uint32_t value) noexcept
    {
        if (IsKnownBarCount(value))
        {
            m_slotRecordBars = value;
            WriteDword(ValueSlotRecordBars, value);
        }
    }

    void AppSettings::EditorHeight(double value) noexcept
    {
        m_editorHeight = std::clamp(value, 120.0, 2000.0);
        WriteDword(ValueEditorHeight, static_cast<uint32_t>(std::lround(m_editorHeight)));
    }

    void AppSettings::ValuesAs(ValueDisplay value) noexcept
    {
        m_valuesAs = value;
        WriteDword(ValueValuesAs, static_cast<uint32_t>(value));
    }

    _Use_decl_annotations_
    void AppSettings::LastFolder(std::wstring const& value) noexcept
    {
        m_lastFolder = value;
        WriteString(ValueLastFolder, value);
    }
}
