// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Deliberately free of pch.h and XAML, like the rest of the document layer.

#include "LayoutSerializer.h"
#include "JsonText.h"

#include <algorithm>

namespace glass
{
    namespace
    {
        namespace mjson = winrt::Windows::Data::Json;

        constexpr wchar_t CommentText[] =
            L"Windows MIDI Glass layout. Written by the MIDI Glass app. The MIDI service does not read this file.";

        constexpr wchar_t KeyComment[] = L"_comment";
        constexpr wchar_t KeyFileVersion[] = L"fileVersion";
        constexpr wchar_t KeyName[] = L"name";
        constexpr wchar_t KeyDescription[] = L"description";
        constexpr wchar_t KeyCreated[] = L"created";
        constexpr wchar_t KeyModified[] = L"modified";
        constexpr wchar_t KeyPageWidth[] = L"pageWidth";
        constexpr wchar_t KeyPageHeight[] = L"pageHeight";
        constexpr wchar_t KeyCanvasWidth[] = L"canvasWidth";
        constexpr wchar_t KeyCanvasHeight[] = L"canvasHeight";
        constexpr wchar_t KeyTheme[] = L"theme";
        constexpr wchar_t KeyScaleMode[] = L"scaleMode";
        constexpr wchar_t KeyCustomScalePercent[] = L"customScalePercent";
        constexpr wchar_t KeyCornerButton[] = L"fullScreenButtonCorner";
        constexpr wchar_t KeyPreferredDisplay[] = L"preferredDisplayId";
        constexpr wchar_t KeySuppressStartup[] = L"suppressAllStartupValues";
        constexpr wchar_t KeyVirtualDevice[] = L"publishesVirtualDevice";
        constexpr wchar_t KeyFavorite[] = L"isFavorite";
        constexpr wchar_t KeyTempo[] = L"tempo";
        constexpr wchar_t KeyDevices[] = L"devices";
        constexpr wchar_t KeyPages[] = L"pages";
        constexpr wchar_t KeySequences[] = L"sequences";

        constexpr wchar_t KeyId[] = L"id";
        constexpr wchar_t KeyHueSlot[] = L"hueSlot";
        constexpr wchar_t KeySharedBand[] = L"sharedBand";
        constexpr wchar_t KeyControls[] = L"controls";

        constexpr wchar_t KeyKind[] = L"kind";
        constexpr wchar_t KeyLabel[] = L"label";
        constexpr wchar_t KeyX[] = L"x";
        constexpr wchar_t KeyY[] = L"y";
        constexpr wchar_t KeyWidth[] = L"width";
        constexpr wchar_t KeyHeight[] = L"height";
        constexpr wchar_t KeyLiteralColor[] = L"literalColor";
        constexpr wchar_t KeyAspectLocked[] = L"aspectLocked";
        constexpr wchar_t KeyStyle[] = L"style";
        constexpr wchar_t KeyLabelPlaced[] = L"labelPlaced";
        constexpr wchar_t KeyShowValue[] = L"showValue";
        constexpr wchar_t KeyKeyboardOrder[] = L"keyboardOrder";
        constexpr wchar_t KeyPickup[] = L"pickup";
        constexpr wchar_t KeyDefaultValue[] = L"defaultValue";
        constexpr wchar_t KeySendsValueOnStart[] = L"sendsValueOnStart";
        constexpr wchar_t KeySendInterval[] = L"sendIntervalMilliseconds";
        constexpr wchar_t KeyMessages[] = L"messages";
        constexpr wchar_t KeyFeedback[] = L"feedback";

        constexpr wchar_t KeyTrigger[] = L"trigger";
        constexpr wchar_t KeyDevice[] = L"device";
        constexpr wchar_t KeyGroup[] = L"group";
        constexpr wchar_t KeyChannel[] = L"channel";
        constexpr wchar_t KeyNumber[] = L"number";
        constexpr wchar_t KeyOnValue[] = L"onValue";
        constexpr wchar_t KeyOffValue[] = L"offValue";
        constexpr wchar_t KeyMinimum[] = L"minimum";
        constexpr wchar_t KeyMaximum[] = L"maximum";
        constexpr wchar_t KeyValue[] = L"value";
        constexpr wchar_t KeyDetents[] = L"detents";
        constexpr wchar_t KeyMode[] = L"mode";
        constexpr wchar_t KeyStep[] = L"step";
        constexpr wchar_t KeyStops[] = L"stops";
        constexpr wchar_t KeySystemExclusive[] = L"systemExclusive";
        constexpr wchar_t KeyMidi1Protocol[] = L"midi1Protocol";
        constexpr wchar_t KeyScaling[] = L"scaling";
        constexpr wchar_t KeyWords[] = L"words";
        constexpr wchar_t KeySequence[] = L"sequence";
        constexpr wchar_t KeyTargetPage[] = L"targetPage";
        constexpr wchar_t KeyTargetLayer[] = L"targetLayer";

        constexpr wchar_t KeyEnabled[] = L"enabled";
        constexpr wchar_t KeyMatch[] = L"match";
        constexpr wchar_t KeyMatchMode[] = L"matchMode";
        constexpr wchar_t KeySendsBeatClock[] = L"sendsBeatClock";

        constexpr wchar_t KeySteps[] = L"steps";
        constexpr wchar_t KeyMessage[] = L"message";
        constexpr wchar_t KeyWaitMilliseconds[] = L"waitMilliseconds";
        constexpr wchar_t KeyRepeatCount[] = L"repeatCount";
        constexpr wchar_t KeyTargetControl[] = L"targetControl";
        constexpr wchar_t KeyTargetValue[] = L"targetValue";
        constexpr wchar_t KeyBeatsPerMinute[] = L"beatsPerMinute";

        // Enums travel as names rather than numbers. A number in a file is unreadable, and it
        // silently means something different the day somebody inserts a value in the middle.
        template <typename TEnum>
        struct EnumName
        {
            TEnum Value{};
            std::wstring_view Name{};
        };

        constexpr EnumName<ControlKind> ControlKindNames[]
        {
            { ControlKind::Knob, L"knob" },
            { ControlKind::Fader, L"fader" },
            { ControlKind::Pad, L"pad" },
            { ControlKind::Button, L"button" },
            { ControlKind::Toggle, L"toggle" },
            { ControlKind::XYPad, L"xyPad" },
            { ControlKind::Encoder, L"encoder" },
            { ControlKind::Meter, L"meter" },
            { ControlKind::Lamp, L"lamp" },
            { ControlKind::Readout, L"readout" },
            { ControlKind::Label, L"label" },
            { ControlKind::Image, L"image" },
            { ControlKind::PageTab, L"pageTab" },
        };

        constexpr EnumName<MessageTrigger> TriggerNames[]
        {
            { MessageTrigger::TurnsOn, L"turnsOn" },
            { MessageTrigger::TurnsOff, L"turnsOff" },
            { MessageTrigger::Changes, L"changes" },
            { MessageTrigger::Touched, L"touched" },
            { MessageTrigger::Released, L"released" },
        };

        constexpr EnumName<MessageKind> MessageKindNames[]
        {
            { MessageKind::Note, L"note" },
            { MessageKind::ControlChange, L"controlChange" },
            { MessageKind::ProgramChange, L"programChange" },
            { MessageKind::PitchBend, L"pitchBend" },
            { MessageKind::ChannelPressure, L"channelPressure" },
            { MessageKind::PerNoteController, L"perNoteController" },
            { MessageKind::RegisteredController, L"registeredController" },
            { MessageKind::AssignedController, L"assignedController" },
            { MessageKind::SystemExclusive, L"systemExclusive" },
            { MessageKind::RawUmp, L"rawUmp" },
            { MessageKind::Sequence, L"sequence" },
            { MessageKind::GoToPage, L"goToPage" },
            { MessageKind::HoldLayer, L"holdLayer" },
        };

        constexpr EnumName<PickupMode> PickupNames[]
        {
            { PickupMode::Jump, L"jump" },
            { PickupMode::Catch, L"catch" },
            { PickupMode::Relative, L"relative" },
        };

        // "useTheme" is never written: an override that defers to the theme is the absence of
        // an override, and writing it would put three dead keys on every control in the file.
        constexpr EnumName<ControlStyleOverride> StyleNames[]
        {
            { ControlStyleOverride::UseTheme, L"useTheme" },
            { ControlStyleOverride::Plate, L"plate" },
            { ControlStyleOverride::Outline, L"outline" },
            { ControlStyleOverride::Solid, L"solid" },
            { ControlStyleOverride::Bare, L"bare" },
        };

        constexpr EnumName<LabelPlacementOverride> LabelPlacedNames[]
        {
            { LabelPlacementOverride::UseTheme, L"useTheme" },
            { LabelPlacementOverride::Inside, L"inside" },
            { LabelPlacementOverride::Below, L"below" },
            { LabelPlacementOverride::None, L"none" },
        };

        constexpr EnumName<ShowValueOverride> ShowValueNames[]
        {
            { ShowValueOverride::UseTheme, L"useTheme" },
            { ShowValueOverride::Always, L"always" },
            { ShowValueOverride::WhileTouched, L"whileTouched" },
            { ShowValueOverride::Never, L"never" },
        };

        constexpr EnumName<ValueScaling> ScalingNames[]
        {
            { ValueScaling::Fraction, L"fraction" },
            { ValueScaling::Absolute, L"absolute" },
        };

        constexpr EnumName<DetentMode> DetentModeNames[]
        {
            { DetentMode::Continuous, L"continuous" },
            { DetentMode::EvenSteps, L"evenSteps" },
            { DetentMode::ExplicitValues, L"explicitValues" },
        };

        constexpr EnumName<ScaleMode> ScaleModeNames[]
        {
            { ScaleMode::ActualSize, L"actualSize" },
            { ScaleMode::FitToScreen, L"fitToScreen" },
            { ScaleMode::Custom, L"custom" },
        };

        constexpr EnumName<ScreenCorner> CornerNames[]
        {
            { ScreenCorner::TopLeft, L"topLeft" },
            { ScreenCorner::TopRight, L"topRight" },
            { ScreenCorner::BottomLeft, L"bottomLeft" },
            { ScreenCorner::BottomRight, L"bottomRight" },
        };

        constexpr EnumName<TempoSourceKind> TempoKindNames[]
        {
            { TempoSourceKind::Internal, L"internal" },
            { TempoSourceKind::FollowIncomingClock, L"followIncomingClock" },
        };

        constexpr EnumName<SequenceStepKind> StepKindNames[]
        {
            { SequenceStepKind::SendMidiMessage, L"sendMessage" },
            { SequenceStepKind::SendSystemExclusive, L"sendSystemExclusive" },
            { SequenceStepKind::Wait, L"wait" },
            { SequenceStepKind::SetControlValue, L"setControlValue" },
            { SequenceStepKind::GoToPage, L"goToPage" },
            { SequenceStepKind::HoldLayer, L"holdLayer" },
            { SequenceStepKind::RepeatBlockStart, L"repeatStart" },
            { SequenceStepKind::RepeatBlockEnd, L"repeatEnd" },
        };

        constexpr EnumName<midiapp::EndpointMatchMode> MatchModeNames[]
        {
            { midiapp::EndpointMatchMode::EndpointDeviceId, L"endpointDeviceId" },
            { midiapp::EndpointMatchMode::UsbVendorAndProduct, L"usbVendorAndProduct" },
            { midiapp::EndpointMatchMode::EndpointName, L"endpointName" },
        };

        template <typename TEnum, size_t N>
        std::wstring_view NameOf(_In_ EnumName<TEnum> const (&table)[N], _In_ TEnum value) noexcept
        {
            for (auto const& entry : table)
            {
                if (entry.Value == value)
                {
                    return entry.Name;
                }
            }

            return table[0].Name;
        }

        template <typename TEnum, size_t N>
        TEnum ValueOf(
            _In_ EnumName<TEnum> const (&table)[N],
            _In_ std::wstring_view name,
            _In_ TEnum fallback) noexcept
        {
            for (auto const& entry : table)
            {
                if (entry.Name == name)
                {
                    return entry.Value;
                }
            }

            return fallback;
        }

        // ---- reading helpers. None of these trust the file. ----

        std::wstring ReadString(_In_ mjson::JsonObject const& object, _In_ std::wstring_view key) noexcept
        {
            try
            {
                winrt::hstring const name{ key };

                if (!object.HasKey(name))
                {
                    return {};
                }

                auto const value = object.Lookup(name);

                if (value == nullptr || value.ValueType() != mjson::JsonValueType::String)
                {
                    return {};
                }

                return midiapp::SanitizeStoredString(std::wstring{ value.GetString() });
            }
            catch (...)
            {
                return {};
            }
        }

        double ReadNumber(
            _In_ mjson::JsonObject const& object,
            _In_ std::wstring_view key,
            _In_ double fallback) noexcept
        {
            try
            {
                winrt::hstring const name{ key };

                if (!object.HasKey(name))
                {
                    return fallback;
                }

                auto const value = object.Lookup(name);

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

        bool ReadBool(
            _In_ mjson::JsonObject const& object,
            _In_ std::wstring_view key,
            _In_ bool fallback) noexcept
        {
            try
            {
                winrt::hstring const name{ key };

                if (!object.HasKey(name))
                {
                    return fallback;
                }

                auto const value = object.Lookup(name);

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
                winrt::hstring const name{ key };

                if (!object.HasKey(name))
                {
                    return nullptr;
                }

                auto const value = object.Lookup(name);

                if (value == nullptr || value.ValueType() != mjson::JsonValueType::Object)
                {
                    return nullptr;
                }

                return value.GetObject();
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
                winrt::hstring const name{ key };

                if (!object.HasKey(name))
                {
                    return nullptr;
                }

                auto const value = object.Lookup(name);

                if (value == nullptr || value.ValueType() != mjson::JsonValueType::Array)
                {
                    return nullptr;
                }

                return value.GetArray();
            }
            catch (...)
            {
                return nullptr;
            }
        }

        // One end of a message's range. An absolute end is whatever the device documentation
        // said, up to the widest field anything here has; a fraction is a fraction.
        MessageValue ReadMessageValue(
            _In_ mjson::JsonObject const& object,
            _In_ std::wstring_view key,
            _In_ MessageValue const& fallback) noexcept
        {
            auto const nested = ReadObject(object, key);

            if (nested == nullptr)
            {
                return fallback;
            }

            MessageValue result{};

            result.Scaling = ValueOf(ScalingNames, ReadString(nested, KeyScaling), ValueScaling::Fraction);

            auto const highest = result.Scaling == ValueScaling::Absolute ? 4294967295.0 : 1.0;

            result.Value = std::clamp(ReadNumber(nested, KeyValue, fallback.Value), 0.0, highest);

            return result;
        }

        void WriteMessageValue(
            _Inout_ JsonTextWriter& writer,
            _In_ std::wstring_view key,
            _In_ MessageValue const& value) noexcept
        {
            writer.BeginObject(key);
            writer.Write(KeyValue, value.Value);
            writer.Write(KeyScaling, NameOf(ScalingNames, value.Scaling));
            writer.EndObject();
        }

        MessageDetents ReadDetents(_In_ mjson::JsonObject const& object) noexcept
        {
            MessageDetents detents{};

            auto const nested = ReadObject(object, KeyDetents);

            if (nested == nullptr)
            {
                return detents;
            }

            detents.Mode = ValueOf(DetentModeNames, ReadString(nested, KeyMode), DetentMode::Continuous);
            detents.Scaling = ValueOf(ScalingNames, ReadString(nested, KeyScaling), ValueScaling::Fraction);

            auto const highest = detents.Scaling == ValueScaling::Absolute ? 4294967295.0 : 1.0;

            detents.Step = std::clamp(ReadNumber(nested, KeyStep, 0.0), 0.0, highest);

            if (auto const stops = ReadArray(nested, KeyStops))
            {
                for (uint32_t i = 0; i < stops.Size() && detents.Stops.size() < MaximumDetentStops; ++i)
                {
                    auto const value = stops.GetAt(i);

                    if (value != nullptr && value.ValueType() == mjson::JsonValueType::Number)
                    {
                        auto const number = value.GetNumber();

                        if (std::isfinite(number))
                        {
                            detents.Stops.push_back(std::clamp(number, 0.0, highest));
                        }
                    }
                }
            }

            return detents;
        }

        void WriteDetents(_Inout_ JsonTextWriter& writer, _In_ MessageDetents const& detents) noexcept
        {
            if (detents.Mode == DetentMode::Continuous)
            {
                return;
            }

            writer.BeginObject(KeyDetents);
            writer.Write(KeyMode, NameOf(DetentModeNames, detents.Mode));
            writer.Write(KeyScaling, NameOf(ScalingNames, detents.Scaling));

            if (detents.Mode == DetentMode::EvenSteps)
            {
                writer.Write(KeyStep, detents.Step);
            }
            else
            {
                writer.BeginArray(KeyStops);

                for (auto const stop : detents.Stops)
                {
                    writer.WriteArrayNumber(stop);
                }

                writer.EndArray();
            }

            writer.EndObject();
        }

        // System exclusive travels as hex rather than an array of numbers. A firmware dump is
        // tens of thousands of bytes, and one number per line would make the file unreadable and
        // enormous for no gain.
        std::wstring ToHex(_In_ std::vector<uint8_t> const& bytes) noexcept
        {
            constexpr wchar_t digits[] = L"0123456789ABCDEF";

            std::wstring text{};
            text.reserve(bytes.size() * 2);

            for (auto const value : bytes)
            {
                text += digits[(value >> 4) & 0x0F];
                text += digits[value & 0x0F];
            }

            return text;
        }

        std::vector<uint8_t> FromHex(_In_ std::wstring_view text) noexcept
        {
            std::vector<uint8_t> bytes{};

            if (text.size() % 2 != 0 || text.size() / 2 > MaximumSystemExclusiveBytes)
            {
                return bytes;
            }

            auto const nibble = [](wchar_t ch) noexcept -> int32_t
                {
                    if (ch >= L'0' && ch <= L'9') { return ch - L'0'; }
                    if (ch >= L'A' && ch <= L'F') { return ch - L'A' + 10; }
                    if (ch >= L'a' && ch <= L'f') { return ch - L'a' + 10; }
                    return -1;
                };

            bytes.reserve(text.size() / 2);

            for (size_t i = 0; i < text.size(); i += 2)
            {
                auto const high = nibble(text[i]);
                auto const low = nibble(text[i + 1]);

                if (high < 0 || low < 0)
                {
                    // one bad character means the whole blob is untrustworthy
                    return {};
                }

                bytes.push_back(static_cast<uint8_t>((high << 4) | low));
            }

            return bytes;
        }
    }

    // ================================ reading ================================

    namespace
    {
        ControlMessage ReadMessage(_In_ mjson::JsonObject const& object) noexcept
        {
            ControlMessage message{};

            message.Trigger = ValueOf(TriggerNames, ReadString(object, KeyTrigger), MessageTrigger::Changes);
            message.Kind = ValueOf(MessageKindNames, ReadString(object, KeyKind), MessageKind::ControlChange);
            message.DeviceName = ReadString(object, KeyDevice);
            message.GroupIndex = ReadInt(object, KeyGroup, 0, AllGroups, MaximumGroupCount - 1);
            message.ChannelIndex = ReadInt(object, KeyChannel, 0, 0, 15);
            message.Number = static_cast<uint32_t>(ReadInt(object, KeyNumber, 0, 0, 0x7FFFFFFF));
            message.Minimum = ReadMessageValue(object, KeyMinimum, { 0.0, ValueScaling::Fraction });
            message.Maximum = ReadMessageValue(object, KeyMaximum, { 1.0, ValueScaling::Fraction });
            message.Detents = ReadDetents(object);
            message.SystemExclusive = FromHex(ReadString(object, KeySystemExclusive));
            message.UseMidi1Protocol = ReadBool(object, KeyMidi1Protocol, false);
            message.SequenceName = ReadString(object, KeySequence);
            message.TargetPageId = ReadString(object, KeyTargetPage);
            message.TargetLayerId = ReadString(object, KeyTargetLayer);

            if (auto const words = ReadArray(object, KeyWords))
            {
                for (uint32_t i = 0; i < words.Size() && i < 4; ++i)
                {
                    auto const value = words.GetAt(i);

                    if (value != nullptr && value.ValueType() == mjson::JsonValueType::Number)
                    {
                        auto const number = value.GetNumber();

                        if (number >= 0 && number <= 4294967295.0)
                        {
                            message.RawWords.push_back(static_cast<uint32_t>(number));
                        }
                    }
                }
            }

            message.Unknown = CaptureUnknown(object,
                { KeyTrigger, KeyKind, KeyDevice, KeyGroup, KeyChannel, KeyNumber, KeyMinimum,
                  KeyMaximum, KeySystemExclusive, KeyWords, KeySequence, KeyTargetPage,
                  KeyTargetLayer, KeyMidi1Protocol, KeyDetents });

            return message;
        }

        FeedbackBinding ReadFeedback(_In_ mjson::JsonObject const& object) noexcept
        {
            FeedbackBinding feedback{};

            feedback.Enabled = ReadBool(object, KeyEnabled, false);
            feedback.Kind = ValueOf(MessageKindNames, ReadString(object, KeyKind), MessageKind::ControlChange);
            feedback.DeviceName = ReadString(object, KeyDevice);
            feedback.GroupIndex = ReadInt(object, KeyGroup, 0, AllGroups, MaximumGroupCount - 1);
            feedback.ChannelIndex = ReadInt(object, KeyChannel, 0, 0, 15);
            feedback.Number = static_cast<uint32_t>(ReadInt(object, KeyNumber, 0, 0, 0x7FFFFFFF));

            feedback.Unknown = CaptureUnknown(object,
                { KeyEnabled, KeyKind, KeyDevice, KeyGroup, KeyChannel, KeyNumber });

            return feedback;
        }

        Control ReadControl(_In_ mjson::JsonObject const& object) noexcept
        {
            Control control{};

            control.Id = ReadString(object, KeyId);
            control.Kind = ValueOf(ControlKindNames, ReadString(object, KeyKind), ControlKind::Knob);
            control.Label = ReadString(object, KeyLabel);
            control.X = ReadNumber(object, KeyX, 0);
            control.Y = ReadNumber(object, KeyY, 0);
            control.Width = ReadNumber(object, KeyWidth, 56);
            control.Height = ReadNumber(object, KeyHeight, 56);
            control.HueSlot = ReadInt(object, KeyHueSlot, 0, LiteralHue, HueSlotCount - 1);
            control.LiteralColor = ReadString(object, KeyLiteralColor);
            control.AspectLocked = ReadBool(object, KeyAspectLocked, false);
            control.Style = ValueOf(StyleNames, ReadString(object, KeyStyle), ControlStyleOverride::UseTheme);
            control.LabelPlaced = ValueOf(LabelPlacedNames, ReadString(object, KeyLabelPlaced), LabelPlacementOverride::UseTheme);
            control.ShowValue = ValueOf(ShowValueNames, ReadString(object, KeyShowValue), ShowValueOverride::UseTheme);
            control.KeyboardOrder = ReadInt(object, KeyKeyboardOrder, 0, 0, 0x7FFFFFFF);
            control.Pickup = ValueOf(PickupNames, ReadString(object, KeyPickup), PickupMode::Jump);
            control.DefaultValue = std::clamp(ReadNumber(object, KeyDefaultValue, 0.0), 0.0, 1.0);
            control.SendsValueOnStart = ReadBool(object, KeySendsValueOnStart, false);
            control.SendIntervalMilliseconds = ReadInt(object, KeySendInterval, 0, 0, 10000);

            if (auto const messages = ReadArray(object, KeyMessages))
            {
                for (uint32_t i = 0; i < messages.Size() && control.Messages.size() < MaximumMessagesPerControl; ++i)
                {
                    auto const value = messages.GetAt(i);

                    if (value != nullptr && value.ValueType() == mjson::JsonValueType::Object)
                    {
                        control.Messages.push_back(ReadMessage(value.GetObject()));
                    }
                }
            }

            if (auto const feedback = ReadObject(object, KeyFeedback))
            {
                control.Feedback = ReadFeedback(feedback);
            }

            control.Unknown = CaptureUnknown(object,
                { KeyId, KeyKind, KeyLabel, KeyX, KeyY, KeyWidth, KeyHeight, KeyHueSlot,
                  KeyLiteralColor, KeyAspectLocked, KeyKeyboardOrder, KeyPickup, KeyDefaultValue,
                  KeySendsValueOnStart, KeySendInterval, KeyMessages, KeyFeedback,
                  KeyStyle, KeyLabelPlaced, KeyShowValue });

            return control;
        }

        Page ReadPage(_In_ mjson::JsonObject const& object) noexcept
        {
            Page page{};

            page.Id = ReadString(object, KeyId);
            page.Name = ReadString(object, KeyName);
            page.HueSlot = ReadInt(object, KeyHueSlot, 0, LiteralHue, HueSlotCount - 1);
            page.IsSharedBand = ReadBool(object, KeySharedBand, false);

            if (auto const controls = ReadArray(object, KeyControls))
            {
                for (uint32_t i = 0; i < controls.Size() && page.Controls.size() < MaximumControlsPerPage; ++i)
                {
                    auto const value = controls.GetAt(i);

                    if (value != nullptr && value.ValueType() == mjson::JsonValueType::Object)
                    {
                        page.Controls.push_back(ReadControl(value.GetObject()));
                    }
                }
            }

            page.Unknown = CaptureUnknown(object, { KeyId, KeyName, KeyHueSlot, KeySharedBand, KeyControls });

            return page;
        }

        DeviceEntry ReadDevice(_In_ mjson::JsonObject const& object) noexcept
        {
            DeviceEntry device{};

            device.Name = ReadString(object, KeyName);

            if (auto const match = ReadObject(object, KeyMatch))
            {
                device.Match = midiapp::MatchFromJson(match);
            }

            device.MatchMode = ValueOf(MatchModeNames, ReadString(object, KeyMatchMode),
                midiapp::EndpointMatchMode::EndpointDeviceId);
            device.SendsBeatClock = ReadBool(object, KeySendsBeatClock, false);

            device.Unknown = CaptureUnknown(object, { KeyName, KeyMatch, KeyMatchMode, KeySendsBeatClock });

            return device;
        }

        SequenceStep ReadStep(_In_ mjson::JsonObject const& object) noexcept
        {
            SequenceStep step{};

            step.Kind = ValueOf(StepKindNames, ReadString(object, KeyKind), SequenceStepKind::SendMidiMessage);

            if (auto const message = ReadObject(object, KeyMessage))
            {
                step.Message = ReadMessage(message);
            }

            step.WaitMilliseconds = static_cast<uint32_t>(ReadInt(object, KeyWaitMilliseconds, 0, 0, 24 * 60 * 60 * 1000));
            step.RepeatCount = static_cast<uint32_t>(ReadInt(object, KeyRepeatCount, 1, 1, 10000));
            step.TargetControlId = ReadString(object, KeyTargetControl);
            step.TargetValue = std::clamp(ReadNumber(object, KeyTargetValue, 0.0), 0.0, 1.0);

            step.Unknown = CaptureUnknown(object,
                { KeyKind, KeyMessage, KeyWaitMilliseconds, KeyRepeatCount, KeyTargetControl, KeyTargetValue });

            return step;
        }

        Sequence ReadSequence(_In_ mjson::JsonObject const& object) noexcept
        {
            Sequence sequence{};

            sequence.Name = ReadString(object, KeyName);

            if (auto const steps = ReadArray(object, KeySteps))
            {
                for (uint32_t i = 0; i < steps.Size() && sequence.Steps.size() < MaximumStepsPerSequence; ++i)
                {
                    auto const value = steps.GetAt(i);

                    if (value != nullptr && value.ValueType() == mjson::JsonValueType::Object)
                    {
                        sequence.Steps.push_back(ReadStep(value.GetObject()));
                    }
                }
            }

            sequence.Unknown = CaptureUnknown(object, { KeyName, KeySteps });

            return sequence;
        }
    }

    _Use_decl_annotations_
    ReadResult ReadLayoutFromJson(std::wstring_view json) noexcept
    {
        ReadResult result{};

        try
        {
            if (json.empty())
            {
                result.Detail = L"The file is empty.";
                return result;
            }

            if (json.size() * sizeof(wchar_t) > MaximumLayoutFileBytes)
            {
                result.Detail = L"The file is larger than a layout is allowed to be.";
                return result;
            }

            mjson::JsonObject root{ nullptr };

            if (!mjson::JsonObject::TryParse(winrt::hstring{ json }, root) || root == nullptr)
            {
                result.Detail = L"The file is not valid JSON.";
                return result;
            }

            auto& document = result.Document;

            document.FileVersion = static_cast<uint32_t>(
                ReadInt(root, KeyFileVersion, static_cast<int32_t>(LayoutFileVersion), 1, 0x7FFFFFFF));

            result.IsFromNewerVersion = document.FileVersion > LayoutFileVersion;

            document.Name = ReadString(root, KeyName);
            document.Description = ReadString(root, KeyDescription);
            document.CreatedTimestamp = static_cast<int64_t>(ReadNumber(root, KeyCreated, 0));
            document.ModifiedTimestamp = static_cast<int64_t>(ReadNumber(root, KeyModified, 0));

            document.PageWidth = ReadInt(root, KeyPageWidth, 1280, 1, 16384);
            document.PageHeight = ReadInt(root, KeyPageHeight, 800, 1, 16384);
            document.CanvasWidth = ReadInt(root, KeyCanvasWidth, document.PageWidth, 1, 32768);
            document.CanvasHeight = ReadInt(root, KeyCanvasHeight, document.PageHeight, 1, 32768);

            document.ThemeName = ReadString(root, KeyTheme);

            document.Scale = ValueOf(ScaleModeNames, ReadString(root, KeyScaleMode), ScaleMode::ActualSize);
            document.CustomScalePercent = std::clamp(ReadNumber(root, KeyCustomScalePercent, 100.0), 10.0, 400.0);
            document.FullScreenButtonCorner = ValueOf(CornerNames, ReadString(root, KeyCornerButton), ScreenCorner::TopRight);
            document.PreferredDisplayId = ReadString(root, KeyPreferredDisplay);
            document.SuppressAllStartupValues = ReadBool(root, KeySuppressStartup, false);
            document.PublishesVirtualDevice = ReadBool(root, KeyVirtualDevice, false);
            document.IsFavorite = ReadBool(root, KeyFavorite, false);

            if (auto const tempo = ReadObject(root, KeyTempo))
            {
                document.Tempo.Kind = ValueOf(TempoKindNames, ReadString(tempo, KeyKind), TempoSourceKind::Internal);
                document.Tempo.BeatsPerMinute = std::clamp(ReadNumber(tempo, KeyBeatsPerMinute, 120.0), 1.0, 999.0);
                document.Tempo.DeviceName = ReadString(tempo, KeyDevice);
                document.Tempo.Unknown = CaptureUnknown(tempo, { KeyKind, KeyBeatsPerMinute, KeyDevice });
            }

            if (auto const devices = ReadArray(root, KeyDevices))
            {
                for (uint32_t i = 0; i < devices.Size() && document.Devices.size() < MaximumDevicesPerLayout; ++i)
                {
                    auto const value = devices.GetAt(i);

                    if (value != nullptr && value.ValueType() == mjson::JsonValueType::Object)
                    {
                        document.Devices.push_back(ReadDevice(value.GetObject()));
                    }
                }
            }

            if (auto const pages = ReadArray(root, KeyPages))
            {
                for (uint32_t i = 0; i < pages.Size() && document.Pages.size() < MaximumPagesPerLayout; ++i)
                {
                    auto const value = pages.GetAt(i);

                    if (value != nullptr && value.ValueType() == mjson::JsonValueType::Object)
                    {
                        document.Pages.push_back(ReadPage(value.GetObject()));
                    }
                }
            }

            if (auto const sequences = ReadArray(root, KeySequences))
            {
                for (uint32_t i = 0; i < sequences.Size() && document.Sequences.size() < MaximumSequencesPerLayout; ++i)
                {
                    auto const value = sequences.GetAt(i);

                    if (value != nullptr && value.ValueType() == mjson::JsonValueType::Object)
                    {
                        document.Sequences.push_back(ReadSequence(value.GetObject()));
                    }
                }
            }

            document.Unknown = CaptureUnknown(root,
                { KeyComment, KeyFileVersion, KeyName, KeyDescription, KeyCreated, KeyModified,
                  KeyPageWidth, KeyPageHeight, KeyCanvasWidth, KeyCanvasHeight, KeyTheme,
                  KeyScaleMode, KeyCustomScalePercent, KeyCornerButton, KeyPreferredDisplay,
                  KeySuppressStartup, KeyVirtualDevice, KeyFavorite, KeyTempo, KeyDevices, KeyPages, KeySequences });

            result.Succeeded = true;
        }
        catch (...)
        {
            result.Succeeded = false;
            result.Detail = L"The file could not be read.";
        }

        return result;
    }

    // ================================ writing ================================

    namespace
    {
        void WriteMessage(_Inout_ JsonTextWriter& writer, _In_ ControlMessage const& message) noexcept
        {
            writer.Write(KeyTrigger, NameOf(TriggerNames, message.Trigger));
            writer.Write(KeyKind, NameOf(MessageKindNames, message.Kind));
            writer.Write(KeyDevice, message.DeviceName);
            writer.Write(KeyGroup, static_cast<int64_t>(message.GroupIndex));
            writer.Write(KeyChannel, static_cast<int64_t>(message.ChannelIndex));
            writer.Write(KeyNumber, static_cast<int64_t>(message.Number));
            WriteMessageValue(writer, KeyMinimum, message.Minimum);
            WriteMessageValue(writer, KeyMaximum, message.Maximum);
            WriteDetents(writer, message.Detents);
            writer.Write(KeyMidi1Protocol, message.UseMidi1Protocol);

            if (!message.SystemExclusive.empty())
            {
                writer.Write(KeySystemExclusive, ToHex(message.SystemExclusive));
            }

            if (!message.RawWords.empty())
            {
                writer.BeginArray(KeyWords);

                for (auto const word : message.RawWords)
                {
                    writer.WriteArrayValue(static_cast<int64_t>(word));
                }

                writer.EndArray();
            }

            if (!message.SequenceName.empty())
            {
                writer.Write(KeySequence, message.SequenceName);
            }

            if (!message.TargetPageId.empty())
            {
                writer.Write(KeyTargetPage, message.TargetPageId);
            }

            if (!message.TargetLayerId.empty())
            {
                writer.Write(KeyTargetLayer, message.TargetLayerId);
            }

            WriteUnknown(writer, message.Unknown);
        }

        void WriteControl(_Inout_ JsonTextWriter& writer, _In_ Control const& control) noexcept
        {
            writer.BeginObject();

            writer.Write(KeyId, control.Id);
            writer.Write(KeyKind, NameOf(ControlKindNames, control.Kind));
            writer.Write(KeyLabel, control.Label);
            writer.Write(KeyX, control.X);
            writer.Write(KeyY, control.Y);
            writer.Write(KeyWidth, control.Width);
            writer.Write(KeyHeight, control.Height);
            writer.Write(KeyHueSlot, static_cast<int64_t>(control.HueSlot));

            if (!control.LiteralColor.empty())
            {
                writer.Write(KeyLiteralColor, control.LiteralColor);
            }

            writer.Write(KeyAspectLocked, control.AspectLocked);

            // Written only when the control actually disagrees with its theme.
            if (control.Style != ControlStyleOverride::UseTheme)
            {
                writer.Write(KeyStyle, NameOf(StyleNames, control.Style));
            }

            if (control.LabelPlaced != LabelPlacementOverride::UseTheme)
            {
                writer.Write(KeyLabelPlaced, NameOf(LabelPlacedNames, control.LabelPlaced));
            }

            if (control.ShowValue != ShowValueOverride::UseTheme)
            {
                writer.Write(KeyShowValue, NameOf(ShowValueNames, control.ShowValue));
            }

            writer.Write(KeyKeyboardOrder, static_cast<int64_t>(control.KeyboardOrder));
            writer.Write(KeyPickup, NameOf(PickupNames, control.Pickup));
            writer.Write(KeyDefaultValue, control.DefaultValue);
            writer.Write(KeySendsValueOnStart, control.SendsValueOnStart);
            writer.Write(KeySendInterval, static_cast<int64_t>(control.SendIntervalMilliseconds));

            writer.BeginArray(KeyMessages);

            for (auto const& message : control.Messages)
            {
                writer.BeginObject();
                WriteMessage(writer, message);
                writer.EndObject();
            }

            writer.EndArray();

            if (control.Feedback.Enabled || control.Feedback.Unknown != nullptr)
            {
                writer.BeginObject(KeyFeedback);
                writer.Write(KeyEnabled, control.Feedback.Enabled);
                writer.Write(KeyKind, NameOf(MessageKindNames, control.Feedback.Kind));
                writer.Write(KeyDevice, control.Feedback.DeviceName);
                writer.Write(KeyGroup, static_cast<int64_t>(control.Feedback.GroupIndex));
                writer.Write(KeyChannel, static_cast<int64_t>(control.Feedback.ChannelIndex));
                writer.Write(KeyNumber, static_cast<int64_t>(control.Feedback.Number));
                WriteUnknown(writer, control.Feedback.Unknown);
                writer.EndObject();
            }

            WriteUnknown(writer, control.Unknown);

            writer.EndObject();
        }
    }

    _Use_decl_annotations_
    std::wstring WriteLayoutToJson(LayoutDocument const& document) noexcept
    {
        try
        {
            JsonTextWriter writer{};

            writer.BeginObject();

            writer.Write(KeyComment, CommentText);
            writer.Write(KeyFileVersion, static_cast<int64_t>(document.FileVersion));
            writer.Write(KeyName, document.Name);
            writer.Write(KeyDescription, document.Description);
            writer.Write(KeyCreated, document.CreatedTimestamp);
            writer.Write(KeyModified, document.ModifiedTimestamp);
            writer.Write(KeyPageWidth, static_cast<int64_t>(document.PageWidth));
            writer.Write(KeyPageHeight, static_cast<int64_t>(document.PageHeight));
            writer.Write(KeyCanvasWidth, static_cast<int64_t>(document.CanvasWidth));
            writer.Write(KeyCanvasHeight, static_cast<int64_t>(document.CanvasHeight));
            writer.Write(KeyTheme, document.ThemeName);
            writer.Write(KeyScaleMode, NameOf(ScaleModeNames, document.Scale));
            writer.Write(KeyCustomScalePercent, document.CustomScalePercent);
            writer.Write(KeyCornerButton, NameOf(CornerNames, document.FullScreenButtonCorner));
            writer.Write(KeyPreferredDisplay, document.PreferredDisplayId);
            writer.Write(KeySuppressStartup, document.SuppressAllStartupValues);
            writer.Write(KeyVirtualDevice, document.PublishesVirtualDevice);
            writer.Write(KeyFavorite, document.IsFavorite);

            writer.BeginObject(KeyTempo);
            writer.Write(KeyKind, NameOf(TempoKindNames, document.Tempo.Kind));
            writer.Write(KeyBeatsPerMinute, document.Tempo.BeatsPerMinute);
            writer.Write(KeyDevice, document.Tempo.DeviceName);
            WriteUnknown(writer, document.Tempo.Unknown);
            writer.EndObject();

            writer.BeginArray(KeyDevices);

            for (auto const& device : document.Devices)
            {
                writer.BeginObject();
                writer.Write(KeyName, device.Name);
                writer.WriteRaw(KeyMatch, CanonicalJson(midiapp::MatchToJson(device.Match), writer.Depth()));
                writer.Write(KeyMatchMode, NameOf(MatchModeNames, device.MatchMode));
                writer.Write(KeySendsBeatClock, device.SendsBeatClock);
                WriteUnknown(writer, device.Unknown);
                writer.EndObject();
            }

            writer.EndArray();

            writer.BeginArray(KeyPages);

            for (auto const& page : document.Pages)
            {
                writer.BeginObject();
                writer.Write(KeyId, page.Id);
                writer.Write(KeyName, page.Name);
                writer.Write(KeyHueSlot, static_cast<int64_t>(page.HueSlot));
                writer.Write(KeySharedBand, page.IsSharedBand);

                writer.BeginArray(KeyControls);

                for (auto const& control : page.Controls)
                {
                    WriteControl(writer, control);
                }

                writer.EndArray();

                WriteUnknown(writer, page.Unknown);
                writer.EndObject();
            }

            writer.EndArray();

            writer.BeginArray(KeySequences);

            for (auto const& sequence : document.Sequences)
            {
                writer.BeginObject();
                writer.Write(KeyName, sequence.Name);

                writer.BeginArray(KeySteps);

                for (auto const& step : sequence.Steps)
                {
                    writer.BeginObject();
                    writer.Write(KeyKind, NameOf(StepKindNames, step.Kind));

                    if (step.Kind == SequenceStepKind::SendMidiMessage ||
                        step.Kind == SequenceStepKind::SendSystemExclusive)
                    {
                        writer.BeginObject(KeyMessage);
                        WriteMessage(writer, step.Message);
                        writer.EndObject();
                    }

                    if (step.Kind == SequenceStepKind::Wait)
                    {
                        writer.Write(KeyWaitMilliseconds, static_cast<int64_t>(step.WaitMilliseconds));
                    }

                    if (step.Kind == SequenceStepKind::RepeatBlockStart)
                    {
                        writer.Write(KeyRepeatCount, static_cast<int64_t>(step.RepeatCount));
                    }

                    if (step.Kind == SequenceStepKind::SetControlValue)
                    {
                        writer.Write(KeyTargetControl, step.TargetControlId);
                        writer.Write(KeyTargetValue, step.TargetValue);
                    }

                    WriteUnknown(writer, step.Unknown);
                    writer.EndObject();
                }

                writer.EndArray();

                WriteUnknown(writer, sequence.Unknown);
                writer.EndObject();
            }

            writer.EndArray();

            WriteUnknown(writer, document.Unknown);

            writer.EndObject();

            return writer.Text() + L"\n";
        }
        catch (...)
        {
            return {};
        }
    }
}
