// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "PatternTemplates.h"

#include "SequencePattern.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace midisequencer
{
    namespace
    {
        constexpr int64_t Eighth = TicksPerQuarterNote / 2;

        // The General MIDI drum map.
        constexpr uint8_t Kick = 36;
        constexpr uint8_t SideStick = 37;
        constexpr uint8_t Snare = 38;
        constexpr uint8_t Clap = 39;
        constexpr uint8_t ClosedHat = 42;
        constexpr uint8_t OpenHat = 46;
        constexpr uint8_t Maracas = 70;
        constexpr uint8_t Claves = 75;

        // Velocities out of 127 for each kind of step.
        constexpr uint8_t PlayVelocity = 100;
        constexpr uint8_t AccentVelocity = 127;
        constexpr uint8_t SoftVelocity = 72;
        constexpr uint8_t GhostVelocity = 40;
        constexpr uint8_t MaybeVelocity = 84;
        constexpr uint8_t MaybeChance = 50;

        // ---- drum machine classics. Sixteenth steps read "1e&a2e&a3e&a4e&a". ----

        constexpr DrumTemplateLane FourOnTheFloor[]{
            { Kick,      L"x...x...x...x..." },
            { Snare,     L"....x.......x..." },
            { ClosedHat, L"o.x.o.x.o.x.o.x." },
        };

        constexpr DrumTemplateLane Rock[]{
            { Kick,      L"x.......x.x....." },
            { Snare,     L"....x.......x..." },
            { ClosedHat, L"x.x.x.x.x.x.x.x." },
        };

        constexpr DrumTemplateLane Disco[]{
            { Kick,      L"x...x...x...x..." },
            { Snare,     L"....x.......x..." },
            { ClosedHat, L"xo.oxo.oxo.oxo.o" },
            { OpenHat,   L"..x...x...x...x." },
        };

        constexpr DrumTemplateLane Funk[]{
            { Kick,      L"x..x..x...x....." },
            { Snare,     L"....X..g.g..X..g" },
            { ClosedHat, L"xoxoxoxoxoxoxo.o" },
            { OpenHat,   L"..............x." },
        };

        // Eighth steps, swung to triplets.
        constexpr DrumTemplateLane Shuffle[]{
            { Kick,      L"x...x..." },
            { Snare,     L"..x...x." },
            { ClosedHat, L"xoxoxoxo" },
        };

        constexpr DrumTemplateLane Breakbeat[]{
            { Kick,      L"x.x.......x..x.." },
            { Snare,     L"....x..g....x.g." },
            { ClosedHat, L"x.x.x.x.x.x.xxx." },
        };

        // ---- electronic ----

        constexpr DrumTemplateLane House[]{
            { Kick,      L"x...x...x...x..." },
            { Clap,      L"....x.......x..." },
            { ClosedHat, L".o.o.o.o.o.o.o.o" },
            { OpenHat,   L"..x...x...x...x." },
        };

        constexpr DrumTemplateLane Techno[]{
            { Kick,      L"x...x...x...x..." },
            { Clap,      L"....o.......o..." },
            { ClosedHat, L"oo.ooo.ooo.ooo.o" },
            { OpenHat,   L"..x...x...x...x." },
            { SideStick, L"...o.....o....o." },
        };

        constexpr DrumTemplateLane Electro[]{
            { Kick,      L"x.....x..x....x." },
            { Clap,      L"....x.......x..." },
            { ClosedHat, L"x.xxx.x.x.xxx.x." },
        };

        constexpr DrumTemplateLane UkGarage[]{
            { Kick,      L"x.........x..x.." },
            { Clap,      L"....x.......x..." },
            { ClosedHat, L"x.xox.xox.xox.xo" },
        };

        constexpr DrumTemplateLane DrumAndBass[]{
            { Kick,      L"x.........x....." },
            { Snare,     L"....x..g....x..." },
            { ClosedHat, L"x.x.x.x.x.x.x.x." },
        };

        constexpr DrumTemplateLane Dubstep[]{
            { Kick,      L"x......x..x....." },
            { Snare,     L"........x......." },
            { ClosedHat, L"x.x.x.x.x.xxx.x." },
        };

        // ---- hip hop ----

        constexpr DrumTemplateLane BoomBap[]{
            { Kick,      L"x......x..x....." },
            { Snare,     L"....x.......x..." },
            { ClosedHat, L"x.x.x.x.x.x.x.x." },
        };

        // The hat on each "a" plays about half the time, so the loop doesn't repeat exactly.
        constexpr DrumTemplateLane LoFi[]{
            { Kick,      L"x.........x.x..." },
            { Snare,     L"....x.......x..." },
            { ClosedHat, L"x.o?x.o?x.o?x.o?" },
        };

        // Half time: the clap is on beat 3. The hats roll in triplets and thirty-second notes.
        constexpr DrumTemplateLane Trap[]{
            { Kick,      L"x.....x...x....x" },
            { Clap,      L"........x......." },
            { ClosedHat, L"x.x.x.x.x.x3x.2." },
        };

        // Half time, with hats grouped 3 + 3 + 2.
        constexpr DrumTemplateLane Drill[]{
            { Kick,      L"x......x..x....." },
            { Snare,     L"........x....x.." },
            { ClosedHat, L"x..x..x.x..x..x." },
        };

        // ---- Latin, Caribbean and African ----

        // The dembow: a kick on every beat and the snare on 3 + 3 + 2.
        constexpr DrumTemplateLane Reggaeton[]{
            { Kick,      L"x...x...x...x..." },
            { Snare,     L"...x..x....x..x." },
            { ClosedHat, L"o.o.o.o.o.o.o.o." },
        };

        constexpr DrumTemplateLane Afrobeats[]{
            { Kick,      L"x.....x...x....." },
            { Clap,      L"....x.......x..." },
            { SideStick, L"...x..x....x..x." },
            { Maracas,   L"x.oox.oox.oox.oo" },
        };

        constexpr DrumTemplateLane BossaNova[]{
            { Kick,      L"x..xx..xx..xx..x" },
            { SideStick, L"x..x..x...x..x.." },
            { ClosedHat, L"o.o.o.o.o.o.o.o." },
        };

        // Eighth steps: the kick and the rim drop together on beat 3.
        constexpr DrumTemplateLane OneDrop[]{
            { Kick,      L"....x..." },
            { SideStick, L"....x..." },
            { ClosedHat, L"xoxoxoxo" },
        };

        // The 3-2 son clave.
        constexpr DrumTemplateLane SonClave[]{
            { Claves,    L"x..x..x...x.x..." },
            { Maracas,   L"x.oox.oox.oox.oo" },
        };

        constexpr PatternTemplate Templates[]{
            { .Id = L"FourOnTheFloor", .Group = PatternTemplateGroup::DrumMachine, .BeatsPerMinute = 120, .Lanes = FourOnTheFloor },
            { .Id = L"Rock", .Group = PatternTemplateGroup::DrumMachine, .BeatsPerMinute = 110, .Lanes = Rock },
            { .Id = L"Disco", .Group = PatternTemplateGroup::DrumMachine, .BeatsPerMinute = 118, .Lanes = Disco },
            { .Id = L"Funk", .Group = PatternTemplateGroup::DrumMachine, .BeatsPerMinute = 100, .Swing = 54, .Lanes = Funk },
            { .Id = L"Shuffle", .Group = PatternTemplateGroup::DrumMachine, .BeatsPerMinute = 120, .StepTicks = Eighth, .Swing = 67, .Lanes = Shuffle },
            { .Id = L"Breakbeat", .Group = PatternTemplateGroup::DrumMachine, .BeatsPerMinute = 130, .Lanes = Breakbeat },

            { .Id = L"House", .Group = PatternTemplateGroup::Electronic, .BeatsPerMinute = 124, .Swing = 54, .Lanes = House },
            { .Id = L"Techno", .Group = PatternTemplateGroup::Electronic, .BeatsPerMinute = 132, .Lanes = Techno },
            { .Id = L"Electro", .Group = PatternTemplateGroup::Electronic, .BeatsPerMinute = 120, .Lanes = Electro },
            { .Id = L"UkGarage", .Group = PatternTemplateGroup::Electronic, .BeatsPerMinute = 132, .Swing = 62, .Lanes = UkGarage },
            { .Id = L"DrumAndBass", .Group = PatternTemplateGroup::Electronic, .BeatsPerMinute = 174, .Lanes = DrumAndBass },
            { .Id = L"Dubstep", .Group = PatternTemplateGroup::Electronic, .BeatsPerMinute = 140, .Lanes = Dubstep },

            { .Id = L"BoomBap", .Group = PatternTemplateGroup::HipHop, .BeatsPerMinute = 90, .Swing = 58, .Lanes = BoomBap },
            { .Id = L"LoFi", .Group = PatternTemplateGroup::HipHop, .BeatsPerMinute = 82, .Swing = 60, .Lanes = LoFi },
            { .Id = L"Trap", .Group = PatternTemplateGroup::HipHop, .BeatsPerMinute = 140, .Lanes = Trap },
            { .Id = L"Drill", .Group = PatternTemplateGroup::HipHop, .BeatsPerMinute = 142, .Lanes = Drill },

            { .Id = L"Reggaeton", .Group = PatternTemplateGroup::World, .BeatsPerMinute = 95, .Lanes = Reggaeton },
            { .Id = L"Afrobeats", .Group = PatternTemplateGroup::World, .BeatsPerMinute = 105, .Lanes = Afrobeats },
            { .Id = L"BossaNova", .Group = PatternTemplateGroup::World, .BeatsPerMinute = 136, .Lanes = BossaNova },
            { .Id = L"OneDrop", .Group = PatternTemplateGroup::World, .BeatsPerMinute = 76, .StepTicks = Eighth, .Swing = 62, .Lanes = OneDrop },
            { .Id = L"SonClave", .Group = PatternTemplateGroup::World, .BeatsPerMinute = 100, .Lanes = SonClave },

            // ---- melodic: semitones from the root, a sixteenth note a word ----

            { .Id = L"AcidTechno", .Mode = PatternMode::Melodic, .Group = PatternTemplateGroup::Basslines, .BeatsPerMinute = 130,
              .Melody = L"0! 0 12~ 0 . 0 10! 0 3 . 0~ 12 0! 7 0 3" },
            { .Id = L"AcidHouse", .Mode = PatternMode::Melodic, .Group = PatternTemplateGroup::Basslines, .BeatsPerMinute = 122,
              .Melody = L"0 . 0 12~ 0 0! . 10 0 . 3~ 5 0! . 12 ." },
            { .Id = L"RollingTechno", .Mode = PatternMode::Melodic, .Group = PatternTemplateGroup::Basslines, .BeatsPerMinute = 128,
              .Melody = L". 0 0 12 . 0 0 0 . 0 0 12 . 0 0 0", .NoteLength = 70 },
            { .Id = L"OffbeatHouse", .Mode = PatternMode::Melodic, .Group = PatternTemplateGroup::Basslines, .BeatsPerMinute = 124,
              .Melody = L". . 0 - . . 0 - . . 0 - . . 0 -", .NoteLength = 80 },
            { .Id = L"OctaveDisco", .Mode = PatternMode::Melodic, .Group = PatternTemplateGroup::Basslines, .BeatsPerMinute = 120,
              .Melody = L"0 . 12 . 0 . 12 . 0 . 12 . 0 . 12 .", .NoteLength = 60 },
            { .Id = L"SynthwaveEighths", .Mode = PatternMode::Melodic, .Group = PatternTemplateGroup::Basslines, .BeatsPerMinute = 100,
              .Melody = L"0 - 0 - 0 - 0 - -4 - -4 - -2 - -2 -", .NoteLength = 80 },
            { .Id = L"FunkBass", .Mode = PatternMode::Melodic, .Group = PatternTemplateGroup::Basslines, .BeatsPerMinute = 100, .Swing = 54,
              .Melody = L"0 . . 0 12! . 0 . . 10 . 12! 0 . 7 10" },
            { .Id = L"ElectroBass", .Mode = PatternMode::Melodic, .Group = PatternTemplateGroup::Basslines, .BeatsPerMinute = 120,
              .Melody = L"0 . . 0 . . 0 . . . 0 . 12 . 0 ." },
            { .Id = L"TranceGallop", .Mode = PatternMode::Melodic, .Group = PatternTemplateGroup::Basslines, .BeatsPerMinute = 138,
              .Melody = L"0 . 0 0 0 . 0 0 0 . 0 0 0 . 0 0", .NoteLength = 60 },
            { .Id = L"UkGarageBass", .Mode = PatternMode::Melodic, .Group = PatternTemplateGroup::Basslines, .BeatsPerMinute = 132, .Swing = 62,
              .Melody = L"0 . . 0 . . 12 . . 0 . 7 . 10 - .", .NoteLength = 70 },
            { .Id = L"ReggaeDub", .Mode = PatternMode::Melodic, .Group = PatternTemplateGroup::Basslines, .BeatsPerMinute = 76,
              .Melody = L"0 - - - . . 0 . 3 - 5 - 7 - 5 -", .NoteLength = 90 },
            { .Id = L"Trap808", .Mode = PatternMode::Melodic, .Group = PatternTemplateGroup::Basslines, .BeatsPerMinute = 140,
              .Melody = L"0 - - - - - . 0 - - 7~ 5 - - 3 -", .NoteLength = 90 },
            { .Id = L"WalkingBass", .Mode = PatternMode::Melodic, .Group = PatternTemplateGroup::Basslines, .BeatsPerMinute = 120,
              .Melody = L"0 - - - 4 - - - 7 - - - -1 - - -", .NoteLength = 90 },
            { .Id = L"MinimalTechno", .Mode = PatternMode::Melodic, .Group = PatternTemplateGroup::Basslines, .BeatsPerMinute = 126,
              .Melody = L". . 0 . . 0 . . . . 0 . 12! . . .", .NoteLength = 40 },

            { .Id = L"BerlinSchool", .Mode = PatternMode::Melodic, .Group = PatternTemplateGroup::Sequences, .BeatsPerMinute = 120,
              .Melody = L"0 7 12 7 0 7 12 7 3 10 15 10 3 10 15 10", .RootOctaveNote = 48, .NoteLength = 60 },
            { .Id = L"UpDownArpeggio", .Mode = PatternMode::Melodic, .Group = PatternTemplateGroup::Sequences, .BeatsPerMinute = 120,
              .Melody = L"0 3 7 12 15 12 7 3 0 3 7 12 15 12 7 3", .RootOctaveNote = 48, .NoteLength = 60 },
        };

        uint16_t Velocity(_In_ uint8_t outOf127) noexcept
        {
            return static_cast<uint16_t>(ScaleUp(outOf127, 7, 16));
        }

        // A drum step from its character. False for a rest, or for a character that means nothing.
        bool DrumStep(_In_ wchar_t character, _Out_ PatternStep& step) noexcept
        {
            step = PatternStep{};

            switch (character)
            {
            case L'x': step.Velocity = Velocity(PlayVelocity); return true;
            case L'X': step.Velocity = Velocity(AccentVelocity); return true;
            case L'o': step.Velocity = Velocity(SoftVelocity); return true;
            case L'g': step.Velocity = Velocity(GhostVelocity); return true;

            case L'?':
                step.Velocity = Velocity(MaybeVelocity);
                step.Chance = MaybeChance;
                return true;

            default:
                if (character >= L'2' && character <= static_cast<wchar_t>(L'0' + MaximumRatchet))
                {
                    step.Velocity = Velocity(PlayVelocity);
                    step.Ratchet = static_cast<uint8_t>(character - L'0');
                    return true;
                }

                return false;
            }
        }

        std::vector<std::wstring_view> Words(_In_ std::wstring_view text)
        {
            std::vector<std::wstring_view> words{};

            while (!text.empty())
            {
                auto const start = text.find_first_not_of(L' ');

                if (start == std::wstring_view::npos)
                {
                    break;
                }

                text.remove_prefix(start);

                auto const end = std::min(text.find(L' '), text.size());
                words.push_back(text.substr(0, end));
                text.remove_prefix(end);
            }

            return words;
        }

        // A melodic word that plays a note: semitones from the root, then "!" and "~" in either
        // order. False for anything else.
        bool ParseNoteWord(_In_ std::wstring_view word, _Out_ int32_t& semitones, _Out_ bool& accent, _Out_ bool& slide) noexcept
        {
            semitones = 0;
            accent = false;
            slide = false;

            while (!word.empty() && (word.back() == L'!' || word.back() == L'~'))
            {
                (word.back() == L'!' ? accent : slide) = true;
                word.remove_suffix(1);
            }

            auto negative{ false };

            if (!word.empty() && (word.front() == L'-' || word.front() == L'+'))
            {
                negative = word.front() == L'-';
                word.remove_prefix(1);
            }

            if (word.empty() || word.size() > 3)
            {
                return false;
            }

            int32_t value{ 0 };

            for (auto const c : word)
            {
                if (c < L'0' || c > L'9')
                {
                    return false;
                }

                value = value * 10 + (c - L'0');
            }

            semitones = negative ? -value : value;
            return true;
        }

        uint16_t StepsAt(_In_ double ratio, _In_ uint16_t steps) noexcept
        {
            return static_cast<uint16_t>(std::clamp<long long>(std::llround(static_cast<double>(steps) * ratio), 1, MaximumPatternSteps));
        }
    }

    std::span<PatternTemplate const> PatternTemplates() noexcept
    {
        return Templates;
    }

    _Use_decl_annotations_
    PatternTemplate const* FindPatternTemplate(std::wstring_view id) noexcept
    {
        auto const found = std::find_if(std::begin(Templates), std::end(Templates), [id](PatternTemplate const& source) { return source.Id == id; });
        return found != std::end(Templates) ? &*found : nullptr;
    }

    _Use_decl_annotations_
    uint8_t TemplateRootNote(PatternTemplate const& source, uint8_t key) noexcept
    {
        // F sharp and below go up from the template's C, G and above go down to it.
        auto const pitch = static_cast<int32_t>(key % 12);
        auto const note = static_cast<int32_t>(source.RootOctaveNote) + (pitch <= 6 ? pitch : pitch - 12);
        return static_cast<uint8_t>(std::clamp(note, 0, 127));
    }

    _Use_decl_annotations_
    PatternSettings MakePatternFromTemplate(PatternTemplate const& source, uint8_t key, uint8_t channel)
    {
        PatternSettings pattern{};
        pattern.Mode = source.Mode;
        pattern.StepTicks = source.StepTicks;
        pattern.Swing = source.Swing;
        pattern.Channel = static_cast<uint8_t>(channel & 0x0F);
        pattern.LanesKeepLength = true;

        if (source.Mode == PatternMode::Drums)
        {
            size_t longest{ 1 };

            for (auto const& from : source.Lanes)
            {
                longest = std::max(longest, from.Steps.size());
            }

            pattern.StepCount = static_cast<uint16_t>(std::clamp<size_t>(longest, 1, MaximumPatternSteps));

            for (auto const& from : source.Lanes)
            {
                if (pattern.Lanes.size() >= MaximumPatternLanes)
                {
                    break;
                }

                auto lane = MakePatternLane(from.Note, static_cast<uint16_t>(std::clamp<size_t>(from.Steps.size(), 1, MaximumPatternSteps)));

                for (size_t i = 0; i < from.Steps.size() && i < MaximumPatternSteps; ++i)
                {
                    PatternStep step{};

                    if (DrumStep(from.Steps[i], step))
                    {
                        step.Index = static_cast<uint16_t>(i);
                        step.Note = lane.Note;
                        lane.Steps.push_back(step);
                    }
                }

                pattern.Lanes.push_back(std::move(lane));
            }
        }
        else
        {
            auto const root = TemplateRootNote(source, key);
            auto const words = Words(source.Melody);

            pattern.StepCount = static_cast<uint16_t>(std::clamp<size_t>(words.size(), 1, MaximumPatternSteps));

            auto lane = MakePatternLane(root, pattern.StepCount);

            // The step a "-" holds on: an index into the lane's steps.
            auto held = SIZE_MAX;

            for (size_t i = 0; i < words.size() && i < MaximumPatternSteps; ++i)
            {
                auto const word = words[i];

                if (word == L"-")
                {
                    if (held != SIZE_MAX)
                    {
                        auto& step = lane.Steps[held];
                        step.Length = static_cast<uint16_t>(std::min<uint32_t>(step.Length + 100u, MaximumStepLength));
                    }

                    continue;
                }

                held = SIZE_MAX;

                int32_t semitones{ 0 };
                bool accent{ false };
                bool slide{ false };

                if (word == L"." || !ParseNoteWord(word, semitones, accent, slide))
                {
                    continue;
                }

                PatternStep step{};
                step.Index = static_cast<uint16_t>(i);
                step.Note = static_cast<uint8_t>(std::clamp(static_cast<int32_t>(root) + semitones, 0, 127));
                step.Accent = accent;
                step.Slide = slide;
                step.Length = std::clamp<uint16_t>(source.NoteLength, 1, MaximumStepLength);
                step.Velocity = Velocity(PlayVelocity);

                lane.Steps.push_back(step);
                held = lane.Steps.size() - 1;
            }

            pattern.Lanes.push_back(std::move(lane));
        }

        NormalizePattern(pattern);
        return pattern;
    }

    _Use_decl_annotations_
    bool PatternHasSteps(PatternSettings const& pattern) noexcept
    {
        return std::any_of(pattern.Lanes.begin(), pattern.Lanes.end(), [](PatternLane const& lane) { return !lane.Steps.empty(); });
    }

    _Use_decl_annotations_
    void OverlayPattern(PatternSettings& pattern, PatternSettings const& added)
    {
        if (pattern.StepTicks <= 0 || added.StepTicks <= 0)
        {
            return;
        }

        // A step of what goes on top, in the pattern's steps.
        auto const ratio = static_cast<double>(added.StepTicks) / static_cast<double>(pattern.StepTicks);

        if (auto const length = StepsAt(ratio, added.StepCount); length > pattern.StepCount)
        {
            SetPatternStepCount(pattern, length);
        }

        for (auto const& from : added.Lanes)
        {
            PatternLane* target{ nullptr };

            if (pattern.Mode == PatternMode::Melodic)
            {
                if (pattern.Lanes.empty())
                {
                    pattern.Lanes.push_back(MakePatternLane(from.Note, pattern.StepCount));
                }

                target = &pattern.Lanes.front();
            }
            else if (auto const found = std::find_if(pattern.Lanes.begin(), pattern.Lanes.end(), [&from](PatternLane const& lane) { return lane.Note == from.Note; });
                found != pattern.Lanes.end())
            {
                target = &*found;
            }
            else if (pattern.Lanes.size() < MaximumPatternLanes)
            {
                auto lane = MakePatternLane(from.Note, pattern.StepCount);
                lane.Name = from.Name;
                pattern.Lanes.push_back(std::move(lane));
                target = &pattern.Lanes.back();
            }

            if (target == nullptr)
            {
                continue;
            }

            // A lane with a length of its own that's shorter than what goes on it gets longer.
            target->StepCount = std::max(target->StepCount, StepsAt(ratio, from.StepCount));

            for (auto step : from.Steps)
            {
                auto const index = std::llround(static_cast<double>(step.Index) * ratio);

                if (index < 0 || index >= MaximumPatternSteps)
                {
                    continue;
                }

                step.Index = static_cast<uint16_t>(index);
                step.Length = static_cast<uint16_t>(std::clamp<long long>(std::llround(static_cast<double>(step.Length) * ratio), 1, MaximumStepLength));

                if (pattern.Mode == PatternMode::Drums)
                {
                    step.Note = target->Note;
                }

                auto const at = std::lower_bound(target->Steps.begin(), target->Steps.end(), step.Index,
                    [](PatternStep const& existing, uint16_t value) { return existing.Index < value; });

                if (at != target->Steps.end() && at->Index == step.Index)
                {
                    *at = step;
                }
                else
                {
                    target->Steps.insert(at, step);
                }
            }
        }

        NormalizePattern(pattern);
    }
}
