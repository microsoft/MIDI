// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// What a customer reads about the message converter.

#include "pch.h"
#include "MessageText.h"
#include "StringResources.h"

namespace midipatchbay
{
    namespace
    {
        ControllerMessageKind KindOf(_In_ ConverterInput input) noexcept
        {
            switch (input)
            {
            case ConverterInput::ControlChange:     return ControllerMessageKind::ControlChange;
            case ConverterInput::ProgramChange:     return ControllerMessageKind::ProgramChange;
            case ConverterInput::PitchBend:         return ControllerMessageKind::PitchBend;
            case ConverterInput::ChannelPressure:   return ControllerMessageKind::ChannelPressure;
            case ConverterInput::PolyPressure:      return ControllerMessageKind::PolyPressure;
            case ConverterInput::Clock:             return ControllerMessageKind::Clock;
            case ConverterInput::Start:             return ControllerMessageKind::Start;
            case ConverterInput::Continue:          return ControllerMessageKind::Continue;
            case ConverterInput::Stop:              return ControllerMessageKind::Stop;
            default:                                return ControllerMessageKind::Note;
            }
        }

        // "Note C3", "Notes C1 to G1", "Any CC", and the channel when the row looks at only one.
        winrt::hstring DescribeFrom(_In_ ConverterRow const& row)
        {
            // Clock, start, continue and stop have nothing more to say.
            if (!ConverterInputHasChannel(row.From))
            {
                return DescribeControllerMessageKind(KindOf(row.From));
            }

            auto const lowest = (std::min)(row.Lowest, row.Highest);
            auto const highest = (std::max)(row.Lowest, row.Highest);
            auto const all = lowest == 0 && highest == 127;
            auto const one = lowest == highest;

            winrt::hstring text{};

            switch (row.From)
            {
            case ConverterInput::Note:
                text = all ? resources::GetString(L"ConverterFromAnyNote")
                    : one ? resources::FormatString(L"ConverterFromNoteFormat", DescribeNote(lowest))
                    : resources::FormatString(L"ConverterFromNotesFormat", DescribeNote(lowest), DescribeNote(highest));
                break;

            case ConverterInput::ControlChange:
                text = all ? resources::GetString(L"ConverterFromAnyController")
                    : one ? resources::FormatString(L"ConverterFromControllerFormat", static_cast<int>(lowest))
                    : resources::FormatString(L"ConverterFromControllersFormat", static_cast<int>(lowest), static_cast<int>(highest));
                break;

            case ConverterInput::ProgramChange:
                text = all ? resources::GetString(L"ConverterFromAnyProgram")
                    : one ? resources::FormatString(L"ConverterFromProgramFormat", static_cast<int>(lowest))
                    : resources::FormatString(L"ConverterFromProgramsFormat", static_cast<int>(lowest), static_cast<int>(highest));
                break;

            case ConverterInput::PolyPressure:
                text = all ? resources::GetString(L"ConverterFromAnyPolyPressure")
                    : one ? resources::FormatString(L"ConverterFromPolyPressureFormat", DescribeNote(lowest))
                    : resources::FormatString(L"ConverterFromPolyPressuresFormat", DescribeNote(lowest), DescribeNote(highest));
                break;

            case ConverterInput::PitchBend:
                text = resources::GetString(L"ConverterFromPitchBend");
                break;

            default:
                text = resources::GetString(L"ConverterFromChannelPressure");
                break;
            }

            if (row.FromChannel >= 0)
            {
                text = resources::FormatString(L"ConverterOnChannelFormat", text, static_cast<int>(row.FromChannel) + 1);
            }

            return text;
        }

        // "CC 27 on channel 4", or "the CC with the same number" when it keeps the message's number.
        winrt::hstring DescribeTo(_In_ ConverterRow const& row)
        {
            auto const& to = row.To;
            auto const keepsNumber = row.SameNumber && ConverterInputHasNumber(row.From) &&
                ControllerMessageNumberMaximum(to.Kind) != 0 &&
                (to.Kind != ControllerMessageKind::ProgramChange || to.Step == ProgramStep::This);

            winrt::hstring text{};

            if (keepsNumber)
            {
                switch (to.Kind)
                {
                case ControllerMessageKind::Note:                   text = resources::GetString(L"ConverterToSameNote"); break;
                case ControllerMessageKind::ControlChange:          text = resources::GetString(L"ConverterToSameController"); break;
                case ControllerMessageKind::PolyPressure:           text = resources::GetString(L"ConverterToSamePolyPressure"); break;
                case ControllerMessageKind::ProgramChange:          text = resources::GetString(L"ConverterToSameProgram"); break;
                case ControllerMessageKind::RegisteredController:   text = resources::GetString(L"ConverterToSameRpn"); break;
                default:                                            text = resources::GetString(L"ConverterToSameNrpn"); break;
                }
            }
            else
            {
                text = DescribeControllerMessage(to);
            }

            if (IsChannelMessageKind(to.Kind) && (!row.SameChannel || !ConverterInputHasChannel(row.From)))
            {
                text = resources::FormatString(L"ConverterOnChannelFormat", text, static_cast<int>(to.Channel & 0x0F) + 1);
            }

            return text;
        }
    }

    _Use_decl_annotations_
    winrt::hstring DescribeConverterInput(ConverterInput input) noexcept
    {
        return DescribeControllerMessageKind(KindOf(input));
    }

    _Use_decl_annotations_
    winrt::hstring DescribeConverterRow(ConverterRow const& row) noexcept
    {
        try
        {
            return resources::FormatString(L"ConverterRowFormat", DescribeFrom(row), DescribeTo(row));
        }
        catch (...)
        {
        }

        return {};
    }

    _Use_decl_annotations_
    winrt::hstring DescribeConverterStep(MessageConverterSettings const& settings) noexcept
    {
        try
        {
            if (settings.Rows.size() == 1)
            {
                return DescribeConverterRow(settings.Rows.front());
            }

            if (!settings.Rows.empty())
            {
                return resources::FormatString(L"BlockDescConverterRowsFormat", static_cast<int>(settings.Rows.size()));
            }
        }
        catch (...)
        {
        }

        return {};
    }
}
