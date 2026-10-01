// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Deliberately free of pch.h, XAML and the MIDI SDK.

#include "MackieControl.h"

#include <algorithm>
#include <cstdlib>

namespace glass
{
    namespace
    {
        constexpr uint32_t RecFirstNote = 0;
        constexpr uint32_t SoloFirstNote = 8;
        constexpr uint32_t MuteFirstNote = 16;
        constexpr uint32_t SelectFirstNote = 24;
        constexpr uint32_t VPotPressFirstNote = 32;
        constexpr uint32_t FunctionKeyFirstNote = 54;
        constexpr uint32_t FunctionKeyCount = 8;

        // Eight notes in a row, one per strip, named with the strip number.
        struct StripButtons
        {
            uint32_t FirstNote;
            wchar_t const* FilePrefix;
            wchar_t const* ResourceKey;
        };

        constexpr StripButtons StripButtonSets[]
        {
            { RecFirstNote, L"rec", L"MackieRecFormat" },
            { SoloFirstNote, L"solo", L"MackieSoloFormat" },
            { MuteFirstNote, L"mute", L"MackieMuteFormat" },
            { SelectFirstNote, L"select", L"MackieSelectFormat" },
            { VPotPressFirstNote, L"vpotPress", L"MackieVPotPressFormat" },
        };

        struct NamedButton
        {
            uint32_t Note;
            wchar_t const* FileName;
            wchar_t const* ResourceKey;
        };

        constexpr NamedButton NamedButtons[]
        {
            { 40, L"assignTrack", L"MackieAssignTrack" },
            { 41, L"assignSend", L"MackieAssignSend" },
            { 42, L"assignPan", L"MackieAssignPan" },
            { 43, L"assignPlugIn", L"MackieAssignPlugIn" },
            { 44, L"assignEq", L"MackieAssignEq" },
            { 45, L"assignInstrument", L"MackieAssignInstrument" },
            { 46, L"bankLeft", L"MackieBankLeft" },
            { 47, L"bankRight", L"MackieBankRight" },
            { 48, L"channelLeft", L"MackieChannelLeft" },
            { 49, L"channelRight", L"MackieChannelRight" },
            { 50, L"flip", L"MackieFlip" },
            { 51, L"globalView", L"MackieGlobalView" },
            { 52, L"nameValue", L"MackieNameValue" },
            { 53, L"smpteBeats", L"MackieSmpteBeats" },
            { 62, L"midiTracks", L"MackieMidiTracks" },
            { 63, L"inputs", L"MackieInputs" },
            { 64, L"audioTracks", L"MackieAudioTracks" },
            { 65, L"audioInstruments", L"MackieAudioInstruments" },
            { 66, L"aux", L"MackieAux" },
            { 67, L"buses", L"MackieBuses" },
            { 68, L"outputs", L"MackieOutputs" },
            { 69, L"user", L"MackieUser" },
            { 70, L"shift", L"MackieShift" },
            { 71, L"option", L"MackieOption" },
            { 72, L"control", L"MackieControlKey" },
            { 73, L"alt", L"MackieAlt" },
            { 74, L"readOff", L"MackieReadOff" },
            { 75, L"write", L"MackieWrite" },
            { 76, L"trim", L"MackieTrim" },
            { 77, L"touch", L"MackieTouch" },
            { 78, L"latch", L"MackieLatch" },
            { 79, L"group", L"MackieGroup" },
            { 80, L"save", L"MackieSave" },
            { 81, L"undo", L"MackieUndo" },
            { 82, L"cancel", L"MackieCancel" },
            { 83, L"enter", L"MackieEnter" },
            { 84, L"marker", L"MackieMarker" },
            { 85, L"nudge", L"MackieNudge" },
            { 86, L"cycle", L"MackieCycle" },
            { 87, L"drop", L"MackieDrop" },
            { 88, L"replace", L"MackieReplace" },
            { 89, L"click", L"MackieClick" },
            { 90, L"globalSolo", L"MackieGlobalSolo" },
            { 91, L"rewind", L"MackieRewind" },
            { 92, L"fastForward", L"MackieFastForward" },
            { 93, L"stop", L"MackieStop" },
            { 94, L"play", L"MackiePlay" },
            { 95, L"record", L"MackieRecord" },
            { 96, L"up", L"MackieUp" },
            { 97, L"down", L"MackieDown" },
            { 98, L"left", L"MackieLeft" },
            { 99, L"right", L"MackieRight" },
            { 100, L"zoom", L"MackieZoom" },
            { 101, L"scrub", L"MackieScrub" },
            { 102, L"userSwitch1", L"MackieUserSwitch1" },
            { 103, L"userSwitch2", L"MackieUserSwitch2" },
        };

        NamedButton const* FindNamedButton(_In_ uint32_t note) noexcept
        {
            for (auto const& button : NamedButtons)
            {
                if (button.Note == note)
                {
                    return &button;
                }
            }

            return nullptr;
        }

        StripButtons const* FindStripButtons(_In_ uint32_t note) noexcept
        {
            for (auto const& set : StripButtonSets)
            {
                if (note >= set.FirstNote && note < set.FirstNote + MackieStripCount)
                {
                    return &set;
                }
            }

            return nullptr;
        }

        bool IsFunctionKey(_In_ uint32_t note) noexcept
        {
            return note >= FunctionKeyFirstNote && note < FunctionKeyFirstNote + FunctionKeyCount;
        }

        // The order a person looks for them in: the transport first, the strips next, and the
        // buttons most surfaces leave out last.
        constexpr uint32_t TransportNotes[]{ 94, 93, 95, 91, 92, 86, 89, 84, 85, 87, 88, 90, 101 };
        constexpr uint32_t NavigationNotes[]{ 46, 47, 48, 49, 50, 96, 97, 98, 99, 100 };
        constexpr uint32_t ViewNotes[]{ 51, 62, 63, 64, 65, 66, 67, 68, 69 };
        constexpr uint32_t AssignNotes[]{ 40, 41, 42, 43, 44, 45 };
        constexpr uint32_t AutomationNotes[]{ 74, 75, 76, 77, 78, 79 };
        constexpr uint32_t UtilityNotes[]{ 70, 71, 72, 73, 80, 81, 82, 83, 52, 53, 102, 103 };

        bool IsButtonKind(_In_ ControlKind kind) noexcept
        {
            return kind == ControlKind::Button || kind == ControlKind::Pad || kind == ControlKind::Toggle;
        }

        bool IsFaderKind(_In_ ControlKind kind) noexcept
        {
            return kind == ControlKind::Fader;
        }

        bool IsEncoderKind(_In_ ControlKind kind) noexcept
        {
            return kind == ControlKind::Knob || kind == ControlKind::Turntable || kind == ControlKind::Wheel;
        }
    }

    _Use_decl_annotations_
    MackieShape ShapeOfMackieFunction(uint32_t function) noexcept
    {
        if (function <= MackieLastButtonNote)
        {
            return MackieShape::Button;
        }

        if (function >= MackieFaderBase && function <= MackieFaderBase + MackieMasterStrip)
        {
            return MackieShape::Fader;
        }

        if ((function >= MackieVPotBase && function < MackieVPotBase + MackieStripCount) ||
            function == MackieJog)
        {
            return MackieShape::Encoder;
        }

        return MackieShape::None;
    }

    _Use_decl_annotations_
    uint32_t MackieStripOf(uint32_t function) noexcept
    {
        if (function >= MackieFaderBase && function <= MackieFaderBase + MackieMasterStrip)
        {
            return function - MackieFaderBase;
        }

        if (function >= MackieVPotBase && function < MackieVPotBase + MackieStripCount)
        {
            return function - MackieVPotBase;
        }

        return 0;
    }

    _Use_decl_annotations_
    std::wstring MackieFunctionFileName(uint32_t function)
    {
        switch (ShapeOfMackieFunction(function))
        {
        case MackieShape::Button:
            if (auto const* const set = FindStripButtons(function))
            {
                return std::wstring{ set->FilePrefix } + std::to_wstring(function - set->FirstNote + 1);
            }

            if (IsFunctionKey(function))
            {
                return L"f" + std::to_wstring(function - FunctionKeyFirstNote + 1);
            }

            if (auto const* const button = FindNamedButton(function))
            {
                return button->FileName;
            }

            return {};

        case MackieShape::Fader:
            return function == MackieFaderBase + MackieMasterStrip
                ? std::wstring{ L"masterFader" }
                : L"fader" + std::to_wstring(MackieStripOf(function) + 1);

        case MackieShape::Encoder:
            return function == MackieJog
                ? std::wstring{ L"jog" }
                : L"vpot" + std::to_wstring(MackieStripOf(function) + 1);

        default:
            return {};
        }
    }

    _Use_decl_annotations_
    uint32_t MackieFunctionFromFileName(std::wstring_view name) noexcept
    {
        if (name.empty())
        {
            return MackieNoFunction;
        }

        for (auto const& button : NamedButtons)
        {
            if (name == button.FileName)
            {
                return button.Note;
            }
        }

        if (name == L"masterFader")
        {
            return MackieFaderBase + MackieMasterStrip;
        }

        if (name == L"jog")
        {
            return MackieJog;
        }

        // Everything left is a prefix and one digit, 1 to 8.
        auto const digit = name.back();

        if (digit < L'1' || digit > L'8')
        {
            return MackieNoFunction;
        }

        auto const index = static_cast<uint32_t>(digit - L'1');
        auto const prefix = name.substr(0, name.size() - 1);

        for (auto const& set : StripButtonSets)
        {
            if (prefix == set.FilePrefix)
            {
                return set.FirstNote + index;
            }
        }

        if (prefix == L"f")
        {
            return FunctionKeyFirstNote + index;
        }

        if (prefix == L"fader")
        {
            return MackieFaderBase + index;
        }

        if (prefix == L"vpot")
        {
            return MackieVPotBase + index;
        }

        return MackieNoFunction;
    }

    _Use_decl_annotations_
    MackieFunctionText DescribeMackieFunction(uint32_t function) noexcept
    {
        switch (ShapeOfMackieFunction(function))
        {
        case MackieShape::Button:
            if (auto const* const set = FindStripButtons(function))
            {
                return { set->ResourceKey, function - set->FirstNote + 1 };
            }

            if (IsFunctionKey(function))
            {
                return { L"MackieFunctionKeyFormat", function - FunctionKeyFirstNote + 1 };
            }

            if (auto const* const button = FindNamedButton(function))
            {
                return { button->ResourceKey, 0 };
            }

            return {};

        case MackieShape::Fader:
            return function == MackieFaderBase + MackieMasterStrip
                ? MackieFunctionText{ L"MackieMasterFader", 0 }
                : MackieFunctionText{ L"MackieFaderFormat", MackieStripOf(function) + 1 };

        case MackieShape::Encoder:
            return function == MackieJog
                ? MackieFunctionText{ L"MackieJog", 0 }
                : MackieFunctionText{ L"MackieVPotFormat", MackieStripOf(function) + 1 };

        default:
            return {};
        }
    }

    _Use_decl_annotations_
    std::vector<MackiePickerEntry> MackiePickerFor(ControlKind kind)
    {
        std::vector<MackiePickerEntry> entries{};

        auto const heading = [&entries](wchar_t const* key) { entries.push_back({ MackieNoFunction, key }); };
        auto const add = [&entries](uint32_t function) { entries.push_back({ function, nullptr }); };

        if (IsButtonKind(kind))
        {
            heading(L"MackieHeadingTransport");
            for (auto const note : TransportNotes) { add(note); }

            heading(L"MackieHeadingStrips");
            for (auto const& set : StripButtonSets)
            {
                for (uint32_t strip = 0; strip < MackieStripCount; ++strip) { add(set.FirstNote + strip); }
            }

            heading(L"MackieHeadingNavigation");
            for (auto const note : NavigationNotes) { add(note); }

            heading(L"MackieHeadingFunctionKeys");
            for (uint32_t key = 0; key < FunctionKeyCount; ++key) { add(FunctionKeyFirstNote + key); }

            heading(L"MackieHeadingViews");
            for (auto const note : ViewNotes) { add(note); }

            heading(L"MackieHeadingAssign");
            for (auto const note : AssignNotes) { add(note); }

            heading(L"MackieHeadingAutomation");
            for (auto const note : AutomationNotes) { add(note); }

            heading(L"MackieHeadingUtility");
            for (auto const note : UtilityNotes) { add(note); }
        }
        else if (IsFaderKind(kind))
        {
            for (uint32_t strip = 0; strip <= MackieMasterStrip; ++strip) { add(MackieFaderBase + strip); }
        }
        else if (IsEncoderKind(kind))
        {
            for (uint32_t strip = 0; strip < MackieStripCount; ++strip) { add(MackieVPotBase + strip); }

            add(MackieJog);
        }

        return entries;
    }

    _Use_decl_annotations_
    bool MackieFunctionFits(uint32_t function, ControlKind kind) noexcept
    {
        switch (ShapeOfMackieFunction(function))
        {
        case MackieShape::Button: return IsButtonKind(kind);
        case MackieShape::Fader: return IsFaderKind(kind);
        case MackieShape::Encoder: return IsEncoderKind(kind);
        default: return false;
        }
    }

    _Use_decl_annotations_
    ControlMessage MakeMackieRow(uint32_t function, std::wstring const& deviceName, int32_t groupIndex)
    {
        ControlMessage row{};

        row.Trigger = MessageTrigger::Changes;
        row.Kind = MessageKind::MackieControl;
        row.Number = function;
        row.DeviceName = deviceName;
        row.GroupIndex = groupIndex;
        row.ChannelIndex = 0;

        return row;
    }

    _Use_decl_annotations_
    uint32_t MackieFunctionOf(ControlMessage const& message) noexcept
    {
        switch (message.Kind)
        {
        case MessageKind::MackieControl:
            return ShapeOfMackieFunction(message.Number) == MackieShape::None ? MackieNoFunction : message.Number;

        case MessageKind::Note:
            if (message.ChannelIndex != 0)
            {
                return MackieNoFunction;
            }

            if (message.Number <= MackieLastButtonNote)
            {
                return message.Number;
            }

            // A fader's touch note stands for the fader it belongs to.
            if (message.Number >= MackieFaderTouchNote && message.Number <= MackieFaderTouchNote + MackieMasterStrip)
            {
                return MackieFaderBase + (message.Number - MackieFaderTouchNote);
            }

            return MackieNoFunction;

        case MessageKind::PitchBend:
            return message.ChannelIndex >= 0 && static_cast<uint32_t>(message.ChannelIndex) <= MackieMasterStrip
                ? MackieFaderBase + static_cast<uint32_t>(message.ChannelIndex)
                : MackieNoFunction;

        case MessageKind::ControlChange:
            if (message.ChannelIndex != 0)
            {
                return MackieNoFunction;
            }

            if (message.Number >= MackieVPotController && message.Number < MackieVPotController + MackieStripCount)
            {
                return MackieVPotBase + (message.Number - MackieVPotController);
            }

            return message.Number == MackieJogController ? MackieJog : MackieNoFunction;

        default:
            return MackieNoFunction;
        }
    }

    _Use_decl_annotations_
    ControlMessage PlainRowFor(ControlMessage const& mackieRow)
    {
        ControlMessage row{};

        row.Trigger = MessageTrigger::Changes;
        row.DeviceName = mackieRow.DeviceName;
        row.GroupIndex = mackieRow.GroupIndex;
        row.ChannelIndex = 0;

        auto const function = mackieRow.Number;

        switch (ShapeOfMackieFunction(function))
        {
        case MackieShape::Button:
            row.Kind = MessageKind::Note;
            row.Number = function;
            break;

        case MackieShape::Fader:
            row.Kind = MessageKind::PitchBend;
            row.ChannelIndex = static_cast<int32_t>(MackieStripOf(function));
            break;

        case MackieShape::Encoder:
            row.Kind = MessageKind::ControlChange;
            row.Number = function == MackieJog
                ? MackieJogController
                : MackieVPotController + MackieStripOf(function);
            break;

        default:
            row.Kind = MessageKind::ControlChange;
            break;
        }

        return row;
    }

    _Use_decl_annotations_
    void WaitForMackieFunction(Control& control, LayoutDocument const& document)
    {
        if (MackiePickerFor(control.Kind).empty())
        {
            return;
        }

        auto& rows = control.Messages;

        auto const first = std::find_if(rows.begin(), rows.end(), [&document](ControlMessage const& row)
            {
                return SendsToADevice(row.Kind) &&
                    document.ProtocolOf(row.DeviceName) == DeviceProtocol::MackieControl;
            });

        if (first == rows.end())
        {
            return;
        }

        auto const deviceName = first->DeviceName;
        auto const groupIndex = first->GroupIndex;

        rows.erase(
            std::remove_if(rows.begin(), rows.end(), [&deviceName](ControlMessage const& row)
                {
                    return SendsToADevice(row.Kind) && row.DeviceName == deviceName;
                }),
            rows.end());

        rows.insert(rows.begin(), MakeMackieRow(MackieNoFunction, deviceName, groupIndex));
    }

    _Use_decl_annotations_
    uint8_t MackieTurnValue(int32_t ticks) noexcept
    {
        auto const distance = static_cast<uint8_t>((std::min)(std::abs(ticks), 63));

        return ticks < 0 ? static_cast<uint8_t>(0x40 | distance) : distance;
    }

    _Use_decl_annotations_
    MackieLight MackieLightFromVelocity(uint32_t velocity) noexcept
    {
        if (velocity == 127)
        {
            return MackieLight::On;
        }

        return (velocity & 1) != 0 ? MackieLight::Blinking : MackieLight::Off;
    }
}
