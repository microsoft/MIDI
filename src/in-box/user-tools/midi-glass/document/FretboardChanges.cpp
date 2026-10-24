// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Deliberately free of pch.h and XAML, like the rest of the document layer.

#include "FretboardChanges.h"

#include <algorithm>
#include <iterator>

namespace glass
{
    namespace
    {
        bool IsOfferedScale(_In_ int32_t scale) noexcept
        {
            return std::any_of(std::begin(PadScaleOrder), std::end(PadScaleOrder),
                [scale](MusicalScale offered) { return static_cast<int32_t>(offered) == scale; });
        }

        int32_t Wrap(_In_ int32_t value, _In_ int32_t count) noexcept
        {
            return ((value % count) + count) % count;
        }

        bool IsFretboardWithId(_In_ Control const& control) noexcept
        {
            return IsFretboard(control.Kind) && !control.Id.empty();
        }
    }

    _Use_decl_annotations_
    FretboardSpec WithFretState(FretboardSpec spec, PadGridState const& state)
    {
        if (state.Tuning >= 0 && state.Tuning < FretTuningPresetCount)
        {
            spec.Tuning = static_cast<FretTuning>(state.Tuning);
            spec.Strings = TuningStrings(spec.Tuning);
        }

        if (state.Capo >= 0)
        {
            spec.Capo = std::clamp(state.Capo, 0, std::max(spec.Frets - 1, 0));
        }

        if (state.NeckFirst >= 1)
        {
            spec = WithNeckAt(std::move(spec), state.NeckFirst);
        }

        if (HasFretKey(spec))
        {
            if (state.Root >= 0 && state.Root <= 11)
            {
                spec.KeyRoot = state.Root;
            }

            if (IsOfferedScale(state.Scale))
            {
                spec.Scale = static_cast<MusicalScale>(state.Scale);
            }
        }

        return spec;
    }

    _Use_decl_annotations_
    bool ReachesFretboards(PadGridFunction function) noexcept
    {
        switch (function)
        {
        case PadGridFunction::CapoUp:
        case PadGridFunction::CapoDown:
        case PadGridFunction::UpTheNeck:
        case PadGridFunction::DownTheNeck:
        case PadGridFunction::MuteStrings:
        case PadGridFunction::NextRoot:
        case PadGridFunction::PreviousRoot:
        case PadGridFunction::NextScale:
        case PadGridFunction::PreviousScale:
        case PadGridFunction::Reset:
            return true;

        default:
            return false;
        }
    }

    _Use_decl_annotations_
    PadGridState ChangedFretState(FretboardSpec const& built, PadGridState state, PadGridFunction function)
    {
        auto const now = WithFretState(built, state);
        auto next = state;

        switch (function)
        {
        case PadGridFunction::CapoUp:
            if (now.Capo + 1 > now.Frets - 1)
            {
                return state;
            }

            next.Capo = now.Capo + 1;
            break;

        case PadGridFunction::CapoDown:
            if (now.Capo <= 0)
            {
                return state;
            }

            next.Capo = now.Capo - 1;
            break;

        case PadGridFunction::UpTheNeck:
        case PadGridFunction::DownTheNeck:
        {
            auto const first = std::max(now.FirstFret, 1);
            auto const moved = WithNeckAt(now, first + (function == PadGridFunction::UpTheNeck ? 1 : -1));
            auto const movedFirst = std::max(moved.FirstFret, 1);

            if (movedFirst == first)
            {
                return state;
            }

            next.NeckFirst = movedFirst;
            break;
        }

        case PadGridFunction::NextRoot:
        case PadGridFunction::PreviousRoot:
            if (!HasFretKey(built))
            {
                return state;
            }

            next.Root = Wrap(now.KeyRoot + (function == PadGridFunction::NextRoot ? 1 : -1), 12);
            break;

        case PadGridFunction::NextScale:
        case PadGridFunction::PreviousScale:
        {
            if (!HasFretKey(built))
            {
                return state;
            }

            constexpr auto count = static_cast<int32_t>(std::size(PadScaleOrder));

            auto const found = std::find(std::begin(PadScaleOrder), std::end(PadScaleOrder), now.Scale);
            auto const index = found == std::end(PadScaleOrder) ? 0 : static_cast<int32_t>(found - std::begin(PadScaleOrder));
            auto const nextIndex = Wrap(index + (function == PadGridFunction::NextScale ? 1 : -1), count);

            next.Scale = static_cast<int32_t>(PadScaleOrder[static_cast<size_t>(nextIndex)]);
            break;
        }

        case PadGridFunction::Reset:
            return PadGridState{};

        default:
            return state;
        }

        return next;
    }

    _Use_decl_annotations_
    PadGridState MovedNeckState(FretboardSpec const& built, PadGridState state, int32_t firstFret)
    {
        auto const now = WithFretState(built, state);
        auto const moved = WithNeckAt(now, firstFret);

        if (std::max(moved.FirstFret, 1) == std::max(now.FirstFret, 1))
        {
            return state;
        }

        state.NeckFirst = std::max(moved.FirstFret, 1);

        return state;
    }

    _Use_decl_annotations_
    FretTuning TuningAtPosition(int32_t position) noexcept
    {
        return static_cast<FretTuning>(std::clamp(position, 0, FretTuningPresetCount - 1));
    }

    _Use_decl_annotations_
    Control const* ShownFretboard(LayoutDocument const& document, PadGridTargets const& targets, bool needsKey) noexcept
    {
        for (auto const& page : document.Pages)
        {
            for (auto const& control : page.Controls)
            {
                if (IsFretboardWithId(control) &&
                    TargetsGrid(targets, control.Id) &&
                    (!needsKey || HasFretKey(control.Fretboard)))
                {
                    return &control;
                }
            }
        }

        return nullptr;
    }

    _Use_decl_annotations_
    FretboardReading ReadFretboard(LayoutDocument const& document, PadGridTargets const& targets, PadGridStates const& states) noexcept
    {
        FretboardReading reading{};

        auto const* const fretboard = ShownFretboard(document, targets, false);

        if (fretboard == nullptr)
        {
            return reading;
        }

        try
        {
            auto const spec = WithFretState(fretboard->Fretboard, StateOf(states, fretboard->Id));

            reading.HasFretboard = true;
            reading.Capo = spec.Capo;
            reading.Tuning = MatchingTuning(spec.Strings);
        }
        catch (...)
        {
            reading = FretboardReading{};
        }

        return reading;
    }
}
