// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "SequenceStrum.h"
#include "Harmony.h"
#include "HarmonyFollow.h"
#include "SequenceArpeggiator.h"
#include "SequenceGenerator.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <map>
#include <mutex>
#include <optional>

namespace midisequencer
{
    namespace
    {
        // Standard tuning, lowest string first, and a ukulele's G C E A from the G string, which is
        // the one a down stroke hits first even though it's tuned high.
        constexpr std::array<uint8_t, 6> GuitarTuning{ 40, 45, 50, 55, 59, 64 };
        constexpr std::array<uint8_t, 4> UkuleleTuning{ 67, 60, 64, 69 };

        // The highest fret a shape starts at, and how far a hand reaches from there.
        constexpr int32_t HighestPosition = 9;
        constexpr int32_t Reach = 3;

        // The quietest a note can be and still be a note: 1 in MIDI 1.0's 7 bits.
        constexpr double QuietestVelocity = 512.0;

        // A muted stroke, and how far a hand drifts at most.
        constexpr double MutedMs = 25.0;
        constexpr double DriftMs = 6.0;

        // How far ahead a ringing string looks for the stroke that stops it.
        constexpr int64_t RingLookahead = 64;

        uint64_t Mix(_In_ uint64_t value) noexcept
        {
            value += 0x9E3779B97F4A7C15ull;
            value = (value ^ (value >> 30)) * 0xBF58476D1CE4E5B9ull;
            value = (value ^ (value >> 27)) * 0x94D049BB133111EBull;
            return value ^ (value >> 31);
        }

        // From -1 to 1, the same every time for the same seed, pass and purpose.
        double Drift(_In_ uint32_t seed, _In_ uint64_t pass, _In_ uint64_t purpose) noexcept
        {
            auto const roll = Mix((static_cast<uint64_t>(seed) << 32) ^ Mix(pass * 0x9E3779B97F4A7C15ull + 11) ^ (purpose * 0xBF58476D1CE4E5B9ull));
            return static_cast<double>(roll % 2001) / 1000.0 - 1.0;
        }

        bool IsStroke(_In_ wchar_t c) noexcept
        {
            switch (c)
            {
            case L'D': case L'd': case L'U': case L'u': case L'x': case L'X': case L'R': case L'T':
                return true;
            default:
                return false;
            }
        }

        bool IsUpStroke(_In_ wchar_t c) noexcept
        {
            return c == L'U' || c == L'u';
        }

        bool IsMuted(_In_ wchar_t c) noexcept
        {
            return c == L'x' || c == L'X';
        }

        // Every pitch class once, as 0 to 11.
        std::vector<uint8_t> Classes(_In_ std::vector<uint8_t> const& pitchClasses)
        {
            std::array<bool, 12> present{};

            for (auto const pc : pitchClasses)
            {
                present[pc % 12] = true;
            }

            std::vector<uint8_t> classes{};

            for (uint8_t pc = 0; pc < 12; ++pc)
            {
                if (present[pc])
                {
                    classes.push_back(pc);
                }
            }

            return classes;
        }

        // The best shape on a fretted instrument: the lowest on the neck a hand can hold, with as
        // few strings muted as it can. Fails (empty) when no shape holds every note of the chord,
        // or all but one of a chord of five notes or more.
        template <size_t N>
        std::vector<StrungNote> FrettedShape(_In_ std::array<uint8_t, N> const& tuning, _In_ std::vector<uint8_t> const& classes, _In_ std::optional<uint8_t> bass, _In_ size_t mostMuted)
        {
            std::array<bool, 12> inChord{};

            for (auto const pc : classes)
            {
                inChord[pc] = true;
            }

            auto const allowedMissing = classes.size() >= 5 ? 1u : 0u;
            double bestScore{ std::numeric_limits<double>::max() };
            std::array<int32_t, N> best{};
            size_t bestFirst{ N };

            std::array<int32_t, N> frets{};

            for (size_t first = 0; first <= mostMuted && first < N; ++first)
            {
                for (int32_t position = 0; position <= HighestPosition; ++position)
                {
                    auto const low = std::max(1, position);
                    auto const high = position + Reach;

                    // String by string from the first that sounds, trying each fret that plays a
                    // note of the chord.
                    auto const search = [&](auto&& self, size_t string) -> void
                    {
                        if (string == N)
                        {
                            int32_t minimum{ std::numeric_limits<int32_t>::max() };
                            int32_t maximum{ 0 };
                            uint32_t opens{ 0 };

                            for (auto s = first; s < N; ++s)
                            {
                                if (frets[s] == 0)
                                {
                                    ++opens;
                                }
                                else
                                {
                                    minimum = std::min(minimum, frets[s]);
                                    maximum = std::max(maximum, frets[s]);
                                }
                            }

                            auto const fretted = minimum != std::numeric_limits<int32_t>::max();

                            if (fretted)
                            {
                                if (maximum - minimum > Reach)
                                {
                                    return;
                                }

                                // A finger for each fret above the lowest, and one barre or
                                // finger for the lowest.
                                uint32_t fingers{ 1 };

                                for (auto s = first; s < N; ++s)
                                {
                                    fingers += frets[s] > minimum ? 1u : 0u;
                                }

                                if (fingers > 4)
                                {
                                    return;
                                }

                                // One finger across the lowest fret can't leave a string open under it.
                                size_t lowString{ N };
                                size_t highString{ 0 };

                                for (auto s = first; s < N; ++s)
                                {
                                    if (frets[s] == minimum)
                                    {
                                        lowString = std::min(lowString, s);
                                        highString = std::max(highString, s);
                                    }
                                }

                                for (auto s = lowString + 1; s < highString; ++s)
                                {
                                    if (frets[s] == 0)
                                    {
                                        return;
                                    }
                                }
                            }

                            if (bass.has_value() && (tuning[first] + frets[first]) % 12 != *bass)
                            {
                                return;
                            }

                            std::array<bool, 12> covered{};

                            for (auto s = first; s < N; ++s)
                            {
                                covered[(tuning[s] + frets[s]) % 12] = true;
                            }

                            uint32_t missing{ 0 };

                            for (auto const pc : classes)
                            {
                                missing += covered[pc] ? 0u : 1u;
                            }

                            if (missing > allowedMissing)
                            {
                                return;
                            }

                            // The same note on two strings sounds thin and wastes a string.
                            uint32_t unisons{ 0 };

                            for (auto s = first; s < N; ++s)
                            {
                                for (auto t = s + 1; t < N; ++t)
                                {
                                    unisons += tuning[s] + frets[s] == tuning[t] + frets[t] ? 1u : 0u;
                                }
                            }

                            auto const score = static_cast<double>(unisons) * 2.0 + static_cast<double>(first) * 2.0 + (fretted ? static_cast<double>(minimum) : 0.0) +
                                (fretted ? static_cast<double>(maximum - minimum) : 0.0) - static_cast<double>(opens) * 0.4 + static_cast<double>(missing) * 3.0;

                            if (score < bestScore)
                            {
                                bestScore = score;
                                best = frets;
                                bestFirst = first;
                            }

                            return;
                        }

                        // The first string that sounds plays the bass.
                        auto const fits = [&](int32_t fret) { return inChord[(tuning[string] + fret) % 12] && (string != first || !bass.has_value() || (tuning[string] + fret) % 12 == *bass); };

                        if (fits(0))
                        {
                            frets[string] = 0;
                            self(self, string + 1);
                        }

                        for (auto fret = low; fret <= high; ++fret)
                        {
                            if (fits(fret))
                            {
                                frets[string] = fret;
                                self(self, string + 1);
                            }
                        }
                    };

                    search(search, first);
                }
            }

            std::vector<StrungNote> shape{};

            if (bestFirst >= N)
            {
                return shape;
            }

            for (auto s = bestFirst; s < N; ++s)
            {
                shape.push_back(StrungNote{ static_cast<uint8_t>(s), static_cast<uint8_t>(tuning[s] + best[s]) });
            }

            return shape;
        }

        // The bass low, the rest close together above it, as a pianist's two hands.
        std::vector<StrungNote> TwoHands(_In_ std::vector<uint8_t> const& classes, _In_ uint8_t bass, _In_ uint8_t bassFrom, _In_ uint8_t handFrom)
        {
            std::vector<StrungNote> notes{ StrungNote{ 0, static_cast<uint8_t>(bassFrom + (bass + 12 - bassFrom % 12) % 12) } };
            std::vector<uint8_t> hand{};

            for (auto const pc : classes)
            {
                hand.push_back(static_cast<uint8_t>(handFrom + (pc + 12 - handFrom % 12) % 12));
            }

            std::sort(hand.begin(), hand.end());

            for (auto const number : hand)
            {
                notes.push_back(StrungNote{ static_cast<uint8_t>(notes.size()), number });
            }

            return notes;
        }

        // An instrument's voicing of a chord's notes, worked out afresh.
        std::vector<StrungNote> VoiceClasses(_In_ StrumInstrument instrument, _In_ std::vector<uint8_t> const& classes, _In_ uint8_t bass)
        {
            switch (instrument)
            {
            case StrumInstrument::Guitar:
                if (auto shape = FrettedShape(GuitarTuning, classes, bass, 2); !shape.empty())
                {
                    return shape;
                }

                // A chord no hand holds: the bass on the low strings, the rest above it.
                return TwoHands(classes, bass, 40, 52);

            case StrumInstrument::Ukulele:
                if (auto shape = FrettedShape(UkuleleTuning, classes, std::nullopt, 0); !shape.empty())
                {
                    return shape;
                }

                return TwoHands(classes, classes.front(), 60, 64);

            case StrumInstrument::Piano:
            default:
                return TwoHands(classes, bass, 36, 55);
            }
        }
    }

    _Use_decl_annotations_
    double StrumCurve(StrumShape shape, double x, double bend) noexcept
    {
        x = std::clamp(x, 0.0, 1.0);
        bend = std::clamp(bend, 0.0, 1.0);
        auto const power = 1.0 + 3.0 * bend;

        switch (shape)
        {
        case StrumShape::SpeedingUp:
            return 1.0 - std::pow(1.0 - x, power);

        case StrumShape::SlowingDown:
            return std::pow(x, power);

        case StrumShape::Ease:
        {
            // More time between the first notes and the last, less in the middle.
            auto const smooth = x * x * (3.0 - 2.0 * x);
            return std::clamp(x + bend * (x - smooth), 0.0, 1.0);
        }

        case StrumShape::Rake:
        {
            // The first third of the notes take up to two thirds of the time.
            constexpr double third = 1.0 / 3.0;
            auto const share = third + bend * third;
            return x <= third ? x / third * share : share + (x - third) / (1.0 - third) * (1.0 - share);
        }

        case StrumShape::Even:
        default:
            return x;
        }
    }

    _Use_decl_annotations_
    std::vector<StrungNote> VoiceChord(StrumInstrument instrument, std::vector<uint8_t> const& pitchClasses, uint8_t bass, std::vector<uint8_t> const& notes)
    {
        bass %= 12;

        if (instrument == StrumInstrument::AsWritten)
        {
            auto sorted = notes;
            std::sort(sorted.begin(), sorted.end());
            sorted.erase(std::unique(sorted.begin(), sorted.end()), sorted.end());

            std::vector<StrungNote> strung{};

            for (auto const number : sorted)
            {
                strung.push_back(StrungNote{ static_cast<uint8_t>(strung.size()), static_cast<uint8_t>(number & 0x7F) });
            }

            return strung;
        }

        auto const classes = Classes(pitchClasses);

        if (classes.empty())
        {
            return {};
        }

        // Finding a fretted shape takes a search, and playback asks for the same few chords over
        // and over, from the playback thread and the window's. Kept by instrument, notes and bass.
        static std::mutex cacheLock{};
        static std::map<uint32_t, std::vector<StrungNote>> cache{};

        uint32_t key = (static_cast<uint32_t>(instrument) << 20) | (static_cast<uint32_t>(bass) << 16);

        for (auto const pc : classes)
        {
            key |= 1u << pc;
        }

        {
            std::scoped_lock lock{ cacheLock };

            if (auto const found = cache.find(key); found != cache.end())
            {
                return found->second;
            }
        }

        auto voiced = VoiceClasses(instrument, classes, bass);

        {
            std::scoped_lock lock{ cacheLock };

            // 4 instruments, 12 basses and 4096 sets of notes at most; far fewer in practice.
            if (cache.size() >= 4096)
            {
                cache.clear();
            }

            cache.emplace(key, voiced);
        }

        return voiced;
    }

    _Use_decl_annotations_
    uint8_t InstrumentStrings(StrumInstrument instrument) noexcept
    {
        switch (instrument)
        {
        case StrumInstrument::Guitar: return static_cast<uint8_t>(GuitarTuning.size());
        case StrumInstrument::Ukulele: return static_cast<uint8_t>(UkuleleTuning.size());
        default: return 0;
        }
    }

    _Use_decl_annotations_
    bool UpStrokeHits(StrumInstrument instrument, uint8_t string, size_t voicedStrings) noexcept
    {
        switch (instrument)
        {
        case StrumInstrument::Guitar:
            return string >= 2;         // the top four of six
        case StrumInstrument::Ukulele:
        case StrumInstrument::Piano:
            return string >= 1;         // all but the G string, or the left hand
        default:
            return static_cast<size_t>(string) >= voicedStrings / 3;
        }
    }

    GeneratorSettings MakeStrumGenerator()
    {
        GeneratorSettings generator{};
        generator.Type = GeneratorType::Strum;
        generator.StepTicks = TicksPerQuarterNote / 2;
        generator.NoteLength = 50;
        generator.ArpSource = ArpeggiatorSource::Chords;
        generator.VelocityLow = 30000;
        generator.VelocityHigh = 52000;
        return generator;
    }

    std::vector<StrumStyle> const& StrumStyles()
    {
        static std::vector<StrumStyle> const styles
        {
            { L"StrumStyleFolk", L"D-DU-UDU", TicksPerQuarterNote / 2, true, 50, 4, StrumShape::SpeedingUp },
            { L"StrumStylePop", L"DUDUDUDU", TicksPerQuarterNote / 2, true, 50, 4, StrumShape::SpeedingUp },
            { L"StrumStyleBallad", L"D---D-DU", TicksPerQuarterNote / 2, true, 50, 4, StrumShape::SpeedingUp },
            { L"StrumStyleChucks", L"DxDxDxDx", TicksPerQuarterNote / 2, true, 50, 4, StrumShape::SpeedingUp },
            { L"StrumStyleReggae", L"--u---u-", TicksPerQuarterNote / 2, false, 30, 4, StrumShape::SpeedingUp },
            { L"StrumStyleFunk", L"DxUxDUxU", TicksPerQuarterNote / 4, false, 70, 4, StrumShape::SpeedingUp },
            { L"StrumStyleTremolo", L"TTTT", TicksPerQuarterNote, true, 50, 8, StrumShape::Even },
            { L"StrumStyleRasgueado", L"R-DUR-DU", TicksPerQuarterNote / 2, true, 50, 4, StrumShape::SpeedingUp },
            { L"StrumStyleBuild", L"DDDDRRRR", TicksPerQuarterNote / 2, true, 50, 3, StrumShape::SpeedingUp },
        };

        return styles;
    }

    _Use_decl_annotations_
    void UseStrumStyle(GeneratorSettings& generator, StrumStyle const& style)
    {
        generator.StrumPattern = style.Pattern;
        generator.StepTicks = style.StepTicks;
        generator.StrumRing = style.Ring;
        generator.NoteLength = style.NoteLength;
        generator.StrumBurst = style.Burst;
        generator.StrumBurstCurve = style.BurstCurve;
    }

    _Use_decl_annotations_
    std::wstring CleanStrumPattern(std::wstring_view pattern)
    {
        std::wstring clean{};

        for (auto const c : pattern)
        {
            if (clean.size() >= MaximumStrumPattern)
            {
                break;
            }

            if (IsStroke(c) || c == L'-')
            {
                clean.push_back(c);
            }
            else if (c == L'.')
            {
                clean.push_back(L'-');
            }
        }

        return clean;
    }

    _Use_decl_annotations_
    void ForEachStrumNote(
        Sequence const& sequence,
        Clip const& clip,
        int64_t passStart,
        int64_t passEnd,
        uint64_t pass,
        int64_t fromTick,
        int64_t toTick,
        std::function<void(Note const&)> const& add,
        std::function<int64_t(int64_t)> const* songTick)
    {
        if (!IsStrum(clip))
        {
            return;
        }

        auto const& generator = *clip.Generator;
        auto const pattern = CleanStrumPattern(generator.StrumPattern);

        if (pattern.empty())
        {
            return;
        }

        // Keys held on a source aren't a Strum's to play, so anything but a track is the row.
        auto const fromChords = generator.ArpSource != ArpeggiatorSource::Track;

        if (fromChords && (!sequence.Harmony || sequence.Harmony->Chords.empty()))
        {
            return;
        }

        Track const* source{ nullptr };

        if (!fromChords)
        {
            source = FindTrack(sequence, generator.ArpTrackId);

            if (source == nullptr || source->IsFolder)
            {
                return;
            }
        }

        auto const stepTicks = std::max<int64_t>(1, generator.StepTicks);
        auto const steps = GeneratorStepCount(clip);
        auto const windowStart = std::max(fromTick, passStart);
        auto const windowEnd = std::min(toTick, passEnd);

        if (windowStart >= windowEnd)
        {
            return;
        }

        std::optional<TempoMap> tempo{};

        if (!sequence.Tempo.empty())
        {
            tempo.emplace(sequence.Tempo);
        }

        auto const toSong = [songTick](int64_t tick) { return songTick != nullptr ? (*songTick)(tick) : tick; };

        auto const ticksForMs = [&](int64_t tick, double ms) -> int64_t
        {
            if (!tempo.has_value())
            {
                // 120 BPM.
                return static_cast<int64_t>(std::llround(ms / 1000.0 * 2.0 * static_cast<double>(TicksPerQuarterNote)));
            }

            auto const song = toSong(tick);
            return tempo->TickAtSeconds(std::max(0.0, tempo->SecondsAtTick(song) + ms / 1000.0)) - song;
        };

        auto const spreadAt = [&](int64_t tick) -> int64_t
        {
            return generator.StrumInTicks
                ? static_cast<int64_t>(std::min(generator.StrumSpreadTicks, MaximumStrumSpreadTicks))
                : std::max<int64_t>(0, ticksForMs(tick, std::min(generator.StrumSpreadMs, MaximumStrumSpreadMs)));
        };

        // A stroke reaches back by its spread when its last note lands on the beat, and forward by
        // its spread and a step for a burst, so the steps either side of the window are looked at.
        auto const reach = (spreadAt(windowStart) * 2 + stepTicks) / stepTicks + 1;
        auto const firstStep = std::max<int64_t>(0, (windowStart - passStart) / stepTicks - reach);
        auto const lastStep = std::min<int64_t>(steps - 1, (windowEnd - 1 - passStart) / stepTicks + reach);

        auto const bend = static_cast<double>(std::min<uint8_t>(generator.StrumBend, 100)) / 100.0;
        auto const tilt = static_cast<double>(std::clamp<int8_t>(generator.StrumTilt, -50, 50)) / 100.0;
        auto const hand = static_cast<double>(std::min<uint8_t>(generator.StrumHand, 100)) / 100.0;
        auto const burst = static_cast<uint32_t>(std::clamp<uint8_t>(generator.StrumBurst, 2, 12));
        auto const rollPass = generator.ChangesEachPass ? pass : 0;
        auto const channel = static_cast<uint8_t>(generator.Channel & 0x0F);

        struct Voiced
        {
            std::vector<StrungNote> Strings{};
            int64_t End{ std::numeric_limits<int64_t>::max() };    // where the chord changes, on this pass's clock
        };

        auto const chordAt = [&](int64_t tick) -> Voiced
        {
            Voiced voiced{};
            std::vector<uint8_t> classes{};
            std::vector<uint8_t> numbers{};
            uint8_t bass{ 0 };

            if (fromChords)
            {
                auto const atSong = toSong(tick);
                auto const playing = ChordAt(*sequence.Harmony, atSong);

                if (playing == nullptr)
                {
                    return voiced;
                }

                classes = ChordPitchClasses(*playing);
                bass = playing->Bass.value_or(playing->Root) % 12;
                classes.push_back(bass);
                numbers = ChordNotes(*playing);
                voiced.End = tick + (playing->Tick + playing->Length - atSong);
            }
            else
            {
                auto const sounding = NotesSoundingAt(sequence, *source, tick);

                if (sounding.Notes.empty())
                {
                    return voiced;
                }

                uint8_t lowest{ 127 };

                for (auto const& held : sounding.Notes)
                {
                    classes.push_back(held.Number % 12);
                    numbers.push_back(held.Number);
                    lowest = std::min(lowest, held.Number);
                }

                bass = lowest % 12;
            }

            voiced.Strings = VoiceChord(generator.Instrument, classes, bass, numbers);
            return voiced;
        };

        // The next stroke after a step that strikes or stops a string.
        auto const nextHit = [&](int64_t step, uint8_t string, size_t voicedStrings) -> int64_t
        {
            for (auto later = step + 1; later < steps && later <= step + RingLookahead; ++later)
            {
                auto const c = pattern[static_cast<size_t>(later) % pattern.size()];

                if (IsStroke(c) && (!IsUpStroke(c) || UpStrokeHits(generator.Instrument, string, voicedStrings)))
                {
                    return passStart + later * stepTicks;
                }
            }

            return passEnd;
        };

        struct Stroke
        {
            int64_t At{ 0 };            // from the step's start
            bool Down{ true };
            double Velocity{ 0 };
            double Spread{ 1.0 };
            bool Muted{ false };
        };

        auto const normal = static_cast<double>(generator.VelocityHigh);
        auto const soft = static_cast<double>(generator.VelocityLow);

        for (auto step = firstStep; step <= lastStep; ++step)
        {
            auto const c = pattern[static_cast<size_t>(step) % pattern.size()];

            if (!IsStroke(c))
            {
                continue;
            }

            auto const strokeTick = passStart + step * stepTicks;
            auto const chord = chordAt(strokeTick);

            if (chord.Strings.empty())
            {
                continue;
            }

            std::vector<Stroke> strokes{};

            switch (c)
            {
            case L'D': strokes.push_back({ 0, true, normal, 1.0, false }); break;
            case L'd': strokes.push_back({ 0, true, soft, 1.0, false }); break;
            case L'U': strokes.push_back({ 0, false, normal * 0.85, 0.7, false }); break;
            case L'u': strokes.push_back({ 0, false, soft * 0.85, 0.7, false }); break;
            case L'x':
            case L'X': strokes.push_back({ 0, true, soft * 0.7, 0.5, true }); break;

            case L'R':
                // Fingers flicked down one after another, closer together or further apart as
                // they go, the last the loudest.
                for (uint32_t k = 0; k < burst; ++k)
                {
                    auto const at = std::llround(static_cast<double>(stepTicks) * StrumCurve(generator.StrumBurstCurve, static_cast<double>(k) / burst, 0.6));
                    strokes.push_back({ at, true, normal * (0.8 + 0.2 * k / (burst - 1)), 0.5, false });
                }
                break;

            case L'T':
                for (uint32_t k = 0; k < burst; ++k)
                {
                    strokes.push_back({ static_cast<int64_t>(stepTicks * k / burst), k % 2 == 0, normal * 0.8, 0.6, false });
                }
                break;

            default:
                break;
            }

            // A little quieter off the beat, as a strumming hand is.
            auto const beat = StartsBeat(sequence.Meter, toSong(strokeTick)) ? 1.0 : 0.92;
            auto const spread = spreadAt(strokeTick);
            std::vector<Note> played{};

            for (size_t si = 0; si < strokes.size(); ++si)
            {
                auto const& stroke = strokes[si];

                std::vector<StrungNote> hit{};

                if (stroke.Down)
                {
                    hit = chord.Strings;
                }
                else
                {
                    for (auto it = chord.Strings.rbegin(); it != chord.Strings.rend(); ++it)
                    {
                        if (UpStrokeHits(generator.Instrument, it->String, chord.Strings.size()))
                        {
                            hit.push_back(*it);
                        }
                    }
                }

                if (hit.empty())
                {
                    continue;
                }

                // Keep bass leaves the lowest string on the beat and spreads the rest.
                auto const keptBass = generator.StrumKeepBass && stroke.Down && hit.size() > 1;
                auto const spreadOver = keptBass ? hit.size() - 1 : hit.size();
                auto const strokeSpread = static_cast<double>(spread) * stroke.Spread;

                auto const shift = generator.StrumLand == StrumLands::Middle ? -std::llround(strokeSpread * StrumCurve(generator.StrumCurve, 0.5, bend))
                    : generator.StrumLand == StrumLands::Last ? -std::llround(strokeSpread)
                    : 0;

                // The next stroke in a burst stops what this one started.
                auto const burstNext = si + 1 < strokes.size() ? std::optional<int64_t>{ strokeTick + strokes[si + 1].At } : std::nullopt;

                for (size_t j = 0; j < hit.size(); ++j)
                {
                    auto const& string = hit[j];
                    auto const bassNote = keptBass && j == 0;
                    auto const place = bassNote ? 0 : (keptBass ? j - 1 : j);
                    auto const x = spreadOver > 1 ? static_cast<double>(place) / static_cast<double>(spreadOver - 1) : 0.0;

                    auto const purpose = static_cast<uint64_t>(step) * 4096 + si * 64 + string.String;
                    auto const drift = std::llround(Drift(clip.Seed, rollPass, purpose) * hand * static_cast<double>(ticksForMs(strokeTick, DriftMs)));
                    auto const offset = bassNote ? 0 : std::llround(strokeSpread * StrumCurve(generator.StrumCurve, x, bend)) + shift;

                    auto start = strokeTick + stroke.At + offset + drift;
                    start = std::max(start, passStart);

                    if (start >= passEnd)
                    {
                        continue;
                    }

                    auto velocity = stroke.Velocity * beat * (1.0 + tilt * (1.0 - 2.0 * x));
                    velocity *= 1.0 + Drift(clip.Seed, rollPass, purpose | (1ull << 40)) * hand * 0.08;

                    int64_t length{ 1 };

                    if (stroke.Muted)
                    {
                        length = std::max<int64_t>(1, ticksForMs(start, MutedMs));
                    }
                    else if (generator.StrumRing)
                    {
                        auto end = burstNext.has_value() ? *burstNext : nextHit(step, string.String, chord.Strings.size());
                        end = std::min({ end, chord.End, passEnd });
                        length = std::max<int64_t>(1, end - start);
                    }
                    else
                    {
                        auto const room = strokes.size() > 1 ? stepTicks / static_cast<int64_t>(strokes.size()) : stepTicks;
                        length = std::max<int64_t>(1, room * generator.NoteLength / 100);
                    }

                    Note note{};
                    note.Tick = start;
                    note.Length = length;
                    note.Channel = channel;
                    note.Number = string.Number;
                    note.Velocity = static_cast<uint16_t>(std::clamp(std::round(velocity), QuietestVelocity, 65535.0));
                    note.ReleaseVelocity = 0x8000;
                    played.push_back(note);
                }
            }

            if (played.empty())
            {
                continue;
            }

            // A key switch goes just before the first note of a stroke that changes between open
            // and muted, and before the first stroke of each pass.
            if (generator.StrumMute == StrumMuting::KeySwitch)
            {
                auto const muted = IsMuted(c);
                std::optional<bool> previous{};

                for (auto earlier = step - 1; earlier >= 0; --earlier)
                {
                    auto const before = pattern[static_cast<size_t>(earlier) % pattern.size()];

                    if (IsStroke(before))
                    {
                        previous = IsMuted(before);
                        break;
                    }
                }

                if (!previous.has_value() || *previous != muted)
                {
                    auto first = played.front().Tick;

                    for (auto const& note : played)
                    {
                        first = std::min(first, note.Tick);
                    }

                    Note keySwitch{};
                    keySwitch.Tick = std::max(passStart, first - 1);
                    keySwitch.Length = 1;
                    keySwitch.Channel = channel;
                    keySwitch.Number = static_cast<uint8_t>((muted ? generator.StrumMuteKey : generator.StrumOpenKey) & 0x7F);
                    keySwitch.Velocity = 0x8000;
                    keySwitch.ReleaseVelocity = 0x8000;

                    if (keySwitch.Tick >= windowStart && keySwitch.Tick < windowEnd)
                    {
                        add(keySwitch);
                    }
                }
            }

            for (auto const& note : played)
            {
                if (note.Tick >= windowStart && note.Tick < windowEnd)
                {
                    add(note);
                }
            }
        }
    }

    _Use_decl_annotations_
    bool IsStrum(Clip const& clip) noexcept
    {
        return clip.Kind == ClipKind::Generator && clip.Generator.has_value() && clip.Generator->Type == GeneratorType::Strum;
    }

    _Use_decl_annotations_
    bool PlaysAlong(Clip const& clip) noexcept
    {
        return IsStrum(clip) || IsArpeggiator(clip);
    }
}
