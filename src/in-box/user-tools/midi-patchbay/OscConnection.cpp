// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "OscConnection.h"
#include "OscAddress.h"
#include "OscRules.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <format>

namespace midipatchbay
{
    namespace
    {
        template <typename TEnum>
        struct EnumName
        {
            TEnum Value;
            std::wstring_view Key;
        };

        constexpr std::array<EnumName<OscFormat>, 4> FormatNames
        { {
            { OscFormat::Paths, L"paths" },
            { OscFormat::MidiType, L"midiType" },
            { OscFormat::Arguments, L"arguments" },
            { OscFormat::Custom, L"custom" },
        } };

        constexpr std::array<EnumName<OscRuleDirection>, 3> RuleDirectionNames
        { {
            { OscRuleDirection::Both, L"both" },
            { OscRuleDirection::In, L"in" },
            { OscRuleDirection::Out, L"out" },
        } };

        constexpr std::array<EnumName<OscRuleMessage>, 6> RuleMessageNames
        { {
            { OscRuleMessage::ControlChange, L"controlChange" },
            { OscRuleMessage::Note, L"note" },
            { OscRuleMessage::ProgramChange, L"programChange" },
            { OscRuleMessage::PitchBend, L"pitchBend" },
            { OscRuleMessage::ChannelPressure, L"channelPressure" },
            { OscRuleMessage::PolyPressure, L"polyPressure" },
        } };

        constexpr std::array<EnumName<OscArgumentNames>, 2> ArgumentNamesNames
        { {
            { OscArgumentNames::OpenStageControl, L"openStageControl" },
            { OscArgumentNames::OscMidiOsc, L"oscMidiOsc" },
        } };

        constexpr std::array<EnumName<OscValueKind>, 3> ValueKindNames
        { {
            { OscValueKind::Integers, L"integers" },
            { OscValueKind::Floats, L"floats" },
            { OscValueKind::UnitFloats, L"zeroToOne" },
        } };

        constexpr std::array<EnumName<OscPitchBendRange>, 2> PitchBendNames
        { {
            { OscPitchBendRange::Unsigned, L"zeroTo16383" },
            { OscPitchBendRange::Signed, L"centeredOnZero" },
        } };

        constexpr std::array<EnumName<OscNoteOff>, 2> NoteOffNames
        { {
            { OscNoteOff::VelocityZero, L"velocityZero" },
            { OscNoteOff::OwnAddress, L"ownAddress" },
        } };

        constexpr std::array<EnumName<OscListenOn>, 2> ListenOnNames
        { {
            { OscListenOn::EveryNetwork, L"everyNetwork" },
            { OscListenOn::ThisPc, L"thisPc" },
        } };

        constexpr std::array<EnumName<OscAcceptFrom>, 3> AcceptFromNames
        { {
            { OscAcceptFrom::Anyone, L"anyone" },
            { OscAcceptFrom::SendHost, L"sendHost" },
            { OscAcceptFrom::List, L"list" },
        } };

        constexpr std::array<EnumName<OscReceivedTimeTags>, 3> TimeTagNames
        { {
            { OscReceivedTimeTags::PlayOnArrival, L"playOnArrival" },
            { OscReceivedTimeTags::UseSenderTime, L"useSenderTime" },
            { OscReceivedTimeTags::KeepSenderSpacing, L"keepSenderSpacing" },
        } };

        constexpr std::array<EnumName<OscSendTiming>, 2> SendTimingNames
        { {
            { OscSendTiming::AtItsTime, L"atItsTime" },
            { OscSendTiming::EarlyWithTimeTags, L"earlyWithTimeTags" },
        } };

        constexpr std::array<EnumName<OscPreset>, 6> PresetNames
        { {
            { OscPreset::ReaperVirtualKeyboard, L"reaperVirtualKeyboard" },
            { OscPreset::Paths, L"paths" },
            { OscPreset::MidiType, L"midiType" },
            { OscPreset::OpenStageControl, L"openStageControl" },
            { OscPreset::OscMidiOsc, L"oscMidiOsc" },
            { OscPreset::TouchOsc, L"touchOsc" },
        } };

        template <typename TEnum, size_t N>
        std::wstring_view KeyOf(_In_ std::array<EnumName<TEnum>, N> const& names, _In_ TEnum value) noexcept
        {
            for (auto const& name : names)
            {
                if (name.Value == value)
                {
                    return name.Key;
                }
            }

            return names[0].Key;
        }

        // Unknown or missing keys read as the default, like the rest of the file.
        template <typename TEnum, size_t N>
        TEnum ReadEnum(
            _In_ json::JsonObject const& object,
            _In_ std::wstring_view key,
            _In_ std::array<EnumName<TEnum>, N> const& names,
            _In_ TEnum fallback) noexcept
        {
            try
            {
                if (object == nullptr || !object.HasKey(key))
                {
                    return fallback;
                }

                auto const value = object.GetNamedValue(key);

                if (value == nullptr || value.ValueType() != json::JsonValueType::String)
                {
                    return fallback;
                }

                auto const text = value.GetString();

                for (auto const& name : names)
                {
                    if (name.Key == std::wstring_view{ text })
                    {
                        return name.Value;
                    }
                }
            }
            catch (...)
            {
            }

            return fallback;
        }

        int64_t ReadInteger(
            _In_ json::JsonObject const& object,
            _In_ std::wstring_view key,
            _In_ int64_t lowest,
            _In_ int64_t highest,
            _In_ int64_t fallback) noexcept
        {
            try
            {
                if (object == nullptr || !object.HasKey(key))
                {
                    return fallback;
                }

                auto const value = object.GetNamedValue(key);

                if (value == nullptr || value.ValueType() != json::JsonValueType::Number)
                {
                    return fallback;
                }

                auto const number = value.GetNumber();

                // Whole numbers only, and NaN fails every comparison.
                if (!(number >= static_cast<double>(lowest) && number <= static_cast<double>(highest)) ||
                    number != static_cast<double>(static_cast<int64_t>(number)))
                {
                    return fallback;
                }

                return static_cast<int64_t>(number);
            }
            catch (...)
            {
                return fallback;
            }
        }

        bool ReadBoolean(_In_ json::JsonObject const& object, _In_ std::wstring_view key, _In_ bool fallback) noexcept
        {
            try
            {
                if (object == nullptr || !object.HasKey(key))
                {
                    return fallback;
                }

                auto const value = object.GetNamedValue(key);

                return value != nullptr && value.ValueType() == json::JsonValueType::Boolean ? value.GetBoolean() : fallback;
            }
            catch (...)
            {
                return fallback;
            }
        }

        std::wstring ReadString(_In_ json::JsonObject const& object, _In_ std::wstring_view key) noexcept
        {
            try
            {
                if (object == nullptr || !object.HasKey(key))
                {
                    return {};
                }

                auto const value = object.GetNamedValue(key);

                return value != nullptr && value.ValueType() == json::JsonValueType::String ? std::wstring{ value.GetString() } : std::wstring{};
            }
            catch (...)
            {
                return {};
            }
        }

        json::JsonObject ReadObject(_In_ json::JsonObject const& object, _In_ std::wstring_view key) noexcept
        {
            try
            {
                if (object != nullptr && object.HasKey(key))
                {
                    auto const value = object.GetNamedValue(key);

                    if (value != nullptr && value.ValueType() == json::JsonValueType::Object)
                    {
                        return value.GetObject();
                    }
                }
            }
            catch (...)
            {
            }

            return nullptr;
        }

        json::JsonArray ReadArray(_In_ json::JsonObject const& object, _In_ std::wstring_view key) noexcept
        {
            try
            {
                if (object != nullptr && object.HasKey(key))
                {
                    auto const value = object.GetNamedValue(key);

                    if (value != nullptr && value.ValueType() == json::JsonValueType::Array)
                    {
                        return value.GetArray();
                    }
                }
            }
            catch (...)
            {
            }

            return nullptr;
        }

        // Host names and addresses: printable, no spaces, and short.
        std::wstring CleanHost(_In_ std::wstring const& text)
        {
            std::wstring clean{};

            for (auto const c : text)
            {
                if (c > 0x20 && c != 0x7F && clean.size() < MaximumOscHostLength)
                {
                    clean += c;
                }
            }

            return clean;
        }

        // Roots are ASCII. Anything else, or anything OSC doesn't allow, isn't a root.
        std::optional<std::string> AsciiRoot(_In_ std::wstring const& text)
        {
            std::string root{};

            for (auto const c : text)
            {
                if (c >= 0x80)
                {
                    return std::nullopt;
                }

                root += static_cast<char>(c);
            }

            if (!osc::IsValidRoot(root))
            {
                return std::nullopt;
            }

            return root;
        }

        bool SameRuleNumber(_In_ OscRuleNumber const& first, _In_ OscRuleNumber const& second) noexcept
        {
            return first.Part == second.Part && first.Offset == second.Offset;
        }

        bool SameRule(_In_ OscRule const& first, _In_ OscRule const& second) noexcept
        {
            return first.Address == second.Address &&
                first.Argument == second.Argument &&
                first.Direction == second.Direction &&
                first.Message == second.Message &&
                SameRuleNumber(first.Channel, second.Channel) &&
                SameRuleNumber(first.Number, second.Number) &&
                first.Low == second.Low &&
                first.High == second.High;
        }

        bool SameRowSettings(_In_ OscRow const& first, _In_ OscRow const& second) noexcept
        {
            return first.Root == second.Root &&
                first.Format == second.Format &&
                (first.Format != OscFormat::Arguments || first.Names == second.Names) &&
                first.ChannelsFromZero == second.ChannelsFromZero &&
                first.Values == second.Values &&
                first.PitchBend == second.PitchBend &&
                first.NoteOff == second.NoteOff &&
                first.SendSysEx == second.SendSysEx &&
                first.SendClock == second.SendClock &&
                first.Channel == second.Channel &&
                std::equal(first.Rules.begin(), first.Rules.end(), second.Rules.begin(), second.Rules.end(), SameRule);
        }

        // A rule as the file can hold it: a valid address, numbers in range, and a value range
        // with two different ends. Nothing when it can't be made one.
        std::optional<OscRule> CleanRule(_In_ OscRule rule) noexcept
        {
            if (!osc::IsValidRuleAddress(rule.Address) ||
                static_cast<uint32_t>(rule.Direction) > static_cast<uint32_t>(OscRuleDirection::Out) ||
                static_cast<uint32_t>(rule.Message) >= static_cast<uint32_t>(OscRuleMessageCount) ||
                !std::isfinite(rule.Low) || !std::isfinite(rule.High) ||
                std::fabs(rule.Low) > MaximumOscRuleValue || std::fabs(rule.High) > MaximumOscRuleValue ||
                rule.Low == rule.High)
            {
                return std::nullopt;
            }

            rule.Argument = (std::min)(rule.Argument, MaximumOscRuleArgument);
            return rule;
        }

        json::IJsonValue RuleNumberToJson(_In_ OscRuleNumber const& number)
        {
            if (number.Part.empty())
            {
                return json::JsonValue::CreateNumberValue(number.Offset);
            }

            return json::JsonValue::CreateStringValue(osc::RuleNumberText(number));
        }

        // A whole number, or "{n}+70" and the like. Nothing when it's neither.
        std::optional<OscRuleNumber> ReadRuleNumber(_In_ json::JsonObject const& object, _In_ std::wstring_view key, _In_ OscRuleNumber const& fallback) noexcept
        {
            try
            {
                if (!object.HasKey(key))
                {
                    return fallback;
                }

                auto const value = object.GetNamedValue(key);

                if (value.ValueType() == json::JsonValueType::Number)
                {
                    auto const number = value.GetNumber();

                    if (!std::isfinite(number) || number != std::floor(number) || std::fabs(number) > 1'000'000.0)
                    {
                        return std::nullopt;
                    }

                    return OscRuleNumber{ {}, static_cast<int32_t>(number) };
                }

                if (value.ValueType() == json::JsonValueType::String)
                {
                    return osc::ParseRuleNumber(value.GetString());
                }
            }
            catch (...)
            {
            }

            return std::nullopt;
        }

        json::JsonObject RuleToJson(_In_ OscRule const& rule)
        {
            json::JsonObject item{};

            item.Insert(L"address", json::JsonValue::CreateStringValue(std::wstring{ rule.Address.begin(), rule.Address.end() }));
            item.Insert(L"argument", json::JsonValue::CreateNumberValue(rule.Argument));
            item.Insert(L"direction", json::JsonValue::CreateStringValue(KeyOf(RuleDirectionNames, rule.Direction)));
            item.Insert(L"message", json::JsonValue::CreateStringValue(KeyOf(RuleMessageNames, rule.Message)));
            item.Insert(L"channel", RuleNumberToJson(rule.Channel));
            item.Insert(L"number", RuleNumberToJson(rule.Number));
            item.Insert(L"low", json::JsonValue::CreateNumberValue(rule.Low));
            item.Insert(L"high", json::JsonValue::CreateNumberValue(rule.High));

            return item;
        }

        std::optional<OscRule> RuleFromJson(_In_ json::JsonObject const& item) noexcept
        {
            try
            {
                OscRule rule{};

                auto const address = ReadString(item, L"address");

                if (std::any_of(address.begin(), address.end(), [](wchar_t c) { return c >= 0x80; }))
                {
                    return std::nullopt;
                }

                // Checked to be ASCII above, so each character fits in one byte.
                for (auto const c : address)
                {
                    rule.Address += static_cast<char>(c);
                }
                rule.Argument = static_cast<uint8_t>(ReadInteger(item, L"argument", 0, MaximumOscRuleArgument, 0));
                rule.Direction = ReadEnum(item, L"direction", RuleDirectionNames, OscRuleDirection::Both);
                rule.Message = ReadEnum(item, L"message", RuleMessageNames, OscRuleMessage::ControlChange);

                auto const channel = ReadRuleNumber(item, L"channel", OscRuleNumber{});
                auto const number = ReadRuleNumber(item, L"number", OscRuleNumber{ {}, 1 });

                if (!channel.has_value() || !number.has_value())
                {
                    return std::nullopt;
                }

                rule.Channel = *channel;
                rule.Number = *number;

                auto const readReal = [&item](std::wstring_view key, double fallback)
                    {
                        if (item.HasKey(key) && item.GetNamedValue(key).ValueType() == json::JsonValueType::Number)
                        {
                            return item.GetNamedNumber(key);
                        }

                        return fallback;
                    };

                rule.Low = readReal(L"low", 0.0);
                rule.High = readReal(L"high", 1.0);

                return CleanRule(std::move(rule));
            }
            catch (...)
            {
                return std::nullopt;
            }
        }
    }

    _Use_decl_annotations_
    std::wstring_view OscFormatKey(OscFormat format) noexcept
    {
        return KeyOf(FormatNames, format);
    }

    _Use_decl_annotations_
    std::wstring_view OscPresetKey(OscPreset preset) noexcept
    {
        return KeyOf(PresetNames, preset);
    }

    _Use_decl_annotations_
    OscRow OscRowFromPreset(OscPreset preset, uint8_t group) noexcept
    {
        OscRow row{};
        row.Group = static_cast<uint8_t>(group & 0x0F);

        switch (preset)
        {
        case OscPreset::ReaperVirtualKeyboard:
            row.Root = "/vkb_midi";
            row.Format = OscFormat::Paths;
            row.ChannelsFromZero = true;
            break;

        case OscPreset::Paths:
            row.Root = "/midi";
            row.Format = OscFormat::Paths;
            break;

        case OscPreset::MidiType:
            row.Root = "/midi";
            row.Format = OscFormat::MidiType;
            break;

        case OscPreset::OpenStageControl:
            row.Root.clear();
            row.Format = OscFormat::Arguments;
            row.Names = OscArgumentNames::OpenStageControl;
            break;

        case OscPreset::OscMidiOsc:
            row.Root.clear();
            row.Format = OscFormat::Arguments;
            row.Names = OscArgumentNames::OscMidiOsc;
            row.NoteOff = OscNoteOff::OwnAddress;
            break;

        case OscPreset::TouchOsc:
            row.Root.clear();
            row.Format = OscFormat::Custom;

            try
            {
                row.Rules = osc::TouchOscRules();
            }
            catch (...)
            {
            }

            break;
        }

        return row;
    }

    _Use_decl_annotations_
    std::optional<OscPreset> OscPresetOf(OscRow const& row) noexcept
    {
        for (auto const& name : PresetNames)
        {
            if (SameRowSettings(row, OscRowFromPreset(name.Value, row.Group)))
            {
                return name.Value;
            }
        }

        return std::nullopt;
    }

    _Use_decl_annotations_
    OscConnectionSettings OscConnectionFromPreset(OscPreset preset) noexcept
    {
        OscConnectionSettings settings{};

        try
        {
            settings.Rows.push_back(OscRowFromPreset(preset, 0));
        }
        catch (...)
        {
        }

        // The ports TouchOSC and REAPER start with: they send on 8000 and listen on 9000.
        if (preset == OscPreset::ReaperVirtualKeyboard)
        {
            // REAPER doesn't send its virtual keyboard back, so this only sends.
            settings.SendHost = L"127.0.0.1";
            settings.SendPort = 8000;
        }
        else
        {
            settings.ReceivePort = 8000;
            settings.SendPort = 9000;
        }

        return settings;
    }

    _Use_decl_annotations_
    bool OscConnectionReceives(OscConnectionSettings const& settings) noexcept
    {
        return settings.ReceivePort != 0;
    }

    _Use_decl_annotations_
    bool OscConnectionSends(OscConnectionSettings const& settings) noexcept
    {
        return settings.SendPort != 0 && (!settings.SendHost.empty() || settings.SendToLastSender);
    }

    _Use_decl_annotations_
    OscRow const* OscRowOnGroup(OscConnectionSettings const& settings, uint8_t group) noexcept
    {
        for (auto const& row : settings.Rows)
        {
            if (row.Group == group)
            {
                return &row;
            }
        }

        return nullptr;
    }

    _Use_decl_annotations_
    std::optional<uint8_t> FreeOscGroup(OscConnectionSettings const& settings) noexcept
    {
        for (uint8_t group = 0; group < 16; group++)
        {
            if (OscRowOnGroup(settings, group) == nullptr)
            {
                return group;
            }
        }

        return std::nullopt;
    }

    _Use_decl_annotations_
    std::wstring OscRowAddressText(OscRow const& row)
    {
        if (row.Root.empty())
        {
            return L"/";
        }

        return std::wstring{ row.Root.begin(), row.Root.end() };
    }

    _Use_decl_annotations_
    void SanitizeOscConnection(OscConnectionSettings& settings) noexcept
    {
        try
        {
            std::vector<OscRow> rows{};
            uint32_t used{ 0 };

            for (auto& row : settings.Rows)
            {
                if (rows.size() >= MaximumOscRows || row.Group > 15 || (used & (1u << row.Group)) != 0)
                {
                    continue;
                }

                if (!osc::IsValidRoot(row.Root))
                {
                    row.Root = "/midi";
                }

                if (static_cast<uint32_t>(row.Format) >= static_cast<uint32_t>(OscFormatCount))
                {
                    row.Format = OscFormat::Paths;
                }

                // The MIDI type is sent to the root itself, so the top level can't be one.
                if (row.Format == OscFormat::MidiType && row.Root.empty())
                {
                    row.Root = "/midi";
                }

                // Rules carry their own addresses. Any other format has none.
                if (row.Format == OscFormat::Custom)
                {
                    row.Root.clear();

                    std::vector<OscRule> rules{};

                    for (auto& rule : row.Rules)
                    {
                        if (rules.size() >= MaximumOscRules)
                        {
                            break;
                        }

                        if (auto clean = CleanRule(std::move(rule)); clean.has_value())
                        {
                            rules.push_back(std::move(*clean));
                        }
                    }

                    row.Rules = std::move(rules);
                }
                else
                {
                    row.Rules.clear();
                }

                row.Channel = static_cast<uint8_t>(row.Channel & 0x0F);

                used |= 1u << row.Group;
                rows.push_back(std::move(row));
            }

            std::sort(rows.begin(), rows.end(), [](OscRow const& a, OscRow const& b) { return a.Group < b.Group; });
            settings.Rows = std::move(rows);

            settings.SendHost = CleanHost(settings.SendHost);

            std::vector<std::wstring> accept{};

            for (auto const& address : settings.AcceptAddresses)
            {
                auto clean = CleanHost(address);

                if (!clean.empty() && accept.size() < MaximumOscAcceptAddresses &&
                    std::find(accept.begin(), accept.end(), clean) == accept.end())
                {
                    accept.push_back(std::move(clean));
                }
            }

            settings.AcceptAddresses = std::move(accept);
        }
        catch (...)
        {
            settings.Rows.clear();
            settings.AcceptAddresses.clear();
            settings.SendHost.clear();
        }

        settings.ReceiveRateLimit = std::clamp(settings.ReceiveRateLimit, MinimumOscReceiveRateLimit, MaximumOscReceiveRateLimit);
        settings.SpacingDelayMilliseconds = (std::min)(settings.SpacingDelayMilliseconds, MaximumOscSpacingDelayMilliseconds);
    }

    _Use_decl_annotations_
    json::JsonObject OscConnectionToJson(OscConnectionSettings const& settings)
    {
        json::JsonObject object{};

        json::JsonObject receive{};
        receive.Insert(L"port", json::JsonValue::CreateNumberValue(settings.ReceivePort));
        receive.Insert(L"listenOn", json::JsonValue::CreateStringValue(KeyOf(ListenOnNames, settings.ListenOn)));
        receive.Insert(L"acceptFrom", json::JsonValue::CreateStringValue(KeyOf(AcceptFromNames, settings.AcceptFrom)));

        if (!settings.AcceptAddresses.empty())
        {
            json::JsonArray addresses{};

            for (auto const& address : settings.AcceptAddresses)
            {
                addresses.Append(json::JsonValue::CreateStringValue(address));
            }

            receive.Insert(L"acceptAddresses", addresses);
        }

        if (settings.ReceiveRateLimit != DefaultOscReceiveRateLimit)
        {
            receive.Insert(L"messagesPerSecond", json::JsonValue::CreateNumberValue(settings.ReceiveRateLimit));
        }

        if (!settings.Advertise)
        {
            receive.Insert(L"advertise", json::JsonValue::CreateBooleanValue(false));
        }

        object.Insert(L"receive", receive);

        json::JsonObject send{};
        send.Insert(L"host", json::JsonValue::CreateStringValue(settings.SendHost));
        send.Insert(L"port", json::JsonValue::CreateNumberValue(settings.SendPort));
        send.Insert(L"toLastSender", json::JsonValue::CreateBooleanValue(settings.SendToLastSender));
        object.Insert(L"send", send);

        object.Insert(L"receivedTimeTags", json::JsonValue::CreateStringValue(KeyOf(TimeTagNames, settings.ReceivedTimeTags)));

        if (settings.SpacingDelayMilliseconds != DefaultOscSpacingDelayMilliseconds)
        {
            object.Insert(L"spacingDelayMilliseconds", json::JsonValue::CreateNumberValue(settings.SpacingDelayMilliseconds));
        }

        object.Insert(L"sendTiming", json::JsonValue::CreateStringValue(KeyOf(SendTimingNames, settings.SendTiming)));
        object.Insert(L"bundleTogether", json::JsonValue::CreateBooleanValue(settings.BundleTogether));

        json::JsonArray rows{};

        for (auto const& row : settings.Rows)
        {
            json::JsonObject item{};

            item.Insert(L"group", json::JsonValue::CreateNumberValue(row.Group));
            item.Insert(L"root", json::JsonValue::CreateStringValue(std::wstring{ row.Root.begin(), row.Root.end() }));
            item.Insert(L"format", json::JsonValue::CreateStringValue(KeyOf(FormatNames, row.Format)));

            if (row.Format == OscFormat::Arguments)
            {
                item.Insert(L"names", json::JsonValue::CreateStringValue(KeyOf(ArgumentNamesNames, row.Names)));
            }

            if (row.Format == OscFormat::Custom)
            {
                json::JsonArray rules{};

                for (auto const& rule : row.Rules)
                {
                    rules.Append(RuleToJson(rule));
                }

                item.Insert(L"rules", rules);
            }

            item.Insert(L"channelsFromZero", json::JsonValue::CreateBooleanValue(row.ChannelsFromZero));
            item.Insert(L"values", json::JsonValue::CreateStringValue(KeyOf(ValueKindNames, row.Values)));
            item.Insert(L"pitchBend", json::JsonValue::CreateStringValue(KeyOf(PitchBendNames, row.PitchBend)));
            item.Insert(L"noteOff", json::JsonValue::CreateStringValue(KeyOf(NoteOffNames, row.NoteOff)));
            item.Insert(L"sendSysEx", json::JsonValue::CreateBooleanValue(row.SendSysEx));
            item.Insert(L"sendClock", json::JsonValue::CreateBooleanValue(row.SendClock));
            item.Insert(L"channel", json::JsonValue::CreateNumberValue(row.Channel));

            rows.Append(item);
        }

        object.Insert(L"rows", rows);

        return object;
    }

    _Use_decl_annotations_
    OscConnectionSettings OscConnectionFromJson(json::JsonObject const& object) noexcept
    {
        OscConnectionSettings settings{};

        try
        {
            auto const receive = ReadObject(object, L"receive");

            settings.ReceivePort = static_cast<uint16_t>(ReadInteger(receive, L"port", 0, 65535, 0));
            settings.ListenOn = ReadEnum(receive, L"listenOn", ListenOnNames, OscListenOn::EveryNetwork);
            settings.AcceptFrom = ReadEnum(receive, L"acceptFrom", AcceptFromNames, OscAcceptFrom::SendHost);
            settings.Advertise = ReadBoolean(receive, L"advertise", true);
            settings.ReceiveRateLimit = static_cast<uint32_t>(ReadInteger(receive, L"messagesPerSecond",
                MinimumOscReceiveRateLimit, MaximumOscReceiveRateLimit, DefaultOscReceiveRateLimit));

            if (auto const addresses = ReadArray(receive, L"acceptAddresses"); addresses != nullptr)
            {
                for (auto const& value : addresses)
                {
                    if (settings.AcceptAddresses.size() >= MaximumOscAcceptAddresses)
                    {
                        break;
                    }

                    if (value != nullptr && value.ValueType() == json::JsonValueType::String)
                    {
                        settings.AcceptAddresses.emplace_back(value.GetString());
                    }
                }
            }

            auto const send = ReadObject(object, L"send");

            settings.SendHost = ReadString(send, L"host");
            settings.SendPort = static_cast<uint16_t>(ReadInteger(send, L"port", 0, 65535, 0));
            settings.SendToLastSender = ReadBoolean(send, L"toLastSender", false);

            settings.ReceivedTimeTags = ReadEnum(object, L"receivedTimeTags", TimeTagNames, OscReceivedTimeTags::PlayOnArrival);
            settings.SpacingDelayMilliseconds = static_cast<uint32_t>(ReadInteger(object, L"spacingDelayMilliseconds",
                0, MaximumOscSpacingDelayMilliseconds, DefaultOscSpacingDelayMilliseconds));
            settings.SendTiming = ReadEnum(object, L"sendTiming", SendTimingNames, OscSendTiming::AtItsTime);
            settings.BundleTogether = ReadBoolean(object, L"bundleTogether", false);

            if (auto const rows = ReadArray(object, L"rows"); rows != nullptr)
            {
                for (auto const& value : rows)
                {
                    // Twice the limit, so duplicates past it don't push good rows out before
                    // SanitizeOscConnection drops them.
                    if (settings.Rows.size() >= MaximumOscRows * 2)
                    {
                        break;
                    }

                    if (value == nullptr || value.ValueType() != json::JsonValueType::Object)
                    {
                        continue;
                    }

                    auto const item = value.GetObject();
                    auto const group = ReadInteger(item, L"group", 0, 15, -1);

                    if (group < 0)
                    {
                        continue;
                    }

                    OscRow row{};
                    row.Group = static_cast<uint8_t>(group);

                    auto const root = AsciiRoot(ReadString(item, L"root"));

                    if (!root.has_value())
                    {
                        continue;
                    }

                    row.Root = *root;
                    row.Format = ReadEnum(item, L"format", FormatNames, OscFormat::Paths);
                    row.Names = ReadEnum(item, L"names", ArgumentNamesNames, OscArgumentNames::OpenStageControl);
                    row.ChannelsFromZero = ReadBoolean(item, L"channelsFromZero", false);
                    row.Values = ReadEnum(item, L"values", ValueKindNames, OscValueKind::Integers);
                    row.PitchBend = ReadEnum(item, L"pitchBend", PitchBendNames, OscPitchBendRange::Unsigned);
                    row.NoteOff = ReadEnum(item, L"noteOff", NoteOffNames, OscNoteOff::VelocityZero);
                    row.SendSysEx = ReadBoolean(item, L"sendSysEx", true);
                    row.SendClock = ReadBoolean(item, L"sendClock", false);
                    row.Channel = static_cast<uint8_t>(ReadInteger(item, L"channel", 0, 15, 0));

                    if (auto const rules = ReadArray(item, L"rules"); rules != nullptr)
                    {
                        for (auto const& ruleValue : rules)
                        {
                            if (row.Rules.size() >= MaximumOscRules)
                            {
                                break;
                            }

                            if (ruleValue == nullptr || ruleValue.ValueType() != json::JsonValueType::Object)
                            {
                                continue;
                            }

                            if (auto rule = RuleFromJson(ruleValue.GetObject()); rule.has_value())
                            {
                                row.Rules.push_back(std::move(*rule));
                            }
                        }
                    }

                    settings.Rows.push_back(std::move(row));
                }
            }
        }
        catch (...)
        {
            settings.Rows.clear();
        }

        SanitizeOscConnection(settings);

        return settings;
    }

    _Use_decl_annotations_
    std::wstring OscConnectionSignature(OscConnectionSettings const& settings) noexcept
    {
        try
        {
            std::wstring text = std::format(L"r{}:{}:{}:{}:{}|s{}:{}:{}|t{}:{}:{}:{}",
                settings.ReceivePort,
                settings.Advertise ? 1 : 0,
                static_cast<int32_t>(settings.ListenOn),
                static_cast<int32_t>(settings.AcceptFrom),
                settings.ReceiveRateLimit,
                settings.SendHost,
                settings.SendPort,
                settings.SendToLastSender ? 1 : 0,
                static_cast<int32_t>(settings.ReceivedTimeTags),
                settings.SpacingDelayMilliseconds,
                static_cast<int32_t>(settings.SendTiming),
                settings.BundleTogether ? 1 : 0);

            for (auto const& address : settings.AcceptAddresses)
            {
                text += L"|a" + address;
            }

            for (auto const& row : settings.Rows)
            {
                text += std::format(L"|g{}:{}:{}:{}:{}:{}:{}:{}:{}:{}:{}",
                    row.Group,
                    std::wstring{ row.Root.begin(), row.Root.end() },
                    static_cast<int32_t>(row.Format),
                    static_cast<int32_t>(row.Names),
                    row.ChannelsFromZero ? 1 : 0,
                    static_cast<int32_t>(row.Values),
                    static_cast<int32_t>(row.PitchBend),
                    static_cast<int32_t>(row.NoteOff),
                    row.SendSysEx ? 1 : 0,
                    row.SendClock ? 1 : 0,
                    row.Channel);

                for (auto const& rule : row.Rules)
                {
                    text += std::format(L"|u{}:{}:{}:{}:{}:{}:{}:{}",
                        std::wstring{ rule.Address.begin(), rule.Address.end() },
                        rule.Argument,
                        static_cast<int32_t>(rule.Direction),
                        static_cast<int32_t>(rule.Message),
                        osc::RuleNumberText(rule.Channel),
                        osc::RuleNumberText(rule.Number),
                        rule.Low,
                        rule.High);
                }
            }

            return text;
        }
        catch (...)
        {
            return {};
        }
    }

    _Use_decl_annotations_
    std::wstring OscServiceLabel(std::wstring_view connectionName, std::wstring_view computerName)
    {
        // Periods would split the label, and control characters have no place in a name.
        auto const clean = [](std::wstring_view text)
            {
                std::wstring result{};

                for (auto const c : text)
                {
                    result += (c == L'.' || c < 0x20 || c == 0x7F) ? L' ' : c;
                }

                auto const first = result.find_first_not_of(L' ');
                auto const last = result.find_last_not_of(L' ');

                return first == std::wstring::npos ? std::wstring{} : result.substr(first, last - first + 1);
            };

        auto const name = clean(connectionName);
        auto const computer = clean(computerName);

        std::wstring label = name.empty() ? computer : (computer.empty() ? name : name + L" (" + computer + L")");

        if (label.empty())
        {
            label = L"OSC";
        }

        // At most 63 bytes of UTF-8, cut where a whole character ends.
        size_t bytes{ 0 };
        size_t keep{ 0 };

        while (keep < label.size())
        {
            auto const c = static_cast<uint32_t>(label[keep]);
            auto const surrogatePair = c >= 0xD800 && c <= 0xDBFF && keep + 1 < label.size();
            auto const size = c < 0x80 ? 1u : c < 0x800 ? 2u : surrogatePair ? 4u : 3u;

            if (bytes + size > 63)
            {
                break;
            }

            bytes += size;
            keep += surrogatePair ? 2 : 1;
        }

        label.resize(keep);

        auto const last = label.find_last_not_of(L' ');
        label.resize(last == std::wstring::npos ? 0 : last + 1);

        return label;
    }
}