// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Deliberately free of pch.h and XAML, like the rest of the document layer.

#include "Fretboard.h"
#include "PadGrid.h"

#include <algorithm>
#include <cmath>
#include <iterator>

namespace glass
{
    namespace
    {
        struct TuningPreset
        {
            FretTuning Tuning;

            // Top to bottom as drawn, and zero where the list ends.
            int32_t Notes[MaximumFretStrings];

            // A short string, and the fret it starts at. -1 for none.
            int32_t ShortString;
            int32_t ShortStart;
        };

        // Note numbers the way every Windows MIDI Services tool counts them: 60 is C3, so a
        // guitar's low E is 40, written E1.
        constexpr TuningPreset Presets[FretTuningPresetCount]
        {
            { FretTuning::Standard,        { 64, 59, 55, 50, 45, 40 }, -1, 0 },
            { FretTuning::DropD,           { 64, 59, 55, 50, 45, 38 }, -1, 0 },
            { FretTuning::HalfStepDown,    { 63, 58, 54, 49, 44, 39 }, -1, 0 },
            { FretTuning::WholeStepDown,   { 62, 57, 53, 48, 43, 38 }, -1, 0 },
            { FretTuning::Dadgad,          { 62, 57, 55, 50, 45, 38 }, -1, 0 },
            { FretTuning::OpenG,           { 62, 59, 55, 50, 43, 38 }, -1, 0 },
            { FretTuning::OpenD,           { 62, 57, 54, 50, 45, 38 }, -1, 0 },
            { FretTuning::OpenE,           { 64, 59, 56, 52, 47, 40 }, -1, 0 },
            { FretTuning::SevenString,     { 64, 59, 55, 50, 45, 40, 35 }, -1, 0 },
            { FretTuning::Bass,            { 43, 38, 33, 28 }, -1, 0 },
            { FretTuning::FiveStringBass,  { 43, 38, 33, 28, 23 }, -1, 0 },

            // A ukulele's top string is tuned high, so its lowest note is the C on the third.
            { FretTuning::Ukulele,         { 69, 64, 60, 67 }, -1, 0 },
            { FretTuning::BaritoneUkulele, { 64, 59, 55, 50 }, -1, 0 },

            // One string for each of a mandolin's pairs.
            { FretTuning::Mandolin,        { 76, 69, 62, 55 }, -1, 0 },

            // Open G with the short fifth string, which starts at the fifth fret.
            { FretTuning::Banjo,           { 62, 59, 55, 50, 67 }, 4, 5 },
        };

        wchar_t const* const TuningKeys[]
        {
            L"FretTuningStandard",
            L"FretTuningDropD",
            L"FretTuningHalfStepDown",
            L"FretTuningWholeStepDown",
            L"FretTuningDadgad",
            L"FretTuningOpenG",
            L"FretTuningOpenD",
            L"FretTuningOpenE",
            L"FretTuningSevenString",
            L"FretTuningBass",
            L"FretTuningFiveStringBass",
            L"FretTuningUkulele",
            L"FretTuningBaritoneUkulele",
            L"FretTuningMandolin",
            L"FretTuningBanjo",
            L"FretTuningCustom",
        };

        static_assert(std::size(TuningKeys) == static_cast<size_t>(FretTuning::Custom) + 1);

        // A gap either side of the strum area and above the neck strip.
        constexpr double RegionGap = 6.0;

        // The fraction of the way from the nut to the bridge a wire sits at, on a real neck.
        // Each fret is a semitone, so each is a twelfth root of two shorter than the last.
        double WireFraction(_In_ int32_t wire) noexcept
        {
            return 1.0 - std::pow(2.0, -static_cast<double>(wire) / 12.0);
        }

        // A point drawn on a mirrored neck, as it would be on one that isn't.
        double Unmirror(_In_ FretboardLayout const& layout, _In_ double x) noexcept
        {
            return layout.Mirrored ? layout.Width - x : x;
        }

        // The middle of each fret that shows, open column first, measured as though the neck
        // weren't mirrored.
        double CenterU(_In_ FretboardLayout const& layout, _In_ int32_t fret) noexcept
        {
            if (fret <= 0 || layout.WireX.size() < 2)
            {
                return Unmirror(layout, (layout.OpenLeft + layout.OpenRight) / 2.0);
            }

            auto const cell = std::clamp(fret, layout.FirstCell, layout.LastCell) - layout.FirstCell;
            auto const left = Unmirror(layout, layout.WireX[static_cast<size_t>(cell)]);
            auto const right = Unmirror(layout, layout.WireX[static_cast<size_t>(cell) + 1]);

            return (left + right) / 2.0;
        }
    }

    _Use_decl_annotations_
    bool IsFretboard(ControlKind kind) noexcept
    {
        return kind == ControlKind::Fretboard;
    }

    _Use_decl_annotations_
    std::vector<FretString> TuningStrings(FretTuning tuning)
    {
        std::vector<FretString> strings{};

        for (auto const& preset : Presets)
        {
            if (preset.Tuning != tuning)
            {
                continue;
            }

            for (int32_t index = 0; index < MaximumFretStrings && preset.Notes[index] != 0; ++index)
            {
                FretString string{};

                string.Note = preset.Notes[index];
                string.StartFret = index == preset.ShortString ? preset.ShortStart : 0;

                strings.push_back(string);
            }
        }

        return strings;
    }

    _Use_decl_annotations_
    FretTuning MatchingTuning(std::vector<FretString> const& strings) noexcept
    {
        for (auto const& preset : Presets)
        {
            size_t count = 0;

            while (count < static_cast<size_t>(MaximumFretStrings) && preset.Notes[count] != 0)
            {
                ++count;
            }

            if (count != strings.size())
            {
                continue;
            }

            auto same = true;

            for (size_t index = 0; index < count && same; ++index)
            {
                auto const start = static_cast<int32_t>(index) == preset.ShortString ? preset.ShortStart : 0;

                same = strings[index].Note == preset.Notes[index] && strings[index].StartFret == start;
            }

            if (same)
            {
                return preset.Tuning;
            }
        }

        return FretTuning::Custom;
    }

    _Use_decl_annotations_
    wchar_t const* TuningNameResourceKey(FretTuning tuning) noexcept
    {
        auto const index = static_cast<size_t>(tuning);

        return index < std::size(TuningKeys) ? TuningKeys[index] : TuningKeys[std::size(TuningKeys) - 1];
    }

    _Use_decl_annotations_
    FretboardSpec NormalizedFretboard(FretboardSpec spec)
    {
        spec.Frets = std::clamp(spec.Frets, MinimumFrets, MaximumFrets);

        if (spec.Strings.empty())
        {
            spec.Strings = TuningStrings(spec.Tuning == FretTuning::Custom ? FretTuning::Standard : spec.Tuning);
        }

        if (spec.Strings.size() > static_cast<size_t>(MaximumFretStrings))
        {
            spec.Strings.resize(static_cast<size_t>(MaximumFretStrings));
        }

        for (auto& string : spec.Strings)
        {
            string.Note = std::clamp(string.Note, 0, 127);
            string.StartFret = std::clamp(string.StartFret, 0, spec.Frets - 1);
        }

        // The name always says what the strings are, whatever the file claimed.
        spec.Tuning = MatchingTuning(spec.Strings);

        spec.FirstFret = std::clamp(spec.FirstFret, 0, spec.Frets - 1);
        spec.LastFret = std::clamp(spec.LastFret, std::max(spec.FirstFret, 1), spec.Frets);
        spec.Capo = std::clamp(spec.Capo, 0, spec.Frets - 1);
        spec.BendRangeSemitones = std::clamp(spec.BendRangeSemitones, MinimumFretBendSemitones, MaximumFretBendSemitones);

        if (spec.KeyRoot < 0 || spec.KeyRoot > 11)
        {
            spec.KeyRoot = NoKey;
        }

        return spec;
    }

    _Use_decl_annotations_
    int32_t FretNote(FretboardSpec const& spec, int32_t string, int32_t fret) noexcept
    {
        if (string < 0 || static_cast<size_t>(string) >= spec.Strings.size() || fret < 0 || fret > spec.Frets)
        {
            return -1;
        }

        auto const& line = spec.Strings[static_cast<size_t>(string)];
        auto const start = std::max(line.StartFret, 0);
        auto const capo = std::max(spec.Capo, 0);

        int32_t note{ -1 };

        if (fret == 0)
        {
            // A capo below where a short string starts doesn't reach it.
            note = line.Note + std::max(0, capo - start);
        }
        else
        {
            if (fret <= start || fret < capo)
            {
                return -1;
            }

            note = line.Note + (fret - start);
        }

        return note >= 0 && note <= 127 ? note : -1;
    }

    _Use_decl_annotations_
    bool HasFretKey(FretboardSpec const& spec) noexcept
    {
        return spec.KeyRoot >= 0 && spec.KeyRoot <= 11;
    }

    _Use_decl_annotations_
    bool FretPlays(FretboardSpec const& spec, int32_t note) noexcept
    {
        if (note < 0 || note > 127)
        {
            return false;
        }

        if (spec.OutOfKey != OutOfKeyFrets::Mute || !HasFretKey(spec))
        {
            return true;
        }

        return IsInScale(spec.Scale, spec.KeyRoot, note);
    }

    _Use_decl_annotations_
    FretboardLayout LayOutFretboard(FretboardSpec const& spec, double width, double height)
    {
        FretboardLayout layout{};

        width = std::isfinite(width) ? std::max(width, 0.0) : 0.0;
        height = std::isfinite(height) ? std::max(height, 0.0) : 0.0;

        layout.Width = width;
        layout.Height = height;
        layout.Mirrored = spec.Strum == StrumSide::Left;
        layout.HasStrum = spec.Strum != StrumSide::None;

        // Laid out as for a right-handed player, then turned round at the end.
        auto const strumWidth = layout.HasStrum ? std::clamp(width * 0.18, 32.0, 240.0) : 0.0;
        auto const openWidth = std::clamp(width * 0.06, 20.0, 56.0);

        auto const openLeft = 0.0;
        auto const openRight = std::min(openWidth, width);
        auto const neckLeft = openRight;
        auto const neckRight = std::max(neckLeft, width - (layout.HasStrum ? strumWidth + RegionGap : 0.0));
        auto const strumLeft = std::min(width, neckRight + RegionGap);

        // Only a neck tall enough for the strings and the strip gets one.
        layout.HasStrip = height >= 90.0;

        auto const stripHeight = layout.HasStrip ? std::clamp(height * 0.1, 12.0, 24.0) : 0.0;

        layout.StripBottom = height;
        layout.StripTop = height - stripHeight;
        layout.StringsTop = 0.0;
        layout.StringsBottom = layout.HasStrip ? std::max(0.0, layout.StripTop - RegionGap) : height;

        auto const count = std::max<size_t>(spec.Strings.size(), 1);

        layout.StringPitch = (layout.StringsBottom - layout.StringsTop) / static_cast<double>(count);

        for (size_t index = 0; index < spec.Strings.size(); ++index)
        {
            layout.StringY.push_back(layout.StringsTop + layout.StringPitch * (static_cast<double>(index) + 0.5));
        }

        layout.FirstCell = std::max(spec.FirstFret, 1);
        layout.LastCell = std::max(spec.LastFret, layout.FirstCell);

        auto const low = WireFraction(layout.FirstCell - 1);
        auto const high = WireFraction(layout.LastCell);
        auto const span = high - low;

        auto const flip = [&](double u) { return layout.Mirrored ? width - u : u; };

        for (auto wire = layout.FirstCell - 1; wire <= layout.LastCell; ++wire)
        {
            auto const fraction = span > 0.0 ? (WireFraction(wire) - low) / span : 0.0;

            layout.WireX.push_back(flip(neckLeft + fraction * (neckRight - neckLeft)));
        }

        layout.OpenLeft = std::min(flip(openLeft), flip(openRight));
        layout.OpenRight = std::max(flip(openLeft), flip(openRight));
        layout.NeckLeft = std::min(flip(neckLeft), flip(neckRight));
        layout.NeckRight = std::max(flip(neckLeft), flip(neckRight));
        layout.StrumLeft = layout.HasStrum ? std::min(flip(strumLeft), flip(width)) : 0.0;
        layout.StrumRight = layout.HasStrum ? std::max(flip(strumLeft), flip(width)) : 0.0;

        return layout;
    }

    _Use_decl_annotations_
    double FretCenterX(FretboardLayout const& layout, int32_t fret) noexcept
    {
        auto const u = CenterU(layout, fret);

        return layout.Mirrored ? layout.Width - u : u;
    }

    _Use_decl_annotations_
    int32_t StringAtY(FretboardLayout const& layout, double y) noexcept
    {
        if (layout.StringY.empty() || layout.StringPitch <= 0.0 || !std::isfinite(y))
        {
            return -1;
        }

        auto const index = static_cast<int64_t>(std::floor((y - layout.StringsTop) / layout.StringPitch));

        return static_cast<int32_t>(std::clamp<int64_t>(index, 0, static_cast<int64_t>(layout.StringY.size()) - 1));
    }

    _Use_decl_annotations_
    int32_t FretAtX(FretboardLayout const& layout, double x) noexcept
    {
        if (layout.WireX.size() < 2 || !std::isfinite(x))
        {
            return 0;
        }

        auto const u = Unmirror(layout, x);
        auto const nut = Unmirror(layout, layout.WireX.front());

        if (u < nut)
        {
            return 0;
        }

        for (size_t wire = 1; wire < layout.WireX.size(); ++wire)
        {
            if (u < Unmirror(layout, layout.WireX[wire]))
            {
                return layout.FirstCell + static_cast<int32_t>(wire) - 1;
            }
        }

        return layout.LastCell;
    }

    _Use_decl_annotations_
    double FretPositionAtX(FretboardLayout const& layout, double x) noexcept
    {
        if (!std::isfinite(x))
        {
            return 0.0;
        }

        auto const u = Unmirror(layout, x);

        // The open column's middle, then each fret's.
        auto previousU = CenterU(layout, 0);
        auto previousFret = 0.0;

        if (u <= previousU)
        {
            return 0.0;
        }

        for (auto fret = layout.FirstCell; fret <= layout.LastCell; ++fret)
        {
            auto const centerU = CenterU(layout, fret);

            if (u <= centerU)
            {
                auto const reach = centerU - previousU;
                auto const along = reach > 0.0 ? (u - previousU) / reach : 1.0;

                return previousFret + along * (static_cast<double>(fret) - previousFret);
            }

            previousU = centerU;
            previousFret = static_cast<double>(fret);
        }

        return static_cast<double>(layout.LastCell);
    }

    _Use_decl_annotations_
    FretHit FretAtPoint(FretboardLayout const& layout, double x, double y) noexcept
    {
        FretHit hit{};

        if (!std::isfinite(x) || !std::isfinite(y) || x < 0.0 || y < 0.0 || x > layout.Width || y > layout.Height)
        {
            return hit;
        }

        if (layout.HasStrip && y >= layout.StripTop)
        {
            auto const left = std::min(layout.OpenLeft, layout.NeckLeft);
            auto const right = std::max(layout.OpenRight, layout.NeckRight);

            hit.Region = x >= left && x <= right ? FretRegion::Strip : FretRegion::None;
            return hit;
        }

        if (y > layout.StringsBottom)
        {
            return hit;
        }

        hit.String = StringAtY(layout, y);

        if (layout.HasStrum && x >= layout.StrumLeft && x <= layout.StrumRight)
        {
            hit.Region = FretRegion::Strum;
            return hit;
        }

        if (x >= layout.OpenLeft && x <= layout.OpenRight)
        {
            hit.Region = FretRegion::Open;
            return hit;
        }

        if (x >= layout.NeckLeft && x <= layout.NeckRight)
        {
            hit.Region = FretRegion::Neck;
            hit.Fret = FretAtX(layout, x);
            hit.FretPosition = FretPositionAtX(layout, x);
            return hit;
        }

        hit.String = -1;
        return hit;
    }

    _Use_decl_annotations_
    uint32_t StringsCrossed(FretboardLayout const& layout, double y0, double y1, std::span<int32_t> out) noexcept
    {
        if (!std::isfinite(y0) || !std::isfinite(y1) || y0 == y1)
        {
            return 0;
        }

        uint32_t written{ 0 };
        auto const count = static_cast<int32_t>(layout.StringY.size());

        if (y1 > y0)
        {
            for (int32_t index = 0; index < count && written < out.size(); ++index)
            {
                auto const y = layout.StringY[static_cast<size_t>(index)];

                if (y > y0 && y <= y1)
                {
                    out[written++] = index;
                }
            }
        }
        else
        {
            for (auto index = count - 1; index >= 0 && written < out.size(); --index)
            {
                auto const y = layout.StringY[static_cast<size_t>(index)];

                if (y < y0 && y >= y1)
                {
                    out[written++] = index;
                }
            }
        }

        return written;
    }

    _Use_decl_annotations_
    double StrumVelocity(double distance, double milliseconds) noexcept
    {
        constexpr double Softest = 0.3;

        // A sweep across six strings in about a tenth of a second plays at full strength.
        constexpr double FastestSpeed = 3.0;

        if (!std::isfinite(distance) || !std::isfinite(milliseconds))
        {
            return Softest;
        }

        auto const speed = std::abs(distance) / std::max(milliseconds, 1.0);

        return std::clamp(Softest + (1.0 - Softest) * speed / FastestSpeed, Softest, 1.0);
    }

    _Use_decl_annotations_
    double CrossBend(int32_t bendRangeSemitones, double across, double stringPitch) noexcept
    {
        if (!std::isfinite(across) || !std::isfinite(stringPitch) || stringPitch <= 0.0 || bendRangeSemitones <= 0)
        {
            return 0.0;
        }

        auto const reach = std::abs(across) / stringPitch;
        auto const fraction = std::clamp((reach - BendDeadZone) / (BendFullReach - BendDeadZone), 0.0, 1.0);

        return fraction * static_cast<double>(bendRangeSemitones);
    }

    _Use_decl_annotations_
    double SlideBend(FretSlide slide, int32_t struckFret, int32_t fret, double fretPosition) noexcept
    {
        switch (slide)
        {
        case FretSlide::Stepped:
            return static_cast<double>(fret - struckFret);

        case FretSlide::Smooth:
            return std::isfinite(fretPosition) ? fretPosition - static_cast<double>(struckFret) : 0.0;

        default:
            return 0.0;
        }
    }

    _Use_decl_annotations_
    double StripX(FretboardSpec const& spec, FretboardLayout const& layout, double fret) noexcept
    {
        auto const left = std::min(layout.OpenLeft, layout.NeckLeft);
        auto const right = std::max(layout.OpenRight, layout.NeckRight);
        auto const fraction = spec.Frets > 0 ? std::clamp(fret / static_cast<double>(spec.Frets), 0.0, 1.0) : 0.0;

        return layout.Mirrored ? right - fraction * (right - left) : left + fraction * (right - left);
    }

    _Use_decl_annotations_
    int32_t NeckWindowAt(FretboardSpec const& spec, FretboardLayout const& layout, double x) noexcept
    {
        auto const left = std::min(layout.OpenLeft, layout.NeckLeft);
        auto const right = std::max(layout.OpenRight, layout.NeckRight);
        auto const firstCell = std::max(spec.FirstFret, 1);
        auto const span = std::max(spec.LastFret - firstCell, 0);

        if (right <= left || !std::isfinite(x) || spec.Frets <= 0)
        {
            return firstCell;
        }

        auto fraction = std::clamp((x - left) / (right - left), 0.0, 1.0);

        if (layout.Mirrored)
        {
            fraction = 1.0 - fraction;
        }

        auto const center = fraction * static_cast<double>(spec.Frets);
        auto const first = static_cast<int32_t>(std::lround(center - static_cast<double>(span) / 2.0));

        return std::clamp(first, 1, std::max(spec.Frets - span, 1));
    }

    _Use_decl_annotations_
    FretboardSpec WithNeckAt(FretboardSpec spec, int32_t firstCell) noexcept
    {
        // Frets 0 and 1 both start at the nut, so the number showing is counted from 1.
        auto const currentFirst = std::max(spec.FirstFret, 1);
        auto const span = std::max(spec.LastFret - currentFirst, 0);
        auto const first = std::clamp(firstCell, 1, std::max(spec.Frets - span, 1));

        if (first == currentFirst)
        {
            return spec;
        }

        spec.FirstFret = first;
        spec.LastFret = std::min(first + span, spec.Frets);

        return spec;
    }

    _Use_decl_annotations_
    int32_t InlayDots(int32_t fret) noexcept
    {
        if (fret <= 0)
        {
            return 0;
        }

        switch (fret % 12)
        {
        case 0:
            return 2;

        case 3:
        case 5:
        case 7:
        case 9:
            return 1;

        default:
            return 0;
        }
    }
}
