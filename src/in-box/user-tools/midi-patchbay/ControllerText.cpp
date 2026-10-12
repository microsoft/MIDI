// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// What a customer reads about the game controller steps.

#include "pch.h"
#include "MessageText.h"
#include "StringResources.h"

namespace midipatchbay
{
    namespace
    {
        constexpr wchar_t const* ControlKeys[] =
        {
            L"ControllerControlA", L"ControllerControlB", L"ControllerControlX", L"ControllerControlY",
            L"ControllerControlLeftBumper", L"ControllerControlRightBumper",
            L"ControllerControlView", L"ControllerControlMenu",
            L"ControllerControlLeftStickButton", L"ControllerControlRightStickButton",
            L"ControllerControlDPadUp", L"ControllerControlDPadDown", L"ControllerControlDPadLeft", L"ControllerControlDPadRight",
            L"ControllerControlLeftTrigger", L"ControllerControlRightTrigger",
            L"ControllerControlLeftStickX", L"ControllerControlLeftStickY",
            L"ControllerControlRightStickX", L"ControllerControlRightStickY",
            L"ControllerControlLeftStickUp", L"ControllerControlLeftStickDown",
            L"ControllerControlLeftStickLeft", L"ControllerControlLeftStickRight",
            L"ControllerControlRightStickUp", L"ControllerControlRightStickDown",
            L"ControllerControlRightStickLeft", L"ControllerControlRightStickRight",
        };

        static_assert(std::size(ControlKeys) == ControllerControlCount);

        constexpr wchar_t const* MessageKindKeys[] =
        {
            L"ControllerMessageNothing", L"ControllerMessageNote", L"ControllerMessageControlChange",
            L"ControllerMessageProgramChange", L"ControllerMessagePitchBend", L"ControllerMessageChannelPressure",
            L"ControllerMessagePolyPressure", L"ControllerMessageRpn", L"ControllerMessageNrpn",
            L"ControllerMessageStart", L"ControllerMessageContinue", L"ControllerMessageStop",
            L"ControllerMessageWords",
        };

        static_assert(std::size(MessageKindKeys) == ControllerMessageKindCount);

        winrt::hstring DescribeTarget(_In_ midiapp::ValueMessageTarget const& target)
        {
            auto const bank = static_cast<int>((target.Number >> 7) & 0x7F);
            auto const index = static_cast<int>(target.Number & 0x7F);

            switch (target.Kind)
            {
            case midiapp::ValueMessageKind::PitchBend:
                return resources::GetString(L"LfoTargetPitchBend");

            case midiapp::ValueMessageKind::ChannelPressure:
                return resources::GetString(L"LfoTargetChannelPressure");

            case midiapp::ValueMessageKind::PolyPressure:
                return resources::FormatString(L"LfoTargetPolyPressureFormat", DescribeNote(static_cast<uint8_t>(index)));

            case midiapp::ValueMessageKind::RegisteredController:
                return resources::FormatString(L"LfoTargetRegisteredFormat", bank, index);

            case midiapp::ValueMessageKind::AssignableController:
                return resources::FormatString(L"LfoTargetAssignableFormat", bank, index);

            default:
                return resources::FormatString(L"LfoTargetControlChangeFormat", index);
            }
        }
    }

    _Use_decl_annotations_
    winrt::hstring DescribeControllerControl(ControllerControl control) noexcept
    {
        auto const index = static_cast<size_t>(control);

        return index < std::size(ControlKeys) ? resources::GetString(ControlKeys[index]) : winrt::hstring{};
    }

    _Use_decl_annotations_
    winrt::hstring DescribeControllerPlayer(uint8_t player) noexcept
    {
        return resources::FormatString(L"ControllerPlayerFormat", static_cast<int>(player) + 1);
    }

    _Use_decl_annotations_
    winrt::hstring DescribeControllerMessageKind(ControllerMessageKind kind) noexcept
    {
        auto const index = static_cast<size_t>(kind);

        return index < std::size(MessageKindKeys) ? resources::GetString(MessageKindKeys[index]) : winrt::hstring{};
    }

    _Use_decl_annotations_
    winrt::hstring DescribeControllerMessage(ControllerMessage const& message) noexcept
    {
        try
        {
            switch (message.Kind)
            {
            case ControllerMessageKind::Note:
                return resources::FormatString(L"ControllerSummaryNoteFormat",
                    DescribeNote(static_cast<uint8_t>((std::min)(message.Number, 127u))));

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
                target.Number = message.Number;

                return DescribeTarget(target);
            }

            case ControllerMessageKind::ProgramChange:
                return message.Step == ProgramStep::Next ? resources::GetString(L"ControllerSummaryNextProgram")
                    : message.Step == ProgramStep::Previous ? resources::GetString(L"ControllerSummaryPreviousProgram")
                    : resources::FormatString(L"ControllerSummaryProgramFormat", static_cast<int>((std::min)(message.Number, 127u)));

            case ControllerMessageKind::Start:
                return resources::GetString(L"ControllerSummaryStart");

            case ControllerMessageKind::Continue:
                return resources::GetString(L"ControllerSummaryContinue");

            case ControllerMessageKind::Stop:
                return resources::GetString(L"ControllerSummaryStop");

            case ControllerMessageKind::Words:
                return resources::GetString(L"ControllerSummaryWords");

            default:
                return resources::GetString(L"ControllerSummaryNothing");
            }
        }
        catch (...)
        {
        }

        return {};
    }

    _Use_decl_annotations_
    winrt::hstring DescribeControllerStep(BlockKind kind, BlockSettings const& settings) noexcept
    {
        try
        {
            if (kind == BlockKind::ControllerAxis)
            {
                auto const& axis = settings.ControllerAxis;

                return resources::FormatString(L"BlockDescControllerAxisFormat",
                    static_cast<int>(axis.Player) + 1,
                    DescribeControllerControl(axis.Control),
                    DescribeTarget(axis.Target),
                    static_cast<int>(axis.Target.Channel) + 1);
            }

            auto const& button = settings.ControllerButton;

            auto text = resources::FormatString(button.Toggles ? L"BlockDescControllerToggleFormat" : L"BlockDescControllerButtonFormat",
                static_cast<int>(button.Player) + 1,
                DescribeControllerControl(button.Control),
                DescribeControllerMessage(button.Pressed));

            if (button.SendsWhenHeld())
            {
                text = resources::FormatString(button.TapOrHold ? L"BlockDescControllerTapOrHoldFormat" : L"BlockDescControllerHeldFormat",
                    text, DescribeControllerMessage(button.Held));
            }

            return text;
        }
        catch (...)
        {
        }

        return {};
    }
}
