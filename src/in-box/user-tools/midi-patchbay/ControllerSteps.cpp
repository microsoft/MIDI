// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Pure: no precompiled header, so the unit tests compile this file exactly as it ships.

#include <windows.h>

#include "ControllerSteps.h"

#include <algorithm>
#include <cmath>

namespace midipatchbay
{
    namespace
    {
        struct ControlInfo
        {
            ControllerControl Control;
            wchar_t const* Key;

            // XInput's bit for a button. Zero for anything else.
            uint16_t Bit;
        };

        constexpr ControlInfo Controls[] =
        {
            { ControllerControl::A, L"a", 0x1000 },
            { ControllerControl::B, L"b", 0x2000 },
            { ControllerControl::X, L"x", 0x4000 },
            { ControllerControl::Y, L"y", 0x8000 },
            { ControllerControl::LeftBumper, L"leftBumper", 0x0100 },
            { ControllerControl::RightBumper, L"rightBumper", 0x0200 },
            { ControllerControl::View, L"view", 0x0020 },
            { ControllerControl::Menu, L"menu", 0x0010 },
            { ControllerControl::LeftStickButton, L"leftStickButton", 0x0040 },
            { ControllerControl::RightStickButton, L"rightStickButton", 0x0080 },
            { ControllerControl::DPadUp, L"dpadUp", 0x0001 },
            { ControllerControl::DPadDown, L"dpadDown", 0x0002 },
            { ControllerControl::DPadLeft, L"dpadLeft", 0x0004 },
            { ControllerControl::DPadRight, L"dpadRight", 0x0008 },
            { ControllerControl::LeftTrigger, L"leftTrigger", 0 },
            { ControllerControl::RightTrigger, L"rightTrigger", 0 },
            { ControllerControl::LeftStickX, L"leftStickX", 0 },
            { ControllerControl::LeftStickY, L"leftStickY", 0 },
            { ControllerControl::RightStickX, L"rightStickX", 0 },
            { ControllerControl::RightStickY, L"rightStickY", 0 },
            { ControllerControl::LeftStickUp, L"leftStickUp", 0 },
            { ControllerControl::LeftStickDown, L"leftStickDown", 0 },
            { ControllerControl::LeftStickLeft, L"leftStickLeft", 0 },
            { ControllerControl::LeftStickRight, L"leftStickRight", 0 },
            { ControllerControl::RightStickUp, L"rightStickUp", 0 },
            { ControllerControl::RightStickDown, L"rightStickDown", 0 },
            { ControllerControl::RightStickLeft, L"rightStickLeft", 0 },
            { ControllerControl::RightStickRight, L"rightStickRight", 0 },
        };

        static_assert(std::size(Controls) == ControllerControlCount);

        constexpr bool ControlsAreInOrder() noexcept
        {
            for (size_t i = 0; i < std::size(Controls); i++)
            {
                if (static_cast<size_t>(Controls[i].Control) != i)
                {
                    return false;
                }
            }

            return true;
        }

        static_assert(ControlsAreInOrder());

        ControlInfo const* FindControl(_In_ ControllerControl control) noexcept
        {
            auto const index = static_cast<size_t>(control);

            return index < std::size(Controls) ? &Controls[index] : nullptr;
        }

        struct MessageKindInfo
        {
            ControllerMessageKind Kind;
            wchar_t const* Key;
        };

        constexpr MessageKindInfo MessageKinds[] =
        {
            { ControllerMessageKind::Nothing, L"nothing" },
            { ControllerMessageKind::Note, L"note" },
            { ControllerMessageKind::ControlChange, L"controlChange" },
            { ControllerMessageKind::ProgramChange, L"programChange" },
            { ControllerMessageKind::PitchBend, L"pitchBend" },
            { ControllerMessageKind::ChannelPressure, L"channelPressure" },
            { ControllerMessageKind::PolyPressure, L"polyPressure" },
            { ControllerMessageKind::RegisteredController, L"rpn" },
            { ControllerMessageKind::AssignableController, L"nrpn" },
            { ControllerMessageKind::Start, L"start" },
            { ControllerMessageKind::Continue, L"continue" },
            { ControllerMessageKind::Stop, L"stop" },
            { ControllerMessageKind::Words, L"words" },
        };

        static_assert(std::size(MessageKinds) == ControllerMessageKindCount);

        constexpr wchar_t const* ProgramStepKeys[] = { L"this", L"next", L"previous" };

        // A trigger counts as pressed past half its travel, and a stick direction past half the
        // way from the middle to that end.
        constexpr double TriggerDownAt = 0.5;
        constexpr double StickDownAt = 0.25;

        // Learn only takes a move that is clearly on purpose, from a control that was resting.
        constexpr double LearnTriggerFrom = 0.25;
        constexpr double LearnStickFrom = 0.2;
        constexpr double LearnStickTo = 0.35;

        constexpr uint8_t StatusNoteOff = 0x8;
        constexpr uint8_t StatusNoteOn = 0x9;
        constexpr uint8_t StatusProgramChange = 0xC;

        constexpr uint8_t StatusStart = 0xFA;
        constexpr uint8_t StatusContinue = 0xFB;
        constexpr uint8_t StatusStop = 0xFC;

        constexpr wchar_t KeyPlayer[] = L"player";
        constexpr wchar_t KeyControl[] = L"control";
        constexpr wchar_t KeyPressed[] = L"pressed";
        constexpr wchar_t KeyToggles[] = L"toggles";
        constexpr wchar_t KeyHeld[] = L"held";
        constexpr wchar_t KeyHoldMilliseconds[] = L"holdMilliseconds";
        constexpr wchar_t KeyRepeats[] = L"repeats";
        constexpr wchar_t KeyRepeatMilliseconds[] = L"repeatMilliseconds";
        constexpr wchar_t KeyTapOrHold[] = L"tapOrHold";
        constexpr wchar_t KeyScale[] = L"valueScale";
        constexpr wchar_t KeyMessage[] = L"message";
        constexpr wchar_t KeyGroup[] = L"group";
        constexpr wchar_t KeyChannel[] = L"channel";
        constexpr wchar_t KeyNumber[] = L"number";
        constexpr wchar_t KeyVelocityPercent[] = L"velocityPercent";
        constexpr wchar_t KeyNoteOff[] = L"noteOff";
        constexpr wchar_t KeyValuePercent[] = L"valuePercent";
        constexpr wchar_t KeySendRelease[] = L"sendRelease";
        constexpr wchar_t KeyReleasePercent[] = L"releasePercent";
        constexpr wchar_t KeyMidi1[] = L"midi1";
        constexpr wchar_t KeyStep[] = L"step";
        constexpr wchar_t KeyLowest[] = L"lowest";
        constexpr wchar_t KeyHighest[] = L"highest";
        constexpr wchar_t KeyWraps[] = L"wraps";
        constexpr wchar_t KeyMessageWords[] = L"messageWords";
        constexpr wchar_t KeyDeadZonePercent[] = L"deadZonePercent";
        constexpr wchar_t KeyShape[] = L"shape";
        constexpr wchar_t KeyIntervalMilliseconds[] = L"intervalMilliseconds";
        constexpr wchar_t KeyReturnToRest[] = L"returnToRest";
        constexpr wchar_t KeyDevice[] = L"device";
        constexpr wchar_t KeyName[] = L"name";
        constexpr wchar_t KeyVendorId[] = L"vendorId";
        constexpr wchar_t KeyProductId[] = L"productId";
        constexpr wchar_t KeyPath[] = L"path";

        constexpr size_t MaximumDeviceNameLength = 256;
        constexpr size_t MaximumDevicePathLength = 1024;

        constexpr wchar_t ScaleSevenBit[] = L"sevenBit";
        constexpr wchar_t ScalePercent[] = L"percent";

        // ----------------------------------------------------------------------- the file

        json::JsonValue GetValue(_In_ json::JsonObject const& object, _In_ std::wstring_view key) noexcept
        {
            try
            {
                if (object != nullptr && object.HasKey(key))
                {
                    return object.GetNamedValue(key);
                }
            }
            catch (...)
            {
            }

            return nullptr;
        }

        double ReadNumber(
            _In_ json::JsonObject const& object,
            _In_ std::wstring_view key,
            _In_ double lowest,
            _In_ double highest,
            _In_ double fallback) noexcept
        {
            try
            {
                auto const value = GetValue(object, key);

                if (value != nullptr && value.ValueType() == json::JsonValueType::Number)
                {
                    auto const number = value.GetNumber();

                    if (std::isfinite(number) && number >= lowest && number <= highest)
                    {
                        return number;
                    }
                }
            }
            catch (...)
            {
            }

            return fallback;
        }

        // Whole numbers only.
        uint32_t ReadWhole(
            _In_ json::JsonObject const& object,
            _In_ std::wstring_view key,
            _In_ uint32_t highest,
            _In_ uint32_t fallback) noexcept
        {
            auto const number = ReadNumber(object, key, 0, highest, -1);

            return number < 0 || std::floor(number) != number ? fallback : static_cast<uint32_t>(number);
        }

        // Whole numbers from lowest to highest only. Anything else is the fallback.
        int32_t ReadWholeBetween(
            _In_ json::JsonObject const& object,
            _In_ std::wstring_view key,
            _In_ int32_t lowest,
            _In_ int32_t highest,
            _In_ int32_t fallback) noexcept
        {
            auto const number = ReadNumber(object, key, lowest, highest, lowest - 1.0);

            return number < lowest || std::floor(number) != number ? fallback : static_cast<int32_t>(number);
        }

        bool ReadBool(_In_ json::JsonObject const& object, _In_ std::wstring_view key, _In_ bool fallback) noexcept
        {
            try
            {
                auto const value = GetValue(object, key);

                if (value != nullptr && value.ValueType() == json::JsonValueType::Boolean)
                {
                    return value.GetBoolean();
                }
            }
            catch (...)
            {
            }

            return fallback;
        }

        std::wstring ReadString(_In_ json::JsonObject const& object, _In_ std::wstring_view key) noexcept
        {
            try
            {
                auto const value = GetValue(object, key);

                if (value != nullptr && value.ValueType() == json::JsonValueType::String)
                {
                    return std::wstring{ value.GetString() };
                }
            }
            catch (...)
            {
            }

            return {};
        }

        json::JsonObject ReadObject(_In_ json::JsonObject const& object, _In_ std::wstring_view key) noexcept
        {
            try
            {
                auto const value = GetValue(object, key);

                if (value != nullptr && value.ValueType() == json::JsonValueType::Object)
                {
                    return value.GetObject();
                }
            }
            catch (...)
            {
            }

            return nullptr;
        }

        int32_t ReadPercent(_In_ json::JsonObject const& object, _In_ std::wstring_view key, _In_ int32_t fallback) noexcept
        {
            auto const percent = ReadNumber(object, key, 0, 100, -1);

            return percent < 0 ? fallback : static_cast<int32_t>(std::lround(percent * 100.0));
        }

        void SetNumber(_In_ json::JsonObject const& object, _In_ std::wstring_view key, _In_ double value)
        {
            object.SetNamedValue(key, json::JsonValue::CreateNumberValue(value));
        }

        void SetBool(_In_ json::JsonObject const& object, _In_ std::wstring_view key, _In_ bool value)
        {
            object.SetNamedValue(key, json::JsonValue::CreateBooleanValue(value));
        }

        void SetString(_In_ json::JsonObject const& object, _In_ std::wstring_view key, _In_ std::wstring_view value)
        {
            object.SetNamedValue(key, json::JsonValue::CreateStringValue(value));
        }

        void SetPercent(_In_ json::JsonObject const& object, _In_ std::wstring_view key, _In_ int32_t hundredths)
        {
            SetNumber(object, key, std::clamp(hundredths, 0, FullScaleHundredths) / 100.0);
        }

        ValueScale ReadScale(_In_ json::JsonObject const& object) noexcept
        {
            return ReadString(object, KeyScale) == ScaleSevenBit ? ValueScale::SevenBit : ValueScale::Percent;
        }

        uint8_t ReadPlayer(_In_ json::JsonObject const& object) noexcept
        {
            return static_cast<uint8_t>(ReadWhole(object, KeyPlayer, ControllerPlayerCount - 1, 0));
        }

        ControllerControl ReadControl(
            _In_ json::JsonObject const& object,
            _In_ bool forButton,
            _In_ ControllerControl fallback) noexcept
        {
            auto const control = ControllerControlFromKey(ReadString(object, KeyControl));

            if (!control.has_value() || (forButton ? !CanBePressed(*control) : !CanSlide(*control)))
            {
                return fallback;
            }

            return *control;
        }

        json::JsonObject DeviceToJson(_In_ HidDeviceId const& device)
        {
            json::JsonObject item{};

            SetString(item, KeyName, device.Name);
            SetNumber(item, KeyVendorId, device.VendorId);
            SetNumber(item, KeyProductId, device.ProductId);

            if (!device.Path.empty())
            {
                SetString(item, KeyPath, device.Path);
            }

            return item;
        }

        HidDeviceId DeviceFromJson(_In_ json::JsonObject const& item) noexcept
        {
            HidDeviceId device{};

            if (item == nullptr)
            {
                return device;
            }

            device.Name = ReadString(item, KeyName);
            device.Path = ReadString(item, KeyPath);
            device.VendorId = static_cast<uint16_t>(ReadWhole(item, KeyVendorId, 0xFFFF, 0));
            device.ProductId = static_cast<uint16_t>(ReadWhole(item, KeyProductId, 0xFFFF, 0));

            if (device.Name.size() > MaximumDeviceNameLength)
            {
                device.Name.resize(MaximumDeviceNameLength);
            }

            if (device.Path.size() > MaximumDevicePathLength)
            {
                device.Path.clear();
            }

            return device;
        }

        HidControl ReadDeviceControl(_In_ json::JsonObject const& object, _In_ bool forButton, _In_ HidControl fallback) noexcept
        {
            auto const control = HidControlFromKey(ReadString(object, KeyControl));

            if (!control.has_value() || (forButton ? !CanBePressed(*control) : !CanSlide(*control)))
            {
                return fallback;
            }

            return *control;
        }

        // The file has the player or the device, whichever the step reads.
        void WriteSource(
            _In_ json::JsonObject const& object,
            _In_ uint8_t player,
            _In_ ControllerControl control,
            _In_ HidDeviceId const& device,
            _In_ HidControl deviceControl)
        {
            if (device.IsEmpty())
            {
                SetNumber(object, KeyPlayer, player);
                SetString(object, KeyControl, ControllerControlKey(control));
                return;
            }

            object.SetNamedValue(KeyDevice, DeviceToJson(device));
            SetString(object, KeyControl, HidControlKey(deviceControl));
        }

        std::wstring SourceSignature(
            _In_ uint8_t player,
            _In_ ControllerControl control,
            _In_ HidDeviceId const& device,
            _In_ HidControl deviceControl)
        {
            if (device.IsEmpty())
            {
                return L"p" + std::to_wstring(player) + L'.' + std::wstring{ ControllerControlKey(control) };
            }

            return L"d" + std::to_wstring(device.VendorId) + L':' + std::to_wstring(device.ProductId) + L'.' +
                std::to_wstring(device.Name.size()) + L':' + device.Name + L'.' + device.Path + L'.' + HidControlKey(deviceControl);
        }

        // How many words a whole message of this type takes, from the type in its first word.
        uint8_t WordsForType(_In_ uint32_t firstWord) noexcept
        {
            constexpr uint8_t sizes[16] = { 1, 1, 1, 2, 2, 4, 1, 1, 2, 2, 2, 3, 3, 4, 4, 4 };

            return sizes[(firstWord >> 28) & 0x0F];
        }

        void MessageToJson(_In_ ControllerMessage const& message, _In_ json::JsonObject const& item)
        {
            SetString(item, KeyMessage, ControllerMessageKindKey(message.Kind));

            switch (message.Kind)
            {
            case ControllerMessageKind::Note:
                SetNumber(item, KeyGroup, message.Group);
                SetNumber(item, KeyChannel, message.Channel);
                SetNumber(item, KeyNumber, message.Number);
                SetPercent(item, KeyVelocityPercent, message.ValueHundredths);
                SetBool(item, KeyNoteOff, message.SendsRelease);
                SetBool(item, KeyMidi1, message.Midi1Protocol);
                break;

            case ControllerMessageKind::ControlChange:
            case ControllerMessageKind::PitchBend:
            case ControllerMessageKind::ChannelPressure:
            case ControllerMessageKind::PolyPressure:
            case ControllerMessageKind::RegisteredController:
            case ControllerMessageKind::AssignableController:
                SetNumber(item, KeyGroup, message.Group);
                SetNumber(item, KeyChannel, message.Channel);

                if (ControllerMessageNumberMaximum(message.Kind) != 0)
                {
                    SetNumber(item, KeyNumber, message.Number);
                }

                SetPercent(item, KeyValuePercent, message.ValueHundredths);
                SetBool(item, KeySendRelease, message.SendsRelease);

                if (message.SendsRelease)
                {
                    SetPercent(item, KeyReleasePercent, message.ReleaseHundredths);
                }

                if (message.Kind != ControllerMessageKind::RegisteredController &&
                    message.Kind != ControllerMessageKind::AssignableController)
                {
                    SetBool(item, KeyMidi1, message.Midi1Protocol);
                }
                break;

            case ControllerMessageKind::ProgramChange:
                SetNumber(item, KeyGroup, message.Group);
                SetNumber(item, KeyChannel, message.Channel);
                SetString(item, KeyStep, ProgramStepKeys[static_cast<size_t>(message.Step)]);

                if (message.Step == ProgramStep::This)
                {
                    SetNumber(item, KeyNumber, message.Number);
                }
                else
                {
                    SetNumber(item, KeyLowest, message.LowestProgram);
                    SetNumber(item, KeyHighest, message.HighestProgram);
                    SetBool(item, KeyWraps, message.Wraps);
                }

                SetBool(item, KeyMidi1, message.Midi1Protocol);
                break;

            case ControllerMessageKind::Start:
            case ControllerMessageKind::Continue:
            case ControllerMessageKind::Stop:
                SetNumber(item, KeyGroup, message.Group);
                break;

            case ControllerMessageKind::Words:
            {
                json::JsonArray words{};

                for (uint8_t i = 0; i < message.WordCount && i < MaximumControllerWords; i++)
                {
                    words.Append(json::JsonValue::CreateNumberValue(message.Words[i]));
                }

                item.SetNamedValue(KeyMessageWords, words);
                break;
            }

            default:
                break;
            }
        }

        ControllerMessage MessageFromJson(_In_ json::JsonObject const& item, _In_ ControllerMessageKind fallbackKind) noexcept
        {
            if (item == nullptr)
            {
                return ControllerMessageFor(fallbackKind);
            }

            auto const kind = ControllerMessageKindFromKey(ReadString(item, KeyMessage)).value_or(fallbackKind);
            auto message = ControllerMessageFor(kind);

            message.Group = static_cast<uint8_t>(ReadWhole(item, KeyGroup, 15, 0));
            message.Channel = static_cast<uint8_t>(ReadWhole(item, KeyChannel, 15, 0));
            message.Midi1Protocol = kind != ControllerMessageKind::RegisteredController &&
                kind != ControllerMessageKind::AssignableController &&
                ReadBool(item, KeyMidi1, false);

            switch (kind)
            {
            case ControllerMessageKind::Note:
                message.Number = ReadWhole(item, KeyNumber, 127, message.Number);
                message.ValueHundredths = ReadPercent(item, KeyVelocityPercent, message.ValueHundredths);
                message.SendsRelease = ReadBool(item, KeyNoteOff, message.SendsRelease);
                break;

            case ControllerMessageKind::ControlChange:
            case ControllerMessageKind::PitchBend:
            case ControllerMessageKind::ChannelPressure:
            case ControllerMessageKind::PolyPressure:
            case ControllerMessageKind::RegisteredController:
            case ControllerMessageKind::AssignableController:
            {
                auto const maximum = ControllerMessageNumberMaximum(kind);

                if (maximum != 0)
                {
                    message.Number = ReadWhole(item, KeyNumber, maximum, message.Number);
                }

                message.ValueHundredths = ReadPercent(item, KeyValuePercent, message.ValueHundredths);
                message.SendsRelease = ReadBool(item, KeySendRelease, message.SendsRelease);
                message.ReleaseHundredths = ReadPercent(item, KeyReleasePercent, message.ReleaseHundredths);
                break;
            }

            case ControllerMessageKind::ProgramChange:
            {
                auto const step = ReadString(item, KeyStep);

                message.Step = step == ProgramStepKeys[1] ? ProgramStep::Next
                    : step == ProgramStepKeys[2] ? ProgramStep::Previous
                    : ProgramStep::This;
                message.Number = ReadWhole(item, KeyNumber, 127, message.Number);

                // Not put in order: SteppedProgram does, and the file keeps what was typed.
                message.LowestProgram = static_cast<uint8_t>(ReadWhole(item, KeyLowest, 127, message.LowestProgram));
                message.HighestProgram = static_cast<uint8_t>(ReadWhole(item, KeyHighest, 127, message.HighestProgram));
                message.Wraps = ReadBool(item, KeyWraps, message.Wraps);
                break;
            }

            case ControllerMessageKind::Words:
            {
                try
                {
                    auto const value = GetValue(item, KeyMessageWords);

                    if (value != nullptr && value.ValueType() == json::JsonValueType::Array)
                    {
                        uint8_t count{ 0 };

                        for (auto const& word : value.GetArray())
                        {
                            if (count >= MaximumControllerWords || word == nullptr ||
                                word.ValueType() != json::JsonValueType::Number)
                            {
                                break;
                            }

                            auto const number = word.GetNumber();

                            if (!std::isfinite(number) || number < 0 || number > 4294967295.0 || std::floor(number) != number)
                            {
                                break;
                            }

                            message.Words[count++] = static_cast<uint32_t>(number);
                        }

                        message.WordCount = (std::max)(count, uint8_t{ 1 });
                    }
                }
                catch (...)
                {
                }
                break;
            }

            default:
                break;
            }

            return message;
        }

        std::wstring MessageSignature(_In_ ControllerMessage const& message)
        {
            auto signature = std::wstring{ ControllerMessageKindKey(message.Kind) } +
                L".g" + std::to_wstring(message.Group) + L"c" + std::to_wstring(message.Channel) +
                L"n" + std::to_wstring(message.Number) + L"v" + std::to_wstring(message.ValueHundredths) +
                (message.SendsRelease ? L"r" : L"-") + std::to_wstring(message.ReleaseHundredths) +
                (message.Midi1Protocol ? L".m1" : L".m2") +
                L".p" + std::to_wstring(static_cast<int32_t>(message.Step)) + L':' +
                std::to_wstring(message.LowestProgram) + L'-' + std::to_wstring(message.HighestProgram) +
                (message.Wraps ? L"w" : L"-") + L".w";

            for (uint8_t i = 0; i < message.WordCount && i < MaximumControllerWords; i++)
            {
                signature += std::to_wstring(message.Words[i]) + L',';
            }

            return signature;
        }
    }

    _Use_decl_annotations_
    bool IsControllerButton(ControllerControl control) noexcept
    {
        return control >= ControllerControl::A && control <= ControllerControl::DPadRight;
    }

    _Use_decl_annotations_
    bool IsControllerTrigger(ControllerControl control) noexcept
    {
        return control == ControllerControl::LeftTrigger || control == ControllerControl::RightTrigger;
    }

    _Use_decl_annotations_
    bool IsControllerStick(ControllerControl control) noexcept
    {
        return control >= ControllerControl::LeftStickX && control <= ControllerControl::RightStickY;
    }

    _Use_decl_annotations_
    bool IsControllerStickDirection(ControllerControl control) noexcept
    {
        return control >= ControllerControl::LeftStickUp && control <= ControllerControl::RightStickRight;
    }

    _Use_decl_annotations_
    bool CanBePressed(ControllerControl control) noexcept
    {
        return IsControllerButton(control) || IsControllerTrigger(control) || IsControllerStickDirection(control);
    }

    _Use_decl_annotations_
    bool CanSlide(ControllerControl control) noexcept
    {
        return IsControllerTrigger(control) || IsControllerStick(control);
    }

    _Use_decl_annotations_
    std::wstring_view ControllerControlKey(ControllerControl control) noexcept
    {
        auto const* info = FindControl(control);

        return info == nullptr ? std::wstring_view{} : std::wstring_view{ info->Key };
    }

    _Use_decl_annotations_
    std::optional<ControllerControl> ControllerControlFromKey(std::wstring_view key) noexcept
    {
        for (auto const& info : Controls)
        {
            if (key == info.Key)
            {
                return info.Control;
            }
        }

        return std::nullopt;
    }

    _Use_decl_annotations_
    ControllerReading ControllerReadingFromXInput(
        uint16_t buttons,
        uint8_t leftTrigger,
        uint8_t rightTrigger,
        int16_t leftStickX,
        int16_t leftStickY,
        int16_t rightStickX,
        int16_t rightStickY) noexcept
    {
        auto const stick = [](int16_t value) { return (static_cast<double>(value) + 32768.0) / 65535.0; };

        ControllerReading reading{};

        reading.Connected = true;
        reading.Buttons = buttons;
        reading.LeftTrigger = leftTrigger / 255.0;
        reading.RightTrigger = rightTrigger / 255.0;
        reading.LeftStickX = stick(leftStickX);
        reading.LeftStickY = stick(leftStickY);
        reading.RightStickX = stick(rightStickX);
        reading.RightStickY = stick(rightStickY);

        return reading;
    }

    _Use_decl_annotations_
    double ControlValue(ControllerReading const& reading, ControllerControl control) noexcept
    {
        switch (control)
        {
        case ControllerControl::LeftTrigger:    return reading.LeftTrigger;
        case ControllerControl::RightTrigger:   return reading.RightTrigger;
        case ControllerControl::LeftStickX:     return reading.LeftStickX;
        case ControllerControl::LeftStickY:     return reading.LeftStickY;
        case ControllerControl::RightStickX:    return reading.RightStickX;
        case ControllerControl::RightStickY:    return reading.RightStickY;
        default:                                return ControlIsDown(reading, control) ? 1.0 : 0.0;
        }
    }

    _Use_decl_annotations_
    bool ControlIsDown(ControllerReading const& reading, ControllerControl control) noexcept
    {
        if (!reading.Connected)
        {
            return false;
        }

        switch (control)
        {
        case ControllerControl::LeftTrigger:        return reading.LeftTrigger >= TriggerDownAt;
        case ControllerControl::RightTrigger:       return reading.RightTrigger >= TriggerDownAt;
        case ControllerControl::LeftStickUp:        return reading.LeftStickY >= 1.0 - StickDownAt;
        case ControllerControl::LeftStickDown:      return reading.LeftStickY <= StickDownAt;
        case ControllerControl::LeftStickLeft:      return reading.LeftStickX <= StickDownAt;
        case ControllerControl::LeftStickRight:     return reading.LeftStickX >= 1.0 - StickDownAt;
        case ControllerControl::RightStickUp:       return reading.RightStickY >= 1.0 - StickDownAt;
        case ControllerControl::RightStickDown:     return reading.RightStickY <= StickDownAt;
        case ControllerControl::RightStickLeft:     return reading.RightStickX <= StickDownAt;
        case ControllerControl::RightStickRight:    return reading.RightStickX >= 1.0 - StickDownAt;
        default:
        {
            auto const* info = FindControl(control);

            return info != nullptr && info->Bit != 0 && (reading.Buttons & info->Bit) != 0;
        }
        }
    }

    _Use_decl_annotations_
    double ControlRestValue(ControllerControl control) noexcept
    {
        return IsControllerStick(control) ? 0.5 : 0.0;
    }

    _Use_decl_annotations_
    std::optional<ControllerControl> FindMovedControl(
        ControllerReading const& before,
        ControllerReading const& now,
        bool forButton) noexcept
    {
        if (!now.Connected)
        {
            return std::nullopt;
        }

        if (forButton)
        {
            for (auto const& info : Controls)
            {
                if (IsControllerButton(info.Control) && (now.Buttons & info.Bit) != 0 && (before.Buttons & info.Bit) == 0)
                {
                    return info.Control;
                }
            }
        }

        for (auto const trigger : { ControllerControl::LeftTrigger, ControllerControl::RightTrigger })
        {
            if (ControlValue(now, trigger) >= TriggerDownAt && ControlValue(before, trigger) < LearnTriggerFrom)
            {
                return trigger;
            }
        }

        for (auto const& info : Controls)
        {
            if (forButton && IsControllerStickDirection(info.Control))
            {
                // The stick this direction belongs to was resting before.
                auto const axis = info.Control == ControllerControl::LeftStickUp || info.Control == ControllerControl::LeftStickDown
                    ? ControllerControl::LeftStickY
                    : info.Control == ControllerControl::LeftStickLeft || info.Control == ControllerControl::LeftStickRight
                    ? ControllerControl::LeftStickX
                    : info.Control == ControllerControl::RightStickUp || info.Control == ControllerControl::RightStickDown
                    ? ControllerControl::RightStickY
                    : ControllerControl::RightStickX;

                if (ControlIsDown(now, info.Control) && std::abs(ControlValue(before, axis) - 0.5) < LearnStickFrom)
                {
                    return info.Control;
                }
            }
            else if (!forButton && IsControllerStick(info.Control))
            {
                if (std::abs(ControlValue(now, info.Control) - 0.5) >= LearnStickTo &&
                    std::abs(ControlValue(before, info.Control) - 0.5) < LearnStickFrom)
                {
                    return info.Control;
                }
            }
        }

        return std::nullopt;
    }

    _Use_decl_annotations_
    std::wstring_view ControllerMessageKindKey(ControllerMessageKind kind) noexcept
    {
        for (auto const& info : MessageKinds)
        {
            if (info.Kind == kind)
            {
                return info.Key;
            }
        }

        return MessageKinds[0].Key;
    }

    _Use_decl_annotations_
    std::optional<ControllerMessageKind> ControllerMessageKindFromKey(std::wstring_view key) noexcept
    {
        for (auto const& info : MessageKinds)
        {
            if (key == info.Key)
            {
                return info.Kind;
            }
        }

        return std::nullopt;
    }

    _Use_decl_annotations_
    bool IsChannelMessageKind(ControllerMessageKind kind) noexcept
    {
        return kind >= ControllerMessageKind::Note && kind <= ControllerMessageKind::AssignableController;
    }

    _Use_decl_annotations_
    bool IsValueMessageKind(ControllerMessageKind kind) noexcept
    {
        return kind == ControllerMessageKind::ControlChange || kind == ControllerMessageKind::PitchBend ||
            kind == ControllerMessageKind::ChannelPressure || kind == ControllerMessageKind::PolyPressure ||
            kind == ControllerMessageKind::RegisteredController || kind == ControllerMessageKind::AssignableController;
    }

    _Use_decl_annotations_
    uint32_t ControllerMessageNumberMaximum(ControllerMessageKind kind) noexcept
    {
        switch (kind)
        {
        case ControllerMessageKind::Note:
        case ControllerMessageKind::ControlChange:
        case ControllerMessageKind::PolyPressure:
        case ControllerMessageKind::ProgramChange:
            return 127;

        case ControllerMessageKind::RegisteredController:
        case ControllerMessageKind::AssignableController:
            return midiapp::MaximumValueMessageNumber;

        default:
            return 0;
        }
    }

    bool ControllerButtonSettings::SendsWhenHeld() const noexcept
    {
        return !Toggles && Held.Kind != ControllerMessageKind::Nothing;
    }

    _Use_decl_annotations_
    ControllerMessage ControllerMessageFor(ControllerMessageKind kind) noexcept
    {
        ControllerMessage message{};

        message.Kind = kind;
        message.Number = 0;
        message.ValueHundredths = FullScaleHundredths;
        message.SendsRelease = true;
        message.ReleaseHundredths = 0;

        switch (kind)
        {
        case ControllerMessageKind::Note:
            message.Number = DefaultControllerNote;
            message.ValueHundredths = DefaultControllerVelocityHundredths;
            break;

        // The sustain pedal: down while the button is.
        case ControllerMessageKind::ControlChange:
            message.Number = 64;
            break;

        case ControllerMessageKind::PolyPressure:
            message.Number = DefaultControllerNote;
            break;

        // Springs back to the middle when let go.
        case ControllerMessageKind::PitchBend:
            message.ReleaseHundredths = FullScaleHundredths / 2;
            break;

        // A parameter is set, not held.
        case ControllerMessageKind::RegisteredController:
        case ControllerMessageKind::AssignableController:
            message.SendsRelease = false;
            break;

        default:
            break;
        }

        return message;
    }

    _Use_decl_annotations_
    uint8_t SteppedProgram(int32_t current, ProgramStep step, uint8_t lowest, uint8_t highest, bool wraps) noexcept
    {
        auto const low = static_cast<int32_t>((std::min)((std::min)(lowest, highest), uint8_t{ 127 }));
        auto const high = static_cast<int32_t>((std::min)((std::max)(lowest, highest), uint8_t{ 127 }));

        if (current < 0)
        {
            return static_cast<uint8_t>(low);
        }

        if (current < low || current > high)
        {
            return static_cast<uint8_t>(step == ProgramStep::Previous ? high : low);
        }

        if (step == ProgramStep::Previous)
        {
            return static_cast<uint8_t>(current > low ? current - 1 : (wraps ? high : low));
        }

        return static_cast<uint8_t>(current < high ? current + 1 : (wraps ? low : high));
    }

    _Use_decl_annotations_
    ControllerWords BuildControllerWords(ControllerMessage const& message, uint8_t program) noexcept
    {
        ControllerWords words{};

        auto const group = static_cast<uint8_t>(message.Group & 0x0F);
        auto const channel = static_cast<uint8_t>(message.Channel & 0x0F);
        auto const value = std::clamp(message.ValueHundredths, 0, FullScaleHundredths) / static_cast<double>(FullScaleHundredths);
        auto const release = std::clamp(message.ReleaseHundredths, 0, FullScaleHundredths) / static_cast<double>(FullScaleHundredths);

        switch (message.Kind)
        {
        case ControllerMessageKind::Note:
        {
            auto const note = static_cast<uint8_t>((std::min)(message.Number, 127u));

            if (message.Midi1Protocol)
            {
                // A MIDI 1.0 note on at velocity 0 is a note off.
                auto const velocity = (std::max)(midiapp::ScaleToBits(value, 7), 1u);

                words.On[0] = midiapp::BuildMidi1ChannelVoice(group, StatusNoteOn, channel, note, static_cast<uint8_t>(velocity));
                words.OnCount = 1;

                if (message.SendsRelease)
                {
                    words.Off[0] = midiapp::BuildMidi1ChannelVoice(group, StatusNoteOff, channel, note, 0);
                    words.OffCount = 1;
                }
            }
            else
            {
                auto const velocity = (std::max)(midiapp::ScaleToBits(value, 16), 1u);

                midiapp::BuildMidi2ChannelVoice(group, StatusNoteOn, channel, note, 0, velocity << 16, words.On.data());
                words.OnCount = 2;

                if (message.SendsRelease)
                {
                    midiapp::BuildMidi2ChannelVoice(group, StatusNoteOff, channel, note, 0, 0, words.Off.data());
                    words.OffCount = 2;
                }
            }
            break;
        }

        case ControllerMessageKind::ControlChange:
        case ControllerMessageKind::PitchBend:
        case ControllerMessageKind::ChannelPressure:
        case ControllerMessageKind::PolyPressure:
        case ControllerMessageKind::RegisteredController:
        case ControllerMessageKind::AssignableController:
        {
            midiapp::ValueMessageTarget target{};

            target.Kind = message.Kind == ControllerMessageKind::ControlChange ? midiapp::ValueMessageKind::ControlChange
                : message.Kind == ControllerMessageKind::PitchBend ? midiapp::ValueMessageKind::PitchBend
                : message.Kind == ControllerMessageKind::ChannelPressure ? midiapp::ValueMessageKind::ChannelPressure
                : message.Kind == ControllerMessageKind::PolyPressure ? midiapp::ValueMessageKind::PolyPressure
                : message.Kind == ControllerMessageKind::RegisteredController ? midiapp::ValueMessageKind::RegisteredController
                : midiapp::ValueMessageKind::AssignableController;
            target.Group = group;
            target.Channel = channel;
            target.Number = message.Number;
            target.Midi1Protocol = message.Midi1Protocol;

            words.OnCount = static_cast<uint8_t>(midiapp::BuildValueMessage(target, value, words.On.data()));

            if (message.SendsRelease)
            {
                words.OffCount = static_cast<uint8_t>(midiapp::BuildValueMessage(target, release, words.Off.data()));
            }
            break;
        }

        case ControllerMessageKind::ProgramChange:
            if (message.Midi1Protocol)
            {
                words.On[0] = midiapp::BuildMidi1ChannelVoice(group, StatusProgramChange, channel, program & 0x7F, 0);
                words.OnCount = 1;
            }
            else
            {
                // No bank: the bank valid bit stays clear.
                midiapp::BuildMidi2ChannelVoice(group, StatusProgramChange, channel, 0, 0,
                    static_cast<uint32_t>(program & 0x7F) << 24, words.On.data());
                words.OnCount = 2;
            }
            break;

        case ControllerMessageKind::Start:
        case ControllerMessageKind::Continue:
        case ControllerMessageKind::Stop:
        {
            auto const status = message.Kind == ControllerMessageKind::Start ? StatusStart
                : message.Kind == ControllerMessageKind::Continue ? StatusContinue
                : StatusStop;

            words.On[0] = (0x1u << 28) | (static_cast<uint32_t>(group) << 24) | (static_cast<uint32_t>(status) << 16);
            words.OnCount = 1;
            break;
        }

        case ControllerMessageKind::Words:
            // Only a whole message: a count that doesn't match its type would split or join
            // messages on the way out.
            if (message.WordCount >= 1 && message.WordCount <= MaximumControllerWords &&
                message.WordCount == WordsForType(message.Words[0]))
            {
                std::copy_n(message.Words.begin(), message.WordCount, words.On.begin());
                words.OnCount = message.WordCount;
            }
            break;

        default:
            break;
        }

        return words;
    }

    _Use_decl_annotations_
    ControllerButtonMachine::ControllerButtonMachine(ProgramPositionFor programs) :
        m_programs(std::move(programs))
    {
    }

    _Use_decl_annotations_
    ControllerWords ControllerButtonMachine::Start(ControllerMessage const& message)
    {
        uint8_t program{ 0 };

        if (message.Kind == ControllerMessageKind::ProgramChange)
        {
            if (message.Step == ProgramStep::This)
            {
                program = static_cast<uint8_t>((std::min)(message.Number, 127u));
            }
            else
            {
                auto const position = m_programs
                    ? m_programs(static_cast<uint8_t>(message.Group & 0x0F), static_cast<uint8_t>(message.Channel & 0x0F))
                    : nullptr;

                if (position == nullptr)
                {
                    program = SteppedProgram(-1, message.Step, message.LowestProgram, message.HighestProgram, message.Wraps);
                }
                else
                {
                    auto current = position->load();

                    do
                    {
                        program = SteppedProgram(current, message.Step, message.LowestProgram, message.HighestProgram, message.Wraps);
                    }
                    while (!position->compare_exchange_weak(current, program));
                }
            }
        }

        return BuildControllerWords(message, program);
    }

    _Use_decl_annotations_
    void ControllerButtonMachine::Send(ControllerSink const& sink, Pending& pending)
    {
        if (pending.Count > 0 && sink)
        {
            sink(pending.Words.data(), pending.Count);
        }

        pending.Count = 0;
    }

    _Use_decl_annotations_
    void ControllerButtonMachine::Update(
        ControllerButtonSettings const& settings,
        uint64_t nowMilliseconds,
        bool down,
        ControllerSink const& sink)
    {
        auto const startMessage = [this, &sink](ControllerMessage const& message, Pending& off)
            {
                auto const words = Start(message);

                if (words.OnCount > 0 && sink)
                {
                    sink(words.On.data(), words.OnCount);
                }

                off.Words = words.Off;
                off.Count = words.OffCount;
            };

        if (m_tapOff.Count > 0 && nowMilliseconds >= m_tapOffAt)
        {
            Send(sink, m_tapOff);
        }

        if (down && !m_down)
        {
            m_down = true;
            m_downSince = nowMilliseconds;
            m_heldSent = false;
            m_tapWaiting = false;

            if (settings.Toggles)
            {
                if (m_toggledOn)
                {
                    Send(sink, m_pressedOff);
                    m_toggledOn = false;
                }
                else
                {
                    startMessage(settings.Pressed, m_pressedOff);
                    m_toggledOn = true;
                }
            }
            else if (settings.TapOrHold && settings.SendsWhenHeld())
            {
                m_tapWaiting = true;
            }
            else
            {
                // A toggle left on before toggling was turned off ends first.
                Send(sink, m_pressedOff);
                m_toggledOn = false;

                startMessage(settings.Pressed, m_pressedOff);
            }

            return;
        }

        if (!down && m_down)
        {
            m_down = false;

            if (m_toggledOn)
            {
                return;
            }

            if (m_tapWaiting)
            {
                m_tapWaiting = false;

                Send(sink, m_tapOff);
                startMessage(settings.Pressed, m_tapOff);
                m_tapOffAt = nowMilliseconds + TapMessageMilliseconds;
            }

            Send(sink, m_pressedOff);
            Send(sink, m_heldOff);
            m_heldSent = false;
            return;
        }

        if (!down || !settings.SendsWhenHeld())
        {
            return;
        }

        if (!m_heldSent)
        {
            auto const holdFor = static_cast<uint64_t>(std::clamp(settings.HoldMilliseconds, MinimumHoldMilliseconds, MaximumHoldMilliseconds));

            if (nowMilliseconds - m_downSince >= holdFor)
            {
                m_heldSent = true;
                m_tapWaiting = false;

                startMessage(settings.Held, m_heldOff);

                m_nextRepeat = nowMilliseconds + static_cast<uint64_t>(
                    std::clamp(settings.RepeatMilliseconds, MinimumRepeatMilliseconds, MaximumRepeatMilliseconds));
            }

            return;
        }

        if (settings.Repeats && nowMilliseconds >= m_nextRepeat)
        {
            auto const every = static_cast<uint64_t>(std::clamp(settings.RepeatMilliseconds, MinimumRepeatMilliseconds, MaximumRepeatMilliseconds));

            // A repeat is a press again: a note starts over, a value is let go and sent again.
            Send(sink, m_heldOff);
            startMessage(settings.Held, m_heldOff);

            m_nextRepeat += every;

            if (m_nextRepeat <= nowMilliseconds)
            {
                m_nextRepeat = nowMilliseconds + every;
            }
        }
    }

    _Use_decl_annotations_
    void ControllerButtonMachine::Release(ControllerSink const& sink)
    {
        Send(sink, m_tapOff);
        Send(sink, m_pressedOff);
        Send(sink, m_heldOff);

        m_down = false;
        m_heldSent = false;
        m_tapWaiting = false;
        m_toggledOn = false;
    }

    _Use_decl_annotations_
    ControlRest AxisRest(ControllerAxisSettings const& settings) noexcept
    {
        if (settings.ReadsDevice())
        {
            return HidControlRest(settings.DeviceControl);
        }

        return IsControllerStick(settings.Control) ? ControlRest::Middle : ControlRest::Bottom;
    }

    _Use_decl_annotations_
    double ShapedAxisValue(ControllerAxisSettings const& settings, double value) noexcept
    {
        auto const rest = AxisRest(settings);
        auto const clamped = std::isfinite(value) ? std::clamp(value, 0.0, 1.0) : (rest == ControlRest::Middle ? 0.5 : 0.0);
        auto const dead = rest == ControlRest::None
            ? 0.0
            : std::clamp(settings.DeadZoneHundredths, 0, MaximumDeadZoneHundredths) / static_cast<double>(FullScaleHundredths);

        double travel{ 0.0 };

        if (rest == ControlRest::Middle)
        {
            auto const offset = clamped - 0.5;
            auto const reach = (std::min)(std::abs(offset) * 2.0, 1.0);
            auto const kept = (std::max)(0.0, (reach - dead) / (1.0 - dead));

            travel = 0.5 + std::copysign(kept * 0.5, offset);
        }
        else
        {
            travel = (std::max)(0.0, (clamped - dead) / (1.0 - dead));
        }

        return settings.Shape.ShapeUnit(std::clamp(travel, 0.0, 1.0));
    }

    _Use_decl_annotations_
    int64_t AxisStepCount(midiapp::ValueMessageTarget const& target) noexcept
    {
        auto const sevenBit = target.Midi1Protocol &&
            (target.Kind == midiapp::ValueMessageKind::ControlChange ||
             target.Kind == midiapp::ValueMessageKind::ChannelPressure ||
             target.Kind == midiapp::ValueMessageKind::PolyPressure);

        return sevenBit ? 128 : 1024;
    }

    _Use_decl_annotations_
    void ControllerAxisMachine::SendValue(ControllerAxisSettings const& settings, double shaped, int64_t step, ControllerSink const& sink)
    {
        uint32_t words[2]{};
        auto const count = midiapp::BuildValueMessage(settings.Target, shaped, words);

        if (count > 0 && sink)
        {
            sink(words, static_cast<uint8_t>(count));
        }

        m_lastStep = step;
        m_lastSentValue = shaped;
        m_sentAny = true;
    }

    _Use_decl_annotations_
    void ControllerAxisMachine::Update(
        ControllerAxisSettings const& settings,
        uint64_t nowMilliseconds,
        bool connected,
        double value,
        ControllerSink const& sink)
    {
        auto const restKind = AxisRest(settings);
        auto const hasRest = restKind != ControlRest::None;
        auto const rest = ShapedAxisValue(settings, restKind == ControlRest::Middle ? 0.5 : 0.0);

        // A controller that goes away, or an axis it doesn't have, is a control let go of.
        if (!connected || !std::isfinite(value))
        {
            if (hasRest && settings.ReturnsToRest && m_sentAny && m_lastSentValue != rest)
            {
                SendValue(settings, rest, static_cast<int64_t>(std::llround(rest * static_cast<double>(AxisStepCount(settings.Target) - 1))), sink);
                m_lastSentAt = nowMilliseconds;
            }

            return;
        }

        auto const shaped = ShapedAxisValue(settings, value);
        auto const steps = AxisStepCount(settings.Target);
        auto const step = static_cast<int64_t>(std::llround(shaped * static_cast<double>(steps - 1)));

        // Where the control is when the patch starts is not a change, so nothing goes out yet.
        if (!m_started)
        {
            m_started = true;
            m_lastStep = step;
            m_lastSentValue = shaped;
            return;
        }

        // The two ends and the rest are sent exactly, even when the last value was a hair away.
        auto const exact = (shaped == 0.0 || shaped == 1.0 || (hasRest && shaped == rest)) && shaped != m_lastSentValue;

        if (step == m_lastStep && !exact)
        {
            return;
        }

        auto const interval = static_cast<uint64_t>(std::clamp(settings.IntervalMilliseconds, 0, MaximumAxisIntervalMilliseconds));

        // Too soon: the next reading tries again, so the last move is never lost.
        if (m_sentAny && interval > 0 && nowMilliseconds - m_lastSentAt < interval)
        {
            return;
        }

        SendValue(settings, shaped, step, sink);
        m_lastSentAt = nowMilliseconds;
    }

    _Use_decl_annotations_
    void ControllerAxisMachine::Release(ControllerAxisSettings const& settings, ControllerSink const& sink)
    {
        if (settings.ReturnsToRest && m_sentAny && AxisRest(settings) != ControlRest::None)
        {
            auto const rest = ShapedAxisValue(settings, AxisRest(settings) == ControlRest::Middle ? 0.5 : 0.0);

            if (rest != m_lastSentValue)
            {
                SendValue(settings, rest, static_cast<int64_t>(std::llround(rest * static_cast<double>(AxisStepCount(settings.Target) - 1))), sink);
            }
        }

        m_started = false;
        m_sentAny = false;
        m_lastSentAt = 0;
    }

    _Use_decl_annotations_
    void ControllerButtonToJson(ControllerButtonSettings const& settings, json::JsonObject const& object)
    {
        WriteSource(object, settings.Player, settings.Control, settings.Device, settings.DeviceControl);

        json::JsonObject pressed{};
        MessageToJson(settings.Pressed, pressed);
        object.SetNamedValue(KeyPressed, pressed);

        SetBool(object, KeyToggles, settings.Toggles);

        if (settings.Held.Kind != ControllerMessageKind::Nothing)
        {
            json::JsonObject held{};
            MessageToJson(settings.Held, held);
            object.SetNamedValue(KeyHeld, held);

            SetNumber(object, KeyHoldMilliseconds, settings.HoldMilliseconds);
            SetBool(object, KeyRepeats, settings.Repeats);

            if (settings.Repeats)
            {
                SetNumber(object, KeyRepeatMilliseconds, settings.RepeatMilliseconds);
            }

            SetBool(object, KeyTapOrHold, settings.TapOrHold);
        }

        SetString(object, KeyScale, settings.Scale == ValueScale::SevenBit ? ScaleSevenBit : ScalePercent);
    }

    _Use_decl_annotations_
    ControllerButtonSettings ControllerButtonFromJson(json::JsonObject const& object) noexcept
    {
        ControllerButtonSettings settings{};

        try
        {
            settings.Player = ReadPlayer(object);
            settings.Control = ReadControl(object, true, ControllerControl::A);
            settings.Device = DeviceFromJson(ReadObject(object, KeyDevice));
            settings.DeviceControl = ReadDeviceControl(object, true, HidButtonControl(1));
            settings.Pressed = MessageFromJson(ReadObject(object, KeyPressed), ControllerMessageKind::Note);
            settings.Toggles = ReadBool(object, KeyToggles, false);
            settings.Held = MessageFromJson(ReadObject(object, KeyHeld), ControllerMessageKind::Nothing);
            settings.HoldMilliseconds = ReadWholeBetween(object, KeyHoldMilliseconds,
                MinimumHoldMilliseconds, MaximumHoldMilliseconds, DefaultHoldMilliseconds);
            settings.Repeats = ReadBool(object, KeyRepeats, false);
            settings.RepeatMilliseconds = ReadWholeBetween(object, KeyRepeatMilliseconds,
                MinimumRepeatMilliseconds, MaximumRepeatMilliseconds, DefaultRepeatMilliseconds);
            settings.TapOrHold = ReadBool(object, KeyTapOrHold, false);
            settings.Scale = ReadScale(object);
        }
        catch (...)
        {
            settings = ControllerButtonSettings{};
        }

        return settings;
    }

    _Use_decl_annotations_
    void ControllerAxisToJson(ControllerAxisSettings const& settings, json::JsonObject const& object)
    {
        WriteSource(object, settings.Player, settings.Control, settings.Device, settings.DeviceControl);
        SetString(object, KeyMessage, midiapp::ValueMessageKindKey(settings.Target.Kind));
        SetNumber(object, KeyGroup, settings.Target.Group);
        SetNumber(object, KeyChannel, settings.Target.Channel);

        if (midiapp::ValueMessageNumberMaximum(settings.Target.Kind) != 0)
        {
            SetNumber(object, KeyNumber, settings.Target.Number);
        }

        if (settings.Target.Kind != midiapp::ValueMessageKind::RegisteredController &&
            settings.Target.Kind != midiapp::ValueMessageKind::AssignableController)
        {
            SetBool(object, KeyMidi1, settings.Target.Midi1Protocol);
        }

        SetPercent(object, KeyDeadZonePercent, settings.DeadZoneHundredths);

        if (!settings.Shape.ChangesNothing())
        {
            object.SetNamedValue(KeyShape, ShapeToJson(settings.Shape, true));
        }

        SetNumber(object, KeyIntervalMilliseconds, settings.IntervalMilliseconds);
        SetBool(object, KeyReturnToRest, settings.ReturnsToRest);
        SetString(object, KeyScale, settings.Scale == ValueScale::SevenBit ? ScaleSevenBit : ScalePercent);
    }

    _Use_decl_annotations_
    ControllerAxisSettings ControllerAxisFromJson(json::JsonObject const& object) noexcept
    {
        ControllerAxisSettings settings{};

        try
        {
            settings.Player = ReadPlayer(object);
            settings.Control = ReadControl(object, false, ControllerControl::LeftStickX);
            settings.Device = DeviceFromJson(ReadObject(object, KeyDevice));
            settings.DeviceControl = ReadDeviceControl(object, false, HidAxisControl(HidAxis::X));

            auto& target = settings.Target;

            target.Kind = midiapp::ValueMessageKindFromKey(ReadString(object, KeyMessage))
                .value_or(midiapp::ValueMessageKind::PitchBend);
            target.Group = static_cast<uint8_t>(ReadWhole(object, KeyGroup, 15, 0));
            target.Channel = static_cast<uint8_t>(ReadWhole(object, KeyChannel, 15, 0));

            auto const maximum = midiapp::ValueMessageNumberMaximum(target.Kind);

            target.Number = maximum == 0 ? 0 : ReadWhole(object, KeyNumber, maximum,
                target.Kind == midiapp::ValueMessageKind::ControlChange ? 1u :
                target.Kind == midiapp::ValueMessageKind::PolyPressure ? 60u : 0u);
            // An RPN or an NRPN always goes as MIDI 2.0.
            target.Midi1Protocol = target.Kind != midiapp::ValueMessageKind::RegisteredController &&
                target.Kind != midiapp::ValueMessageKind::AssignableController &&
                ReadBool(object, KeyMidi1, false);

            auto const deadZone = ReadNumber(object, KeyDeadZonePercent, 0, MaximumDeadZoneHundredths / 100.0, -1);

            settings.DeadZoneHundredths = deadZone < 0 ? DefaultDeadZoneHundredths : static_cast<int32_t>(std::lround(deadZone * 100.0));
            settings.Shape = ShapeFromJson(ReadObject(object, KeyShape), true);
            settings.IntervalMilliseconds = static_cast<int32_t>(ReadWhole(object, KeyIntervalMilliseconds,
                MaximumAxisIntervalMilliseconds, DefaultAxisIntervalMilliseconds));
            settings.ReturnsToRest = ReadBool(object, KeyReturnToRest, true);
            settings.Scale = ReadScale(object);
        }
        catch (...)
        {
            settings = ControllerAxisSettings{};
        }

        return settings;
    }

    _Use_decl_annotations_
    std::wstring ControllerButtonSignature(ControllerButtonSettings const& settings)
    {
        return SourceSignature(settings.Player, settings.Control, settings.Device, settings.DeviceControl) +
            L"|" + MessageSignature(settings.Pressed) + (settings.Toggles ? L"|t" : L"|-") +
            L"|" + MessageSignature(settings.Held) + L'@' + std::to_wstring(settings.HoldMilliseconds) +
            (settings.Repeats ? L"r" : L"-") + std::to_wstring(settings.RepeatMilliseconds) +
            (settings.TapOrHold ? L".th" : L".--") + (settings.Scale == ValueScale::SevenBit ? L".7" : L".%");
    }

    _Use_decl_annotations_
    std::wstring ControllerAxisSignature(ControllerAxisSettings const& settings)
    {
        auto const& target = settings.Target;

        return SourceSignature(settings.Player, settings.Control, settings.Device, settings.DeviceControl) +
            L"|" + std::wstring{ midiapp::ValueMessageKindKey(target.Kind) } + L".g" + std::to_wstring(target.Group) +
            L"c" + std::to_wstring(target.Channel) + L"n" + std::to_wstring(target.Number) +
            (target.Midi1Protocol ? L".m1" : L".m2") + L".d" + std::to_wstring(settings.DeadZoneHundredths) +
            L".s" + ShapeSignature(settings.Shape) + L".i" + std::to_wstring(settings.IntervalMilliseconds) +
            (settings.ReturnsToRest ? L".r" : L".-") + (settings.Scale == ValueScale::SevenBit ? L".7" : L".%");
    }

    _Use_decl_annotations_
    std::wstring ControllerRestartSignature(uint8_t player, ControllerControl control, HidDeviceId const& device, HidControl deviceControl)
    {
        return L"controller." + SourceSignature(player, control, device, deviceControl);
    }
}
