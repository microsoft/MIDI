// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Deliberately free of pch.h and XAML, like the rest of the document layer.

#include "RemoteControlFile.h"
#include "LayoutSerializer.h"

#include <algorithm>
#include <cmath>

namespace glass
{
    namespace
    {
        namespace mjson = winrt::Windows::Data::Json;

        constexpr wchar_t KeyRules[] = L"rules";
        constexpr wchar_t KeyTellPage[] = L"tellPage";
        constexpr wchar_t KeyEnabled[] = L"enabled";
        constexpr wchar_t KeyWhen[] = L"when";
        constexpr wchar_t KeyDo[] = L"do";
        constexpr wchar_t KeyDevice[] = L"device";
        constexpr wchar_t KeyKind[] = L"kind";
        constexpr wchar_t KeyGroup[] = L"group";
        constexpr wchar_t KeyChannel[] = L"channel";
        constexpr wchar_t KeyAnyChannel[] = L"anyChannel";
        constexpr wchar_t KeyNumber[] = L"number";
        constexpr wchar_t KeyAnyNumber[] = L"anyNumber";
        constexpr wchar_t KeyPage[] = L"page";
        constexpr wchar_t KeyControl[] = L"control";
        constexpr wchar_t KeyControlGroup[] = L"controlGroup";
        constexpr wchar_t KeyFromMessage[] = L"fromMessage";
        constexpr wchar_t KeyValue[] = L"value";
        constexpr wchar_t KeyFunction[] = L"function";
        constexpr wchar_t KeyTargets[] = L"targets";
        constexpr wchar_t KeyKey[] = L"key";
        constexpr wchar_t KeyScale[] = L"scale";
        constexpr wchar_t KeySequence[] = L"sequence";
        constexpr wchar_t KeyFirstValue[] = L"firstValue";

        struct ActionName
        {
            RemoteActionKind Value{};
            std::wstring_view Name{};
        };

        constexpr ActionName ActionNames[]
        {
            { RemoteActionKind::GoToPage, L"goToPage" },
            { RemoteActionKind::NextPage, L"nextPage" },
            { RemoteActionKind::PreviousPage, L"previousPage" },
            { RemoteActionKind::PageFromMessage, L"pageFromMessage" },
            { RemoteActionKind::PressControl, L"pressControl" },
            { RemoteActionKind::SetControlValue, L"setControlValue" },
            { RemoteActionKind::EnableControl, L"enableControl" },
            { RemoteActionKind::DisableControl, L"disableControl" },
            { RemoteActionKind::ToggleEnabled, L"toggleEnabled" },
            { RemoteActionKind::ShowControl, L"showControl" },
            { RemoteActionKind::HideControl, L"hideControl" },
            { RemoteActionKind::ToggleShown, L"toggleShown" },
            { RemoteActionKind::ShowOnlyThis, L"showOnlyThis" },
            { RemoteActionKind::PadGrid, L"padGrid" },
            { RemoteActionKind::SetKeyRoot, L"setKeyRoot" },
            { RemoteActionKind::SetScale, L"setScale" },
            { RemoteActionKind::RunSequence, L"runSequence" },
            { RemoteActionKind::Panic, L"panic" },
        };

        // ---- reading helpers. None of these trust the file. ----

        mjson::IJsonValue Find(_In_ mjson::JsonObject const& object, _In_ std::wstring_view key) noexcept
        {
            try
            {
                winrt::hstring const name{ key };

                if (object == nullptr || !object.HasKey(name))
                {
                    return nullptr;
                }

                return object.Lookup(name);
            }
            catch (...)
            {
                return nullptr;
            }
        }

        std::wstring ReadString(_In_ mjson::JsonObject const& object, _In_ std::wstring_view key) noexcept
        {
            try
            {
                auto const value = Find(object, key);

                if (value == nullptr || value.ValueType() != mjson::JsonValueType::String)
                {
                    return {};
                }

                return SanitizeStoredString(std::wstring{ value.GetString() });
            }
            catch (...)
            {
                return {};
            }
        }

        double ReadNumber(_In_ mjson::JsonObject const& object, _In_ std::wstring_view key, _In_ double fallback) noexcept
        {
            try
            {
                auto const value = Find(object, key);

                if (value == nullptr || value.ValueType() != mjson::JsonValueType::Number)
                {
                    return fallback;
                }

                auto const number = value.GetNumber();

                return std::isfinite(number) ? number : fallback;
            }
            catch (...)
            {
                return fallback;
            }
        }

        int32_t ReadInt(
            _In_ mjson::JsonObject const& object,
            _In_ std::wstring_view key,
            _In_ int32_t fallback,
            _In_ int32_t lowest,
            _In_ int32_t highest) noexcept
        {
            auto const number = ReadNumber(object, key, static_cast<double>(fallback));

            if (number < static_cast<double>(lowest) || number > static_cast<double>(highest))
            {
                return fallback;
            }

            return static_cast<int32_t>(number);
        }

        bool ReadBool(_In_ mjson::JsonObject const& object, _In_ std::wstring_view key, _In_ bool fallback) noexcept
        {
            try
            {
                auto const value = Find(object, key);

                if (value == nullptr || value.ValueType() != mjson::JsonValueType::Boolean)
                {
                    return fallback;
                }

                return value.GetBoolean();
            }
            catch (...)
            {
                return fallback;
            }
        }

        mjson::JsonObject ReadObject(_In_ mjson::JsonObject const& object, _In_ std::wstring_view key) noexcept
        {
            try
            {
                auto const value = Find(object, key);

                return value != nullptr && value.ValueType() == mjson::JsonValueType::Object
                    ? value.GetObject()
                    : nullptr;
            }
            catch (...)
            {
                return nullptr;
            }
        }

        mjson::JsonArray ReadArray(_In_ mjson::JsonObject const& object, _In_ std::wstring_view key) noexcept
        {
            try
            {
                auto const value = Find(object, key);

                return value != nullptr && value.ValueType() == mjson::JsonValueType::Array
                    ? value.GetArray()
                    : nullptr;
            }
            catch (...)
            {
                return nullptr;
            }
        }

        // A message kind a rule may listen for or tell the page with. Anything else the file
        // names is kept, and never matches.
        MessageKind ReadKind(_In_ mjson::JsonObject const& object, _Inout_ bool& unknown) noexcept
        {
            auto const name = ReadString(object, KeyKind);
            MessageKind kind{ MessageKind::ProgramChange };

            if (!name.empty() && !TryReadMessageKindName(name, kind))
            {
                unknown = true;
                return MessageKind::ProgramChange;
            }

            return kind;
        }

        RemoteTrigger ReadTrigger(_In_ mjson::JsonObject const& object, _Inout_ bool& unknown) noexcept
        {
            RemoteTrigger trigger{};

            if (object == nullptr)
            {
                return trigger;
            }

            trigger.DeviceName = ReadString(object, KeyDevice);
            trigger.Kind = ReadKind(object, unknown);
            trigger.GroupIndex = ReadInt(object, KeyGroup, 0, 0, MaximumGroupCount - 1);
            trigger.ChannelIndex = ReadBool(object, KeyAnyChannel, false)
                ? AnyRemoteChannel
                : ReadInt(object, KeyChannel, 0, 0, 15);
            trigger.Number = ReadBool(object, KeyAnyNumber, false)
                ? AnyRemoteNumber
                : ReadInt(object, KeyNumber, 0, 0, 127);

            trigger.Unknown = CaptureUnknown(object,
                { KeyDevice, KeyKind, KeyGroup, KeyChannel, KeyAnyChannel, KeyNumber, KeyAnyNumber });

            return trigger;
        }

        PadGridTargets ReadTargets(_In_ mjson::JsonObject const& object) noexcept
        {
            PadGridTargets targets{};

            auto const list = ReadArray(object, KeyTargets);

            if (list == nullptr)
            {
                return targets;
            }

            targets.AllGrids = false;

            try
            {
                for (uint32_t i = 0; i < list.Size() && targets.ControlIds.size() < MaximumControlsPerPage; ++i)
                {
                    auto const value = list.GetAt(i);

                    if (value == nullptr || value.ValueType() != mjson::JsonValueType::String)
                    {
                        continue;
                    }

                    auto id = SanitizeStoredString(std::wstring{ value.GetString() });

                    if (!id.empty() && std::find(targets.ControlIds.begin(), targets.ControlIds.end(), id) == targets.ControlIds.end())
                    {
                        targets.ControlIds.push_back(std::move(id));
                    }
                }
            }
            catch (...)
            {
            }

            return targets;
        }

        RemoteRule ReadRule(_In_ mjson::JsonObject const& object, _Inout_ bool& unknown) noexcept
        {
            RemoteRule rule{};

            rule.Enabled = ReadBool(object, KeyEnabled, true);
            rule.When = ReadTrigger(ReadObject(object, KeyWhen), unknown);

            auto const actionName = ReadString(object, KeyDo);
            auto const action = std::find_if(std::begin(ActionNames), std::end(ActionNames),
                [&actionName](ActionName const& entry) { return entry.Name == actionName; });

            if (action == std::end(ActionNames))
            {
                // Kept as it was written, never run. Saving it as anything else would lose what a
                // newer build meant by it.
                rule.Action = RemoteActionKind::Unrecognized;
                rule.UnrecognizedAction = actionName;
                unknown = true;
            }
            else
            {
                rule.Action = action->Value;
            }

            rule.TargetPageId = ReadString(object, KeyPage);
            rule.TargetControlId = ReadString(object, KeyControl);
            rule.TargetGroupId = ReadString(object, KeyControlGroup);
            rule.FromMessage = ReadBool(object, KeyFromMessage, false);
            rule.Value = std::clamp(ReadNumber(object, KeyValue, 1.0), 0.0, 1.0);

            if (auto const function = ReadString(object, KeyFunction); !function.empty() &&
                !TryReadPadGridFunctionName(function, rule.PadFunction))
            {
                rule.PadFunction = PadGridFunction::OctaveUp;
                unknown = true;
            }

            rule.PadTargets = ReadTargets(object);

            if (auto const key = ReadString(object, KeyKey); !key.empty() && !TryReadKeyRootName(key, rule.KeyRoot))
            {
                rule.KeyRoot = 0;
            }

            if (auto const scale = ReadString(object, KeyScale); !scale.empty() && !TryReadScaleName(scale, rule.Scale))
            {
                rule.Scale = MusicalScale::Major;
            }

            rule.SequenceName = ReadString(object, KeySequence);

            rule.Unknown = CaptureUnknown(object,
                { KeyEnabled, KeyWhen, KeyDo, KeyPage, KeyControl, KeyControlGroup, KeyFromMessage, KeyValue,
                  KeyFunction, KeyTargets, KeyKey, KeyScale, KeySequence });

            return rule;
        }

        PageFeedback ReadTellPage(_In_ mjson::JsonObject const& object, _Inout_ bool& unknown) noexcept
        {
            PageFeedback feedback{};

            if (object == nullptr)
            {
                return feedback;
            }

            feedback.Enabled = ReadBool(object, KeyEnabled, true);
            feedback.DeviceName = ReadString(object, KeyDevice);
            feedback.Kind = ReadKind(object, unknown);
            feedback.GroupIndex = ReadInt(object, KeyGroup, 0, 0, MaximumGroupCount - 1);
            feedback.ChannelIndex = ReadInt(object, KeyChannel, 0, 0, 15);
            feedback.Number = ReadInt(object, KeyNumber, 0, 0, 127);
            feedback.FirstValue = ReadInt(object, KeyFirstValue, 0, 0, 127);

            feedback.Unknown = CaptureUnknown(object,
                { KeyEnabled, KeyDevice, KeyKind, KeyGroup, KeyChannel, KeyNumber, KeyFirstValue });

            return feedback;
        }

        // Whether the rule's action reads this field, so only what matters goes in the file.
        bool UsesPage(_In_ RemoteActionKind action) noexcept
        {
            return action == RemoteActionKind::GoToPage;
        }

        bool UsesTarget(_In_ RemoteActionKind action) noexcept
        {
            switch (action)
            {
            case RemoteActionKind::PressControl:
            case RemoteActionKind::SetControlValue:
            case RemoteActionKind::EnableControl:
            case RemoteActionKind::DisableControl:
            case RemoteActionKind::ToggleEnabled:
            case RemoteActionKind::ShowControl:
            case RemoteActionKind::HideControl:
            case RemoteActionKind::ToggleShown:
            case RemoteActionKind::ShowOnlyThis:
                return true;

            default:
                return false;
            }
        }

        bool UsesFromMessage(_In_ RemoteActionKind action) noexcept
        {
            return action == RemoteActionKind::SetControlValue ||
                action == RemoteActionKind::SetKeyRoot ||
                action == RemoteActionKind::SetScale;
        }

        bool UsesPadTargets(_In_ RemoteActionKind action) noexcept
        {
            return action == RemoteActionKind::PadGrid ||
                action == RemoteActionKind::SetKeyRoot ||
                action == RemoteActionKind::SetScale;
        }

        void WriteTargets(_Inout_ JsonTextWriter& writer, _In_ PadGridTargets const& targets) noexcept
        {
            if (targets.AllGrids)
            {
                return;
            }

            writer.BeginArray(KeyTargets);

            for (auto const& id : targets.ControlIds)
            {
                writer.WriteArrayString(id);
            }

            writer.EndArray();
        }
    }

    _Use_decl_annotations_
    void ReadRemoteControl(mjson::JsonObject const& object, RemoteControl& remote, bool& namesUnknownAction) noexcept
    {
        remote = {};
        namesUnknownAction = false;

        if (object == nullptr)
        {
            return;
        }

        try
        {
            if (auto const rules = ReadArray(object, KeyRules))
            {
                for (uint32_t i = 0; i < rules.Size() && remote.Rules.size() < MaximumRemoteRules; ++i)
                {
                    auto const value = rules.GetAt(i);

                    if (value != nullptr && value.ValueType() == mjson::JsonValueType::Object)
                    {
                        remote.Rules.push_back(ReadRule(value.GetObject(), namesUnknownAction));
                    }
                }
            }

            remote.TellPage = ReadTellPage(ReadObject(object, KeyTellPage), namesUnknownAction);
            remote.Unknown = CaptureUnknown(object, { KeyRules, KeyTellPage });
        }
        catch (...)
        {
            remote = {};
        }
    }

    _Use_decl_annotations_
    bool HasRemoteControl(RemoteControl const& remote) noexcept
    {
        auto const& tell = remote.TellPage;

        return !remote.Rules.empty() ||
            remote.Unknown != nullptr ||
            tell.Enabled ||
            !tell.DeviceName.empty() ||
            tell.Unknown != nullptr;
    }

    _Use_decl_annotations_
    void WriteRemoteControl(JsonTextWriter& writer, std::wstring_view key, RemoteControl const& remote) noexcept
    {
        if (!HasRemoteControl(remote))
        {
            return;
        }

        writer.BeginObject(key);
        writer.BeginArray(KeyRules);

        for (auto const& rule : remote.Rules)
        {
            writer.BeginObject();

            if (!rule.Enabled)
            {
                writer.Write(KeyEnabled, false);
            }

            writer.BeginObject(KeyWhen);
            writer.Write(KeyDevice, rule.When.DeviceName);
            writer.Write(KeyKind, MessageKindFileName(rule.When.Kind));
            writer.Write(KeyGroup, static_cast<int64_t>(rule.When.GroupIndex));

            if (rule.When.ChannelIndex == AnyRemoteChannel)
            {
                writer.Write(KeyAnyChannel, true);
            }
            else
            {
                writer.Write(KeyChannel, static_cast<int64_t>(rule.When.ChannelIndex));
            }

            if (rule.When.Number == AnyRemoteNumber)
            {
                writer.Write(KeyAnyNumber, true);
            }
            else
            {
                writer.Write(KeyNumber, static_cast<int64_t>(rule.When.Number));
            }

            WriteUnknown(writer, rule.When.Unknown);
            writer.EndObject();

            if (rule.Action == RemoteActionKind::Unrecognized)
            {
                writer.Write(KeyDo, rule.UnrecognizedAction);
            }
            else
            {
                auto const action = std::find_if(std::begin(ActionNames), std::end(ActionNames),
                    [&rule](ActionName const& entry) { return entry.Value == rule.Action; });

                writer.Write(KeyDo, action == std::end(ActionNames) ? ActionNames[0].Name : action->Name);
            }

            if (UsesPage(rule.Action))
            {
                writer.Write(KeyPage, rule.TargetPageId);
            }

            if (UsesTarget(rule.Action))
            {
                if (!rule.TargetGroupId.empty())
                {
                    writer.Write(KeyControlGroup, rule.TargetGroupId);
                }
                else
                {
                    writer.Write(KeyControl, rule.TargetControlId);
                }
            }

            if (UsesFromMessage(rule.Action) && rule.FromMessage)
            {
                writer.Write(KeyFromMessage, true);
            }

            if (rule.Action == RemoteActionKind::SetControlValue && !rule.FromMessage)
            {
                writer.Write(KeyValue, rule.Value);
            }

            if (rule.Action == RemoteActionKind::PadGrid)
            {
                writer.Write(KeyFunction, PadGridFunctionFileName(rule.PadFunction));
            }

            if (UsesPadTargets(rule.Action))
            {
                WriteTargets(writer, rule.PadTargets);
            }

            if (rule.Action == RemoteActionKind::SetKeyRoot && !rule.FromMessage)
            {
                writer.Write(KeyKey, KeyRootFileName(rule.KeyRoot));
            }

            if (rule.Action == RemoteActionKind::SetScale && !rule.FromMessage)
            {
                writer.Write(KeyScale, ScaleFileName(rule.Scale));
            }

            if (rule.Action == RemoteActionKind::RunSequence)
            {
                writer.Write(KeySequence, rule.SequenceName);
            }

            WriteUnknown(writer, rule.Unknown);
            writer.EndObject();
        }

        writer.EndArray();

        auto const& tell = remote.TellPage;

        if (tell.Enabled || !tell.DeviceName.empty() || tell.Unknown != nullptr)
        {
            writer.BeginObject(KeyTellPage);
            writer.Write(KeyEnabled, tell.Enabled);
            writer.Write(KeyDevice, tell.DeviceName);
            writer.Write(KeyKind, MessageKindFileName(tell.Kind));
            writer.Write(KeyGroup, static_cast<int64_t>(tell.GroupIndex));
            writer.Write(KeyChannel, static_cast<int64_t>(tell.ChannelIndex));

            if (tell.Kind == MessageKind::ControlChange)
            {
                writer.Write(KeyNumber, static_cast<int64_t>(tell.Number));
            }

            writer.Write(KeyFirstValue, static_cast<int64_t>(tell.FirstValue));
            WriteUnknown(writer, tell.Unknown);
            writer.EndObject();
        }

        WriteUnknown(writer, remote.Unknown);
        writer.EndObject();
    }
}
