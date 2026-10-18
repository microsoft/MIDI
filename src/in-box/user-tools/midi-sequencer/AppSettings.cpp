// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "AppSettings.h"

#include "MidiTimeCode.h"

#include <winrt/Windows.Data.Json.h>

namespace json = winrt::Windows::Data::Json;

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
        constexpr wchar_t ValueShowMixer[] = L"ShowMixer";
        constexpr wchar_t ValueBarWidth[] = L"BarWidthTenths";
        constexpr wchar_t ValueSnapTicks[] = L"SnapTicks";
        constexpr wchar_t ValueLaunchQuantizeTicks[] = L"LaunchQuantizeTicks";
        constexpr wchar_t ValueSlotRecordBars[] = L"SlotRecordBars";
        constexpr wchar_t ValueEditorHeight[] = L"EditorHeight";
        constexpr wchar_t ValueHeaderWidth[] = L"HeaderWidth";
        constexpr wchar_t ValueLauncherWidth[] = L"LauncherWidth";
        constexpr wchar_t ValueValuesAs[] = L"ValuesAs";
        constexpr wchar_t ValueLastFolder[] = L"LastFolder";
        constexpr wchar_t ValueApplyDeviceOffsets[] = L"ApplyDeviceOffsets";
        constexpr wchar_t ValueClockOutputs[] = L"ClockOutputs";
        constexpr wchar_t ValueTimeCode[] = L"TimeCode";
        constexpr wchar_t ValueFollowClock[] = L"FollowClock";
        constexpr wchar_t ValueClockSendsTempo[] = L"ClockSendsTempo";
        constexpr wchar_t ValueRemoteControlEnabled[] = L"RemoteControlEnabled";
        constexpr wchar_t ValuePortsOff[] = L"PortsOff";
        constexpr wchar_t ValueRecentSequences[] = L"RecentSequences";

        // Text read back from the registry is capped like anything else read from outside.
        constexpr size_t MaximumSettingText = 1024;

        std::wstring CappedString(_In_ json::JsonObject const& o, _In_ wchar_t const* key)
        {
            if (!o.HasKey(key) || o.Lookup(key).ValueType() != json::JsonValueType::String)
            {
                return {};
            }

            std::wstring text{ o.Lookup(key).GetString() };
            text.resize(std::min(text.size(), MaximumSettingText));
            return text;
        }

        double NumberOr(_In_ json::JsonObject const& o, _In_ wchar_t const* key, _In_ double fallback)
        {
            if (!o.HasKey(key) || o.Lookup(key).ValueType() != json::JsonValueType::Number)
            {
                return fallback;
            }

            auto const value = o.Lookup(key).GetNumber();
            return std::isfinite(value) ? value : fallback;
        }

        bool BoolOr(_In_ json::JsonObject const& o, _In_ wchar_t const* key, _In_ bool fallback)
        {
            return o.HasKey(key) && o.Lookup(key).ValueType() == json::JsonValueType::Boolean ? o.Lookup(key).GetBoolean() : fallback;
        }

        std::vector<ClockOutputSetting> ParseClockOutputs(_In_ std::wstring const& text, _In_ bool tempoByDefault) noexcept
        {
            std::vector<ClockOutputSetting> outputs{};

            try
            {
                json::JsonArray array{ nullptr };

                if (text.empty() || !json::JsonArray::TryParse(text, array))
                {
                    return outputs;
                }

                for (auto const& value : array)
                {
                    // An array holds values: check the type, then ask for the object.
                    if (value.ValueType() != json::JsonValueType::Object)
                    {
                        continue;
                    }

                    auto const o = value.GetObject();

                    ClockOutputSetting output{};
                    output.EndpointId = CappedString(o, L"id");
                    output.EndpointName = CappedString(o, L"name");
                    output.Group = static_cast<uint8_t>(std::clamp(NumberOr(o, L"group", 0), 0.0, 15.0));
                    output.Clock = BoolOr(o, L"clock", true);
                    output.StartStop = BoolOr(o, L"startStop", true);
                    output.SongPosition = BoolOr(o, L"songPosition", true);

                    // Saved before tempo was a choice for each device, the one choice for all.
                    output.Tempo = BoolOr(o, L"tempo", tempoByDefault);
                    output.OffsetMilliseconds = static_cast<int32_t>(std::clamp(NumberOr(o, L"offset", 0),
                        -static_cast<double>(AppSettings::MaximumClockOffsetMilliseconds), static_cast<double>(AppSettings::MaximumClockOffsetMilliseconds)));

                    if (!output.EndpointId.empty() || !output.EndpointName.empty())
                    {
                        outputs.push_back(std::move(output));
                    }

                    if (outputs.size() >= AppSettings::MaximumClockOutputs)
                    {
                        break;
                    }
                }
            }
            catch (...)
            {
                outputs.clear();
            }

            return outputs;
        }

        std::wstring WriteClockOutputs(_In_ std::vector<ClockOutputSetting> const& outputs)
        {
            json::JsonArray array{};

            for (auto const& output : outputs)
            {
                json::JsonObject o{};
                o.SetNamedValue(L"id", json::JsonValue::CreateStringValue(output.EndpointId));
                o.SetNamedValue(L"name", json::JsonValue::CreateStringValue(output.EndpointName));
                o.SetNamedValue(L"group", json::JsonValue::CreateNumberValue(output.Group));
                o.SetNamedValue(L"clock", json::JsonValue::CreateBooleanValue(output.Clock));
                o.SetNamedValue(L"startStop", json::JsonValue::CreateBooleanValue(output.StartStop));
                o.SetNamedValue(L"songPosition", json::JsonValue::CreateBooleanValue(output.SongPosition));
                o.SetNamedValue(L"tempo", json::JsonValue::CreateBooleanValue(output.Tempo));
                o.SetNamedValue(L"offset", json::JsonValue::CreateNumberValue(output.OffsetMilliseconds));
                array.Append(o);
            }

            return std::wstring{ array.Stringify() };
        }

        std::vector<PortSetting> ParsePortsOff(_In_ std::wstring const& text) noexcept
        {
            std::vector<PortSetting> ports{};

            try
            {
                json::JsonArray array{ nullptr };

                if (text.empty() || !json::JsonArray::TryParse(text, array))
                {
                    return ports;
                }

                for (auto const& value : array)
                {
                    // An array holds values: check the type, then ask for the object.
                    if (value.ValueType() != json::JsonValueType::Object)
                    {
                        continue;
                    }

                    auto const o = value.GetObject();

                    PortSetting port{};
                    port.EndpointId = CappedString(o, L"id");
                    port.EndpointName = CappedString(o, L"name");
                    port.Group = static_cast<uint8_t>(std::clamp(NumberOr(o, L"group", 0), 0.0, 15.0));
                    port.PlayTo = BoolOr(o, L"playTo", true);
                    port.RecordFrom = BoolOr(o, L"recordFrom", true);

                    // One with nothing turned off says nothing.
                    if ((!port.EndpointId.empty() || !port.EndpointName.empty()) && (!port.PlayTo || !port.RecordFrom))
                    {
                        ports.push_back(std::move(port));
                    }

                    if (ports.size() >= AppSettings::MaximumPortsOff)
                    {
                        break;
                    }
                }
            }
            catch (...)
            {
                ports.clear();
            }

            return ports;
        }

        std::wstring WritePortsOff(_In_ std::vector<PortSetting> const& ports)
        {
            json::JsonArray array{};

            for (auto const& port : ports)
            {
                json::JsonObject o{};
                o.SetNamedValue(L"id", json::JsonValue::CreateStringValue(port.EndpointId));
                o.SetNamedValue(L"name", json::JsonValue::CreateStringValue(port.EndpointName));
                o.SetNamedValue(L"group", json::JsonValue::CreateNumberValue(port.Group));
                o.SetNamedValue(L"playTo", json::JsonValue::CreateBooleanValue(port.PlayTo));
                o.SetNamedValue(L"recordFrom", json::JsonValue::CreateBooleanValue(port.RecordFrom));
                array.Append(o);
            }

            return std::wstring{ array.Stringify() };
        }

        TimeCodeSetting ParseTimeCode(_In_ std::wstring const& text) noexcept
        {
            TimeCodeSetting setting{};

            try
            {
                json::JsonObject o{ nullptr };

                if (text.empty() || !json::JsonObject::TryParse(text, o))
                {
                    return setting;
                }

                setting.EndpointId = CappedString(o, L"id");
                setting.EndpointName = CappedString(o, L"name");
                setting.Enabled = BoolOr(o, L"enabled", !setting.EndpointId.empty() || !setting.EndpointName.empty());
                setting.Group = static_cast<uint8_t>(std::clamp(NumberOr(o, L"group", 0), 0.0, 15.0));

                auto const rate = static_cast<int32_t>(std::clamp(NumberOr(o, L"rate", 3), 0.0, 3.0));
                setting.Rate = midiapp::IsValidFrameRate(rate) ? rate : 3;

                midiapp::MidiTimeCodePosition start{};
                auto const frameRate = midiapp::FrameRateFromValue(setting.Rate);

                if (midiapp::TryParsePosition(CappedString(o, L"start"), frameRate, start))
                {
                    setting.Start = midiapp::FormatPosition(midiapp::ClampPosition(start, frameRate), frameRate);
                }
            }
            catch (...)
            {
                setting = TimeCodeSetting{};
            }

            return setting;
        }

        std::wstring WriteTimeCode(_In_ TimeCodeSetting const& setting)
        {
            json::JsonObject o{};
            o.SetNamedValue(L"enabled", json::JsonValue::CreateBooleanValue(setting.Enabled));
            o.SetNamedValue(L"id", json::JsonValue::CreateStringValue(setting.EndpointId));
            o.SetNamedValue(L"name", json::JsonValue::CreateStringValue(setting.EndpointName));
            o.SetNamedValue(L"group", json::JsonValue::CreateNumberValue(setting.Group));
            o.SetNamedValue(L"rate", json::JsonValue::CreateNumberValue(setting.Rate));
            o.SetNamedValue(L"start", json::JsonValue::CreateStringValue(setting.Start));
            return std::wstring{ o.Stringify() };
        }

        FollowClockSetting ParseFollowClock(_In_ std::wstring const& text) noexcept
        {
            FollowClockSetting setting{};

            try
            {
                json::JsonObject o{ nullptr };

                if (text.empty() || !json::JsonObject::TryParse(text, o))
                {
                    return setting;
                }

                setting.EndpointId = CappedString(o, L"id");
                setting.EndpointName = CappedString(o, L"name");
                setting.Enabled = BoolOr(o, L"enabled", false);
                setting.Group = static_cast<uint8_t>(std::clamp(NumberOr(o, L"group", 0), 0.0, 15.0));
            }
            catch (...)
            {
                setting = FollowClockSetting{};
            }

            return setting;
        }

        std::wstring WriteFollowClock(_In_ FollowClockSetting const& setting)
        {
            json::JsonObject o{};
            o.SetNamedValue(L"enabled", json::JsonValue::CreateBooleanValue(setting.Enabled));
            o.SetNamedValue(L"id", json::JsonValue::CreateStringValue(setting.EndpointId));
            o.SetNamedValue(L"name", json::JsonValue::CreateStringValue(setting.EndpointName));
            o.SetNamedValue(L"group", json::JsonValue::CreateNumberValue(setting.Group));
            return std::wstring{ o.Stringify() };
        }

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
        m_showMixer = ReadDword(ValueShowMixer, 0) != 0;

        auto const tenths = ReadDword(ValueBarWidth, static_cast<uint32_t>(DefaultBarWidth * 10));
        m_barWidth = std::clamp(tenths / 10.0, MinimumBarWidth, MaximumBarWidth);

        auto const snap = ReadDword(ValueSnapTicks, 240);
        m_snapTicks = IsKnownGrid(snap) ? snap : 240;

        auto const launch = ReadDword(ValueLaunchQuantizeTicks, 3840);
        m_launchQuantizeTicks = IsKnownGrid(launch) ? launch : 3840;

        auto const bars = ReadDword(ValueSlotRecordBars, 4);
        m_slotRecordBars = IsKnownBarCount(bars) ? bars : 4;

        m_editorHeight = std::clamp(static_cast<double>(ReadDword(ValueEditorHeight, 300)), 120.0, 2000.0);
        m_headerWidth = std::clamp(static_cast<double>(ReadDword(ValueHeaderWidth, static_cast<uint32_t>(DefaultHeaderWidth))), MinimumHeaderWidth, MaximumHeaderWidth);
        m_launcherWidth = std::clamp(static_cast<double>(ReadDword(ValueLauncherWidth, static_cast<uint32_t>(DefaultLauncherWidth))), MinimumLauncherWidth, MaximumLauncherWidth);

        auto const valuesAs = ReadDword(ValueValuesAs, 0);
        m_valuesAs = valuesAs <= 2 ? static_cast<ValueDisplay>(valuesAs) : ValueDisplay::Midi2;

        m_lastFolder = ReadString(ValueLastFolder, L"");
        m_applyDeviceOffsets = ReadDword(ValueApplyDeviceOffsets, 1) != 0;

        // Read first: it's what devices saved before tempo was a choice for each one get.
        m_clockSendsTempo = ReadDword(ValueClockSendsTempo, 1) != 0;
        m_clockOutputs = ParseClockOutputs(ReadString(ValueClockOutputs, L""), m_clockSendsTempo);
        m_portsOff = ParsePortsOff(ReadString(ValuePortsOff, L""));
        m_timeCode = ParseTimeCode(ReadString(ValueTimeCode, L""));
        m_followClock = ParseFollowClock(ReadString(ValueFollowClock, L""));
        m_remoteControlEnabled = ReadDword(ValueRemoteControlEnabled, 0) != 0;
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

    void AppSettings::ShowMixer(bool value) noexcept
    {
        m_showMixer = value;
        WriteDword(ValueShowMixer, value ? 1 : 0);
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

    void AppSettings::HeaderWidth(double value) noexcept
    {
        m_headerWidth = std::clamp(value, MinimumHeaderWidth, MaximumHeaderWidth);
        WriteDword(ValueHeaderWidth, static_cast<uint32_t>(std::lround(m_headerWidth)));
    }

    void AppSettings::LauncherWidth(double value) noexcept
    {
        m_launcherWidth = std::clamp(value, MinimumLauncherWidth, MaximumLauncherWidth);
        WriteDword(ValueLauncherWidth, static_cast<uint32_t>(std::lround(m_launcherWidth)));
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

    void AppSettings::ApplyDeviceOffsets(bool value) noexcept
    {
        m_applyDeviceOffsets = value;
        WriteDword(ValueApplyDeviceOffsets, value ? 1 : 0);
    }

    _Use_decl_annotations_
    void AppSettings::ClockOutputs(std::vector<ClockOutputSetting> const& value) noexcept
    {
        try
        {
            m_clockOutputs = value;
            m_clockOutputs.resize(std::min(m_clockOutputs.size(), MaximumClockOutputs));
            WriteString(ValueClockOutputs, WriteClockOutputs(m_clockOutputs));
        }
        catch (...)
        {
        }
    }

    _Use_decl_annotations_
    void AppSettings::PortsOff(std::vector<PortSetting> const& value) noexcept
    {
        try
        {
            m_portsOff.clear();

            for (auto const& port : value)
            {
                if (m_portsOff.size() >= MaximumPortsOff)
                {
                    break;
                }

                if ((!port.EndpointId.empty() || !port.EndpointName.empty()) && (!port.PlayTo || !port.RecordFrom))
                {
                    m_portsOff.push_back(port);
                }
            }

            WriteString(ValuePortsOff, WritePortsOff(m_portsOff));
        }
        catch (...)
        {
        }
    }

    std::wstring AppSettings::RecentSequencesText() const noexcept
    {
        return ReadString(ValueRecentSequences, L"");
    }

    _Use_decl_annotations_
    void AppSettings::RecentSequencesText(std::wstring const& value) const noexcept
    {
        WriteString(ValueRecentSequences, value);
    }

    _Use_decl_annotations_
    void AppSettings::TimeCode(TimeCodeSetting const& value) noexcept
    {
        try
        {
            m_timeCode = value;
            WriteString(ValueTimeCode, WriteTimeCode(m_timeCode));
        }
        catch (...)
        {
        }
    }

    _Use_decl_annotations_
    void AppSettings::FollowClock(FollowClockSetting const& value) noexcept
    {
        try
        {
            m_followClock = value;
            WriteString(ValueFollowClock, WriteFollowClock(m_followClock));
        }
        catch (...)
        {
        }
    }

    void AppSettings::ClockSendsTempo(bool value) noexcept
    {
        m_clockSendsTempo = value;
        WriteDword(ValueClockSendsTempo, value ? 1 : 0);
    }

    void AppSettings::RemoteControlEnabled(bool value) noexcept
    {
        m_remoteControlEnabled = value;
        WriteDword(ValueRemoteControlEnabled, value ? 1 : 0);
    }
}
