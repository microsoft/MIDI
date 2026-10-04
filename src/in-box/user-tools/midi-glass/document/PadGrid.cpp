// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Deliberately free of pch.h and XAML, like the rest of the document layer.

#include "PadGrid.h"

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <limits>

namespace glass
{
    namespace
    {
        constexpr double Sqrt3 = 1.7320508075688772;

        // The gap between two pads, as a fraction of a pad, and the most and least it may be.
        // The same gap runs round the outside of the grid, so the pads sit on the control's
        // plate the way they sit on a hardware controller's body.
        constexpr double GapFraction = 0.10;
        constexpr double MinimumGap = 1.5;
        constexpr double MaximumGap = 12.0;

        // Below this a pad is a speck. A control that small still draws, but no smaller.
        constexpr double SmallestDrawnPad = 4.0;

        // A size is compared with a little slack, so a control laid out to hold exactly eight
        // pads is not refused its eighth over the last bit of a double.
        constexpr double FitSlack = 1e-6;

        double GapFor(_In_ double pad) noexcept
        {
            return std::clamp(pad * GapFraction, MinimumGap, MaximumGap);
        }

        // A hexagon standing on a point is taller than it is wide: its width is across the
        // flats and its height is from point to point.
        double HexHeightFor(_In_ double pad) noexcept
        {
            return pad * 2.0 / Sqrt3;
        }

        int32_t RowsFor(_In_ int32_t count, _In_ int32_t columns) noexcept
        {
            return columns <= 0 ? 0 : (count + columns - 1) / columns;
        }

        // The room this many columns and rows need at this size, with the gap round the outside.
        double WidthNeeded(_In_ bool hex, _In_ int32_t columns, _In_ int32_t rows, _In_ double pad) noexcept
        {
            auto const gap = GapFor(pad);
            auto width = columns * pad + (columns + 1) * gap;

            // Every second row of hexagons sits half a pad to the right.
            if (hex && rows > 1)
            {
                width += (pad + gap) * 0.5;
            }

            return width;
        }

        double HeightNeeded(_In_ bool hex, _In_ int32_t rows, _In_ double pad) noexcept
        {
            auto const gap = GapFor(pad);

            if (!hex)
            {
                return rows * pad + (rows + 1) * gap;
            }

            // Hexagons nest into the row below, so rows are closer than a pad's height apart.
            auto const rowPitch = (pad + gap) * Sqrt3 * 0.5;

            return (rows - 1) * rowPitch + HexHeightFor(pad) + 2.0 * gap;
        }

        bool Fits(
            _In_ bool hex,
            _In_ int32_t columns,
            _In_ int32_t rows,
            _In_ double pad,
            _In_ double width,
            _In_ double height) noexcept
        {
            return WidthNeeded(hex, columns, rows, pad) <= width + FitSlack &&
                HeightNeeded(hex, rows, pad) <= height + FitSlack;
        }

        // The largest pad no bigger than `largest` that fits, or zero. Both sizes grow with the
        // pad, so halving the difference finds it to far better than a pixel.
        double LargestFit(
            _In_ bool hex,
            _In_ int32_t columns,
            _In_ int32_t rows,
            _In_ double largest,
            _In_ double width,
            _In_ double height) noexcept
        {
            if (columns <= 0 || rows <= 0)
            {
                return 0.0;
            }

            if (Fits(hex, columns, rows, largest, width, height))
            {
                return largest;
            }

            if (!Fits(hex, columns, rows, SmallestDrawnPad, width, height))
            {
                return 0.0;
            }

            auto low = SmallestDrawnPad;
            auto high = largest;

            for (int32_t step = 0; step < 40; ++step)
            {
                auto const middle = (low + high) * 0.5;

                if (Fits(hex, columns, rows, middle, width, height))
                {
                    low = middle;
                }
                else
                {
                    high = middle;
                }
            }

            return low;
        }

        int32_t RightIntervalOf(_In_ PadGridSpec const& spec) noexcept
        {
            return std::clamp(spec.RightInterval, MinimumRightInterval, MaximumPadInterval);
        }

        int32_t RowIntervalOf(_In_ PadGridSpec const& spec) noexcept
        {
            return std::clamp(spec.RowInterval, MinimumRowInterval, MaximumPadInterval);
        }

        // The semitones of each scale, counted up from its root, as a mask of twelve bits.
        uint16_t ScaleMask(_In_ MusicalScale scale) noexcept
        {
            auto const mask = [](std::initializer_list<int32_t> steps)
                {
                    uint16_t bits{ 0 };

                    for (auto const step : steps)
                    {
                        bits = static_cast<uint16_t>(bits | (1u << step));
                    }

                    return bits;
                };

            switch (scale)
            {
            case MusicalScale::Minor:           return mask({ 0, 2, 3, 5, 7, 8, 10 });
            case MusicalScale::HarmonicMinor:   return mask({ 0, 2, 3, 5, 7, 8, 11 });
            case MusicalScale::MelodicMinor:    return mask({ 0, 2, 3, 5, 7, 9, 11 });
            case MusicalScale::Dorian:          return mask({ 0, 2, 3, 5, 7, 9, 10 });
            case MusicalScale::Phrygian:        return mask({ 0, 1, 3, 5, 7, 8, 10 });
            case MusicalScale::Lydian:          return mask({ 0, 2, 4, 6, 7, 9, 11 });
            case MusicalScale::Mixolydian:      return mask({ 0, 2, 4, 5, 7, 9, 10 });
            case MusicalScale::Locrian:         return mask({ 0, 1, 3, 5, 6, 8, 10 });
            case MusicalScale::MajorPentatonic: return mask({ 0, 2, 4, 7, 9 });
            case MusicalScale::MinorPentatonic: return mask({ 0, 3, 5, 7, 10 });
            case MusicalScale::Blues:           return mask({ 0, 3, 5, 6, 7, 10 });
            case MusicalScale::WholeTone:       return mask({ 0, 2, 4, 6, 8, 10 });
            case MusicalScale::Major:
            default:                            return mask({ 0, 2, 4, 5, 7, 9, 11 });
            }
        }

        int32_t PitchClass(_In_ int32_t note) noexcept
        {
            return ((note % 12) + 12) % 12;
        }
    }

    _Use_decl_annotations_
    bool IsPadGrid(ControlKind kind) noexcept
    {
        return kind == ControlKind::NotePads || kind == ControlKind::HexPads;
    }

    _Use_decl_annotations_
    bool IsHexPadGrid(ControlKind kind) noexcept
    {
        return kind == ControlKind::HexPads;
    }

    _Use_decl_annotations_
    double PadSizeToFit(bool hex, int32_t columns, int32_t rows, double width, double height) noexcept
    {
        if (!std::isfinite(width) || !std::isfinite(height))
        {
            return 0.0;
        }

        return LargestFit(hex, columns, rows, MaximumPadSize, width, height);
    }

    _Use_decl_annotations_
    int32_t PadNote(PadGridSpec const& spec, bool hex, int32_t row, int32_t column, int32_t columns) noexcept
    {
        auto const start = std::clamp(spec.StartNote, 0, 127);
        auto const right = RightIntervalOf(spec);
        auto const up = RowIntervalOf(spec);

        int32_t note{ 0 };

        if (hex)
        {
            // Rows that sit half a pad over, turned into the two directions a hexagon has: one
            // step right, and one step up and to the right. Every second row, the pad directly
            // above has moved one step to the left along that diagonal.
            auto const along = column - row / 2;

            note = start + along * right + row * up;
        }
        else
        {
            // Zero carries each row on from the end of the one below, so the grid is the notes
            // in order and nothing appears twice.
            auto const rowStep = up > 0 ? up : std::max(columns, 1) * right;

            note = start + column * right + row * rowStep;
        }

        return note >= 0 && note <= 127 ? note : -1;
    }

    _Use_decl_annotations_
    PadGridLayout LayOutPadGrid(PadGridSpec const& spec, bool hex, double width, double height)
    {
        PadGridLayout layout{};

        layout.Hex = hex;

        if (!std::isfinite(width) || !std::isfinite(height) || width <= 0.0 || height <= 0.0)
        {
            return layout;
        }

        auto const count = std::clamp(spec.PadCount, MinimumPadCount, MaximumPadCount);
        auto const asked = std::clamp(spec.PadSize, MinimumPadSize, MaximumPadSize);

        // As many to a row as the width holds at the size asked for. That is the flow: resizing
        // the control moves pads between rows rather than resizing them.
        int32_t columns{ 1 };

        for (auto candidate = count; candidate >= 1; --candidate)
        {
            if (WidthNeeded(hex, candidate, RowsFor(count, candidate), asked) <= width + FitSlack)
            {
                columns = candidate;
                break;
            }
        }

        auto pad = asked;

        if (!Fits(hex, columns, RowsFor(count, columns), asked, width, height))
        {
            // Too small to hold them all at that size. Every pad is kept, and the flow chosen is
            // whichever lets them stay largest. A tie goes to the wider flow.
            auto best = 0.0;
            auto bestColumns = columns;

            for (int32_t candidate = 1; candidate <= count; ++candidate)
            {
                auto const size = LargestFit(hex, candidate, RowsFor(count, candidate), asked, width, height);

                if (size > 0.0 && size >= best)
                {
                    best = size;
                    bestColumns = candidate;
                }
            }

            columns = bestColumns;
            pad = best > 0.0 ? best : SmallestDrawnPad;
        }

        auto const rows = RowsFor(count, columns);
        auto const gap = GapFor(pad);

        layout.PadWidth = pad;
        layout.PadHeight = hex ? HexHeightFor(pad) : pad;
        layout.Gap = gap;
        layout.Pitch = pad + gap;
        layout.RowPitch = hex ? layout.Pitch * Sqrt3 * 0.5 : layout.Pitch;
        layout.Columns = columns;
        layout.Rows = rows;

        // The block of pads sits in the middle of the control, and the lowest row is at the
        // bottom of it, the way every grid controller puts its lowest note nearest the player.
        auto const blockWidth = WidthNeeded(hex, columns, rows, pad) - gap * 2.0;
        auto const blockHeight = HeightNeeded(hex, rows, pad) - gap * 2.0;

        auto const left = (width - blockWidth) * 0.5;
        auto const bottom = (height + blockHeight) * 0.5;

        layout.Cells.reserve(static_cast<size_t>(count));

        for (int32_t index = 0; index < count; ++index)
        {
            PadCell cell{};

            cell.Row = index / columns;
            cell.Column = index % columns;

            auto const shift = hex && (cell.Row % 2) == 1 ? layout.Pitch * 0.5 : 0.0;

            cell.CenterX = left + shift + cell.Column * layout.Pitch + pad * 0.5;
            cell.CenterY = bottom - layout.PadHeight * 0.5 - cell.Row * layout.RowPitch;
            cell.Note = PadNote(spec, hex, cell.Row, cell.Column, columns);

            layout.Cells.push_back(cell);
        }

        return layout;
    }

    _Use_decl_annotations_
    int32_t PadAtPoint(PadGridLayout const& layout, double x, double y) noexcept
    {
        if (layout.Cells.empty() || !std::isfinite(x) || !std::isfinite(y) || layout.Pitch <= 0.0)
        {
            return -1;
        }

        auto nearest = -1;
        auto nearestDistance = std::numeric_limits<double>::max();

        for (size_t index = 0; index < layout.Cells.size(); ++index)
        {
            auto const dx = x - layout.Cells[index].CenterX;
            auto const dy = y - layout.Cells[index].CenterY;
            auto const distance = dx * dx + dy * dy;

            if (distance < nearestDistance)
            {
                nearestDistance = distance;
                nearest = static_cast<int32_t>(index);
            }
        }

        if (nearest < 0)
        {
            return -1;
        }

        // Nearest is only half the question. Out past the last pad in a short row, the nearest
        // pad is a long way off, and a touch there has not landed on it.
        auto const& cell = layout.Cells[static_cast<size_t>(nearest)];
        auto const half = layout.Pitch * 0.5;

        if (layout.Hex)
        {
            // The cell a hexagon owns reaches half the way to each neighbor, which puts its
            // corners this far from the middle.
            return nearestDistance <= (layout.Pitch / Sqrt3) * (layout.Pitch / Sqrt3) ? nearest : -1;
        }

        return std::abs(x - cell.CenterX) <= half && std::abs(y - cell.CenterY) <= half ? nearest : -1;
    }

    _Use_decl_annotations_
    double PitchAtPoint(PadGridLayout const& layout, PadGridSpec const& spec, int32_t cell, double x) noexcept
    {
        if (cell < 0 || static_cast<size_t>(cell) >= layout.Cells.size() || layout.Pitch <= 0.0)
        {
            return 0.0;
        }

        auto const index = static_cast<size_t>(cell);
        auto const& here = layout.Cells[index];

        if (here.Note < 0)
        {
            return 0.0;
        }

        auto offset = std::isfinite(x) ? (x - here.CenterX) / layout.Pitch : 0.0;

        offset = std::clamp(offset, -0.5, 0.5);

        // There has to be a pad on that side playing something, or there is no pitch to bend
        // toward. The end of a row holds its own note.
        auto const hasLeft = here.Column > 0 &&
            index > 0 &&
            layout.Cells[index - 1].Row == here.Row &&
            layout.Cells[index - 1].Note >= 0;

        auto const hasRight = index + 1 < layout.Cells.size() &&
            layout.Cells[index + 1].Row == here.Row &&
            layout.Cells[index + 1].Note >= 0;

        if ((offset < 0.0 && !hasLeft) || (offset > 0.0 && !hasRight))
        {
            offset = 0.0;
        }

        auto const magnitude = std::abs(offset);

        auto const bent = magnitude <= PadInTuneHalfWidth
            ? 0.0
            : (magnitude - PadInTuneHalfWidth) / (0.5 - PadInTuneHalfWidth) * 0.5;

        return here.Note + std::copysign(bent, offset) * RightIntervalOf(spec);
    }

    _Use_decl_annotations_
    bool IsInScale(MusicalScale scale, int32_t root, int32_t note) noexcept
    {
        auto const step = PitchClass(note - root);

        return (ScaleMask(scale) & (1u << step)) != 0;
    }

    _Use_decl_annotations_
    PadRole RoleOfNote(PadGridSpec const& spec, int32_t note) noexcept
    {
        if (note < 0 || note > 127)
        {
            return PadRole::OutOfKey;
        }

        if (spec.KeyRoot < 0 || spec.KeyRoot > 11)
        {
            return PadRole::InKey;
        }

        if (PitchClass(note) == spec.KeyRoot)
        {
            return PadRole::Root;
        }

        return IsInScale(spec.Scale, spec.KeyRoot, note) ? PadRole::InKey : PadRole::OutOfKey;
    }

    _Use_decl_annotations_
    bool KeyUsesFlats(int32_t root, MusicalScale scale) noexcept
    {
        if (root < 0 || root > 11)
        {
            return false;
        }

        // Every scale here but the whole tone one borrows the key signature of a major key. A
        // minor key borrows its relative major's, a mode the major it is a mode of.
        int32_t toMajor{ 0 };

        switch (scale)
        {
        case MusicalScale::Minor:
        case MusicalScale::HarmonicMinor:
        case MusicalScale::MelodicMinor:
        case MusicalScale::MinorPentatonic:
        case MusicalScale::Blues:
            toMajor = 3;
            break;

        case MusicalScale::Dorian:     toMajor = 10; break;
        case MusicalScale::Phrygian:   toMajor = 8; break;
        case MusicalScale::Lydian:     toMajor = 7; break;
        case MusicalScale::Mixolydian: toMajor = 5; break;
        case MusicalScale::Locrian:    toMajor = 1; break;

        case MusicalScale::WholeTone:
            return false;

        default:
            break;
        }

        // F, B flat, E flat, A flat, D flat and G flat major are the keys written with flats.
        switch (PitchClass(root + toMajor))
        {
        case 1:
        case 3:
        case 5:
        case 6:
        case 8:
        case 10:
            return true;

        default:
            return false;
        }
    }

    _Use_decl_annotations_
    std::wstring PadNoteName(int32_t note, bool flats)
    {
        static wchar_t const* const sharpNames[]
        {
            L"C", L"C\u266F", L"D", L"D\u266F", L"E", L"F", L"F\u266F", L"G", L"G\u266F", L"A", L"A\u266F", L"B"
        };

        static wchar_t const* const flatNames[]
        {
            L"C", L"D\u266D", L"D", L"E\u266D", L"E", L"F", L"G\u266D", L"G", L"A\u266D", L"A", L"B\u266D", L"B"
        };

        if (note < 0 || note > 127)
        {
            return {};
        }

        auto const names = flats ? flatNames : sharpNames;

        // Middle C, note 60, is C3: the SDK's MidiMessageHelper default, so every MIDI tool names a note alike.
        return std::wstring{ names[note % 12] } + std::to_wstring((note / 12) - 2);
    }
}
