// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Pickers, grid buttons and pad grid readouts on a running surface: a change redraws the pad
// grids it reached, moves every picker on the page to show the grid it targets, and prints what
// the readouts show. Nothing is sent from here.

#include "pch.h"
#include "SurfaceRenderer.h"
#include "PadGrid.h"
#include "PadGridChanges.h"
#include "StringResources.h"

namespace glass
{
    namespace
    {
        // A row offset by name: "Fourth", or "3 scale steps" when the notes are in key.
        std::wstring RowOffsetName(_In_ int32_t offset, _In_ bool inKey)
        {
            static wchar_t const* const semitoneNames[]
            {
                L"PadRowCarryOn", L"PadRowSemitone", L"PadRowWholeTone", L"PadRowMinorThird",
                L"PadRowMajorThird", L"PadRowFourth", L"PadRowTritone", L"PadRowFifth",
                L"PadRowMinorSixth", L"PadRowMajorSixth", L"PadRowMinorSeventh", L"PadRowMajorSeventh",
                L"PadRowOctave",
            };

            if (offset <= 0)
            {
                return std::wstring{ ::midiglass::resources::GetString(L"PadRowCarryOn") };
            }

            if (inKey)
            {
                return offset == 1
                    ? std::wstring{ ::midiglass::resources::GetString(L"PadRowStepsOne") }
                    : std::wstring{ ::midiglass::resources::FormatString(L"PadRowStepsFormat", offset) };
            }

            if (offset < static_cast<int32_t>(std::size(semitoneNames)))
            {
                return std::wstring{ ::midiglass::resources::GetString(semitoneNames[offset]) };
            }

            return std::wstring{ ::midiglass::resources::FormatString(L"PadRowSemitonesFormat", offset) };
        }

        std::wstring KeyName(_In_ PadGridReading const& reading)
        {
            if (!reading.HasKey)
            {
                return std::wstring{ ::midiglass::resources::GetString(L"PadKeyNone") };
            }

            auto const root = RootPickerName(reading.Root);
            auto const scale = std::wstring{ ::midiglass::resources::GetString(ScaleNameResourceKey(reading.Scale)) };

            return std::wstring{ ::midiglass::resources::FormatString(L"ReadoutPadKeyFormat", root, scale) };
        }
    }

    _Use_decl_annotations_
    std::wstring PadPickerPositionName(Control const& picker, size_t position)
    {
        switch (picker.Switch.Picks)
        {
        case SwitchPicks::InKeyOnly:
            return std::wstring{ ::midiglass::resources::GetString(position == 0 ? L"PickerChromatic" : L"PickerInKey") };

        case SwitchPicks::RowOffset:
        {
            static wchar_t const* const names[RowOffsetPickerPositions]
            {
                L"PadRowCarryOn", L"PickerRowThird", L"PadRowFourth", L"PadRowFifth", L"PadRowOctave",
            };

            return position < std::size(names) ? std::wstring{ ::midiglass::resources::GetString(names[position]) } : std::wstring{};
        }

        case SwitchPicks::Octave:
            return SignedNumberText(static_cast<int32_t>(position) - OctavePickerReach);

        default:
            return {};
        }
    }

    _Use_decl_annotations_
    std::wstring PadReadoutText(Control const& readout, LayoutDocument const& document, PadGridStates const& states)
    {
        if (!ReadsAPadGrid(readout))
        {
            return {};
        }

        auto const reading = ReadPadGrid(document, readout.Readout.Targets, states);

        if (!reading.HasGrid)
        {
            return std::wstring{ ::midiglass::resources::GetString(L"ReadoutNoGrids") };
        }

        auto const octave = std::wstring{
            ::midiglass::resources::FormatString(L"ReadoutOctaveFormat", SignedNumberText(reading.Octaves)) };

        auto const rowOffset = RowOffsetName(reading.RowOffset, reading.InKeyOnly);

        switch (readout.Readout.Source)
        {
        case ReadoutSource::PadKey:
            return KeyName(reading);

        case ReadoutSource::Octave:
            return octave;

        case ReadoutSource::RowOffset:
            return rowOffset;

        case ReadoutSource::PadSummary:
        default:
        {
            auto const key = KeyName(reading);

            return std::wstring{ ::midiglass::resources::FormatString(L"ReadoutPadSummaryFormat", key, octave, rowOffset) };
        }
        }
    }

    _Use_decl_annotations_
    PadGridSpec SurfaceRenderer::GridSpecFor(Control const& control) const
    {
        return WithGridState(control.Pads, IsHexPadGrid(control.Kind), StateOf(m_gridStates, control.Id));
    }

    _Use_decl_annotations_
    void SurfaceRenderer::RefreshPadReadouts(LayoutDocument const& document) noexcept
    {
        try
        {
            for (size_t item = 0; item < m_visuals.size() && item < m_readoutTexts.size(); ++item)
            {
                auto const* const control = document.ControlAtIndex(ControlIndexOf(item));

                auto text = control != nullptr ? PadReadoutText(*control, document, m_gridStates) : std::wstring{};

                if (text == m_readoutTexts[item])
                {
                    continue;
                }

                m_readoutTexts[item] = std::move(text);
                RefreshValueText(item);

                // The text drawn is hidden from a screen reader, so the readout's name carries it.
                if (control != nullptr && item < m_elements.size() && m_elements[item] != nullptr)
                {
                    auto const& name = control->Label.empty() ? control->Id : control->Label;

                    m_elements[item].SurfaceName(m_readoutTexts[item].empty()
                        ? winrt::hstring{ name }
                        : ::midiglass::resources::FormatString(L"ReadoutPadAccessibleFormat", name, m_readoutTexts[item]));
                }
            }
        }
        catch (...)
        {
        }
    }

    _Use_decl_annotations_
    std::vector<std::pair<size_t, double>> SurfaceRenderer::ApplyGridStates(
        PadGridStates const& states,
        LayoutDocument const& document,
        Theme const& theme)
    {
        m_gridStates = states;

        std::vector<std::pair<size_t, double>> pickers{};

        for (size_t item = 0; item < m_visuals.size(); ++item)
        {
            auto const* const control = document.ControlAtIndex(ControlIndexOf(item));

            if (control == nullptr)
            {
                continue;
            }

            // The grid's own spec goes in. The changes are laid over it as it is drawn.
            if (IsPadGrid(control->Kind))
            {
                auto const now = GridSpecFor(*control);
                auto const& drawn = PadGridAt(item);

                // Only a grid whose notes moved is drawn again, so one button on a page of grids
                // redraws the grid it changed and leaves the rest alone.
                if (now.KeyRoot != drawn.KeyRoot || now.Scale != drawn.Scale || now.Notes != drawn.Notes ||
                    now.StartNote != drawn.StartNote || now.RowInterval != drawn.RowInterval ||
                    now.RowSteps != drawn.RowSteps || now.ShiftRows != drawn.ShiftRows ||
                    now.ShiftSteps != drawn.ShiftSteps)
                {
                    ResizeItem(item, *control, theme);
                }
            }
            else if (IsKeyPicker(*control))
            {
                auto const value = PickerValueFor(document, *control, m_gridStates);

                SetValue(item, value);
                pickers.emplace_back(item, value);
            }
        }

        RefreshPadReadouts(document);

        return pickers;
    }
}
