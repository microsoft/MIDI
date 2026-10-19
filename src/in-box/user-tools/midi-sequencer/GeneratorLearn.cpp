// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Learn and Words (design section 28). Learn counts which note follows which in a tune, a clip
// or words, a Markov chain, and walks those counts again each pass: one chain for the notes and
// one for the rhythm. It learns only from what it's given. Words spells text as notes: the letters
// run up the scale, and punctuation makes rests and endings.

#include "GeneratorKinds.h"
#include "SequenceGenerator.h"

#include <algorithm>
#include <cmath>
#include <cwctype>
#include <numeric>

namespace midisequencer
{
    namespace
    {
        // Ode to Joy (Beethoven), Frère Jacques and Twinkle, Twinkle, Little Star, as steps of a
        // major scale from the key note, with sixteenth-note steps.
        struct TuneNote
        {
            int8_t Position;
            uint8_t Gap;
        };

        constexpr TuneNote OdeToJoy[]{
            { 2, 4 }, { 2, 4 }, { 3, 4 }, { 4, 4 }, { 4, 4 }, { 3, 4 }, { 2, 4 }, { 1, 4 },
            { 0, 4 }, { 0, 4 }, { 1, 4 }, { 2, 4 }, { 2, 6 }, { 1, 2 }, { 1, 8 },
            { 2, 4 }, { 2, 4 }, { 3, 4 }, { 4, 4 }, { 4, 4 }, { 3, 4 }, { 2, 4 }, { 1, 4 },
            { 0, 4 }, { 0, 4 }, { 1, 4 }, { 2, 4 }, { 1, 6 }, { 0, 2 }, { 0, 8 } };

        constexpr TuneNote FrereJacques[]{
            { 0, 4 }, { 1, 4 }, { 2, 4 }, { 0, 4 }, { 0, 4 }, { 1, 4 }, { 2, 4 }, { 0, 4 },
            { 2, 4 }, { 3, 4 }, { 4, 8 }, { 2, 4 }, { 3, 4 }, { 4, 8 },
            { 4, 2 }, { 5, 2 }, { 4, 2 }, { 3, 2 }, { 2, 4 }, { 0, 4 },
            { 4, 2 }, { 5, 2 }, { 4, 2 }, { 3, 2 }, { 2, 4 }, { 0, 4 },
            { 0, 4 }, { -3, 4 }, { 0, 8 }, { 0, 4 }, { -3, 4 }, { 0, 8 } };

        constexpr TuneNote Twinkle[]{
            { 0, 4 }, { 0, 4 }, { 4, 4 }, { 4, 4 }, { 5, 4 }, { 5, 4 }, { 4, 8 },
            { 3, 4 }, { 3, 4 }, { 2, 4 }, { 2, 4 }, { 1, 4 }, { 1, 4 }, { 0, 8 },
            { 4, 4 }, { 4, 4 }, { 3, 4 }, { 3, 4 }, { 2, 4 }, { 2, 4 }, { 1, 8 },
            { 4, 4 }, { 4, 4 }, { 3, 4 }, { 3, 4 }, { 2, 4 }, { 2, 4 }, { 1, 8 },
            { 0, 4 }, { 0, 4 }, { 4, 4 }, { 4, 4 }, { 5, 4 }, { 5, 4 }, { 4, 8 },
            { 3, 4 }, { 3, 4 }, { 2, 4 }, { 2, 4 }, { 1, 4 }, { 1, 4 }, { 0, 8 } };

        // Krumhansl and Kessler's key profiles: how much each note of the octave belongs to a
        // major or a minor key, from the key note up.
        constexpr std::array<double, 12> MajorProfile{ 6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88 };
        constexpr std::array<double, 12> MinorProfile{ 6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17 };

        constexpr std::array<int32_t, 7> MajorSteps{ 0, 2, 4, 5, 7, 9, 11 };
        constexpr std::array<int32_t, 7> MinorSteps{ 0, 2, 3, 5, 7, 8, 10 };

        // The semitones of the generator's scale above its key note.
        std::vector<int32_t> Degrees(_In_ ScaleKind scale)
        {
            std::vector<int32_t> degrees{};

            for (auto const note : ScaleNotes(0, scale, 0, 11))
            {
                degrees.push_back(note);
            }

            return degrees.empty() ? std::vector<int32_t>{ 0 } : degrees;
        }

        int32_t FloorDivide(_In_ int32_t value, _In_ int32_t by) noexcept
        {
            return value >= 0 ? value / by : -((-value + by - 1) / by);
        }

        // A position in a seven-note scale as a degree of a scale with `count` notes, and the
        // octave it's in.
        std::pair<int32_t, int32_t> MapPosition(_In_ int32_t position, _In_ size_t count) noexcept
        {
            auto octave = FloorDivide(position, 7);
            auto const degree = position - octave * 7;
            auto mapped = static_cast<int32_t>(std::lround(static_cast<double>(degree) * static_cast<double>(count) / 7.0));

            if (mapped >= static_cast<int32_t>(count))
            {
                mapped = 0;
                ++octave;
            }

            return { octave, mapped };
        }

        // A degree of a scale with `count` notes, `octave` octaves up, as a position in seven.
        int32_t PositionOf(_In_ int32_t index, _In_ size_t count) noexcept
        {
            auto const size = static_cast<int32_t>(std::max<size_t>(1, count));
            auto const octave = FloorDivide(index, size);
            auto const degree = index - octave * size;
            return octave * 7 + static_cast<int32_t>(std::lround(static_cast<double>(degree) * 7.0 / static_cast<double>(size)));
        }

        // Where learned notes land in a generator's key and range: the learned notes' middle
        // octave on the root nearest the middle of the range, and each note folded into it.
        class PositionNotes
        {
        public:
            PositionNotes(_In_ GeneratorSettings const& generator, _In_ std::vector<int32_t> positions) :
                m_ladder(ScaleNotes(generator.ScaleRoot, generator.Scale, generator.LowNote, generator.HighNote)),
                m_degrees(Degrees(generator.Scale)),
                m_low(static_cast<int32_t>(std::min(generator.LowNote, generator.HighNote))),
                m_high(static_cast<int32_t>(std::max(generator.LowNote, generator.HighNote)))
            {
                if (positions.empty())
                {
                    positions.push_back(0);
                }

                std::nth_element(positions.begin(), positions.begin() + static_cast<ptrdiff_t>(positions.size() / 2), positions.end());
                auto const middleOctave = FloorDivide(positions[positions.size() / 2], 7);
                m_base = static_cast<int32_t>(m_ladder[MiddleRootIndex(generator, m_ladder)]) - middleOctave * 12;
            }

            uint8_t operator()(_In_ int32_t position) const
            {
                auto const [octave, degree] = MapPosition(position, m_degrees.size());
                auto number = m_base + octave * 12 + m_degrees[static_cast<size_t>(degree)];

                while (number > m_high && number - 12 >= m_low)
                {
                    number -= 12;
                }

                while (number < m_low && number + 12 <= m_high)
                {
                    number += 12;
                }

                // A range narrower than an octave: the nearest note of the key in it.
                if (number < m_low || number > m_high)
                {
                    auto const nearest = std::min_element(m_ladder.begin(), m_ladder.end(), [number](uint8_t a, uint8_t b)
                    {
                        return std::abs(static_cast<int32_t>(a) - number) < std::abs(static_cast<int32_t>(b) - number);
                    });

                    number = *nearest;
                }

                return static_cast<uint8_t>(std::clamp(number, 0, 127));
            }

        private:
            std::vector<uint8_t> m_ladder{};
            std::vector<int32_t> m_degrees{};
            int32_t m_low{ 0 };
            int32_t m_high{ 127 };
            int32_t m_base{ 0 };
        };

        // The next token of a chain: what followed the last `order` tokens somewhere in what it
        // learned, picked by the roll. The tokens loop, so every context has a next one.
        int32_t NextToken(_In_ std::vector<int32_t> const& tokens, _In_ std::vector<int32_t> const& context, _In_ uint64_t roll)
        {
            auto const n = tokens.size();
            auto const order = context.size();
            std::vector<int32_t> candidates{};

            for (size_t j = 0; j < n; ++j)
            {
                auto matches = true;

                for (size_t t = 0; t < order && matches; ++t)
                {
                    matches = tokens[(j + n * 4 - (order - 1) + t) % n] == context[t];
                }

                if (matches)
                {
                    candidates.push_back(tokens[(j + 1) % n]);
                }
            }

            if (candidates.empty())
            {
                return tokens[roll % n];
            }

            return candidates[roll % candidates.size()];
        }

        // How often the tokens go from one to the other, out of every move they make from the
        // first: 1 when it's the only move made from there, 0 when it was never made. The tokens
        // loop. This counts one token back whatever the chain's memory, so a move the chain had
        // to make can still be a rare one.
        double MoveChance(_In_ std::vector<int32_t> const& tokens, _In_ int32_t from, _In_ int32_t to) noexcept
        {
            size_t moves{ 0 };
            size_t made{ 0 };

            for (size_t i = 0; i < tokens.size(); ++i)
            {
                if (tokens[i] == from)
                {
                    ++moves;
                    made += tokens[(i + 1) % tokens.size()] == to ? 1 : 0;
                }
            }

            return moves == 0 ? 0.0 : static_cast<double>(made) / static_cast<double>(moves);
        }

        bool IsEnding(_In_ wchar_t ch) noexcept
        {
            return ch == L'.' || ch == L'!' || ch == L'?';
        }
    }

    _Use_decl_annotations_
    std::vector<LearnedNote> TuneNotes(uint8_t tune)
    {
        auto const make = [](auto const& notes)
        {
            std::vector<LearnedNote> learned{};

            for (auto const& note : notes)
            {
                learned.push_back(LearnedNote{ note.Position, note.Gap, note.Gap });
            }

            return learned;
        };

        switch (tune)
        {
        case 1:
            return make(FrereJacques);

        case 2:
            return make(Twinkle);

        default:
            return make(OdeToJoy);
        }
    }

    _Use_decl_annotations_
    std::vector<LearnedNote> LearnedSource(GeneratorSettings const& generator)
    {
        switch (generator.LearnFrom)
        {
        case LearnSource::Clip:
            if (generator.Learned.size() >= 2)
            {
                return generator.Learned;
            }
            break;

        case LearnSource::Words:
        {
            // The words spelled, as steps of the scale from the key note.
            auto const ladder = ScaleNotes(generator.ScaleRoot, generator.Scale, generator.LowNote, generator.HighNote);
            auto const count = Degrees(generator.Scale).size();
            auto const spelled = SpellWords(generator);

            size_t first{ 0 };

            while (first < ladder.size() && ladder[first] % 12 != generator.ScaleRoot % 12)
            {
                ++first;
            }

            if (first >= ladder.size())
            {
                first = 0;
            }

            std::vector<size_t> played{};

            for (size_t i = 0; i < spelled.size(); ++i)
            {
                if (spelled[i].Number != 0)
                {
                    played.push_back(i);
                }
            }

            std::vector<LearnedNote> learned{};

            for (size_t p = 0; p < played.size() && learned.size() < MaximumLearnedNotes; ++p)
            {
                auto const& step = spelled[played[p]];
                auto const at = std::lower_bound(ladder.begin(), ladder.end(), step.Number);
                auto const index = static_cast<int32_t>(std::distance(ladder.begin(), at)) - static_cast<int32_t>(first);
                auto const next = p + 1 < played.size() ? spelled[played[p + 1]].Step : step.Step + step.Steps;

                LearnedNote note{};
                note.Position = static_cast<int8_t>(std::clamp(PositionOf(index, count), -64, 63));
                note.Gap = static_cast<uint8_t>(std::clamp<int64_t>(next - step.Step, 1, 16));
                note.Hold = static_cast<uint8_t>(std::clamp<int64_t>(step.Steps, 1, note.Gap));
                learned.push_back(note);
            }

            if (learned.size() >= 2)
            {
                return learned;
            }
            break;
        }

        default:
            break;
        }

        return TuneNotes(generator.Tune);
    }

    _Use_decl_annotations_
    std::vector<LearnedNote> LearnFromNotes(std::vector<Note> const& notes, int64_t stepTicks)
    {
        stepTicks = std::max<int64_t>(1, stepTicks);

        // The melody: the highest note at each step that starts one.
        std::vector<Note> melody{};

        for (auto const& note : notes)
        {
            auto const step = (note.Tick + stepTicks / 2) / stepTicks;
            auto existing = std::find_if(melody.begin(), melody.end(), [&](Note const& kept) { return (kept.Tick + stepTicks / 2) / stepTicks == step; });

            if (existing == melody.end())
            {
                melody.push_back(note);
            }
            else if (note.Number > existing->Number)
            {
                *existing = note;
            }
        }

        std::sort(melody.begin(), melody.end(), [](Note const& a, Note const& b) { return a.Tick < b.Tick; });

        if (melody.size() < 2)
        {
            return {};
        }

        if (melody.size() > MaximumLearnedNotes)
        {
            melody.resize(MaximumLearnedNotes);
        }

        // The key it's in: the key whose profile best matches how long each note sounds.
        std::array<double, 12> sounding{};

        for (auto const& note : melody)
        {
            sounding[note.Number % 12] += static_cast<double>(std::max<int64_t>(1, note.Length));
        }

        auto const mean = [](auto const& values) { return std::accumulate(values.begin(), values.end(), 0.0) / 12.0; };
        auto const soundingMean = mean(sounding);

        int32_t tonic{ 0 };
        auto minor = false;
        auto best = -2.0;

        for (int32_t key = 0; key < 12; ++key)
        {
            for (auto const isMinor : { false, true })
            {
                auto const& profile = isMinor ? MinorProfile : MajorProfile;
                auto const profileMean = mean(profile);

                double both{ 0 };
                double left{ 0 };
                double right{ 0 };

                for (int32_t pc = 0; pc < 12; ++pc)
                {
                    auto const a = sounding[static_cast<size_t>(pc)] - soundingMean;
                    auto const b = profile[static_cast<size_t>((pc - key + 12) % 12)] - profileMean;
                    both += a * b;
                    left += a * a;
                    right += b * b;
                }

                auto const correlation = left > 0 && right > 0 ? both / std::sqrt(left * right) : -1.0;

                if (correlation > best)
                {
                    best = correlation;
                    tonic = key;
                    minor = isMinor;
                }
            }
        }

        auto const& steps = minor ? MinorSteps : MajorSteps;

        // Positions count from the key note at or below the middle of the melody.
        std::vector<int32_t> pitches{};

        for (auto const& note : melody)
        {
            pitches.push_back(note.Number);
        }

        std::nth_element(pitches.begin(), pitches.begin() + static_cast<ptrdiff_t>(pitches.size() / 2), pitches.end());
        auto const middle = pitches[pitches.size() / 2];
        auto const reference = middle - ((middle - tonic) % 12 + 12) % 12;

        std::vector<LearnedNote> learned{};

        for (size_t i = 0; i < melody.size(); ++i)
        {
            auto const semitones = static_cast<int32_t>(melody[i].Number) - reference;
            auto octave = FloorDivide(semitones, 12);
            auto const within = semitones - octave * 12;

            // The nearest note of the key, upward on a tie.
            int32_t degree{ 0 };
            auto gap = INT32_MAX;

            for (int32_t d = 0; d <= 7; ++d)
            {
                auto const pitch = d == 7 ? 12 : steps[static_cast<size_t>(d)];

                if (std::abs(pitch - within) < gap || (std::abs(pitch - within) == gap && pitch > within))
                {
                    gap = std::abs(pitch - within);
                    degree = d;
                }
            }

            auto const startStep = (melody[i].Tick + stepTicks / 2) / stepTicks;
            auto const nextStep = i + 1 < melody.size() ? (melody[i + 1].Tick + stepTicks / 2) / stepTicks : startStep + std::max<int64_t>(1, (melody[i].Length + stepTicks / 2) / stepTicks);

            LearnedNote note{};
            note.Position = static_cast<int8_t>(std::clamp(octave * 7 + degree, -64, 63));
            note.Gap = static_cast<uint8_t>(std::clamp<int64_t>(nextStep - startStep, 1, 16));
            note.Hold = static_cast<uint8_t>(std::clamp<int64_t>((melody[i].Length + stepTicks / 2) / stepTicks, 1, note.Gap));

            // How hard it was played, in five steps, so Learn can play the clip's loud and soft.
            note.Level = static_cast<uint8_t>(1 + std::min<uint32_t>(4, static_cast<uint32_t>(melody[i].Velocity) * 5u / 65536u));
            learned.push_back(note);
        }

        return learned;
    }

    _Use_decl_annotations_
    int32_t ScaleDegree(GeneratorSettings const& generator, uint8_t number) noexcept
    {
        try
        {
            auto const degrees = Degrees(generator.Scale);
            auto const within = (static_cast<int32_t>(number) - static_cast<int32_t>(generator.ScaleRoot % 12) + 120) % 12;
            auto const found = std::find(degrees.begin(), degrees.end(), within);
            return found == degrees.end() ? -1 : static_cast<int32_t>(std::distance(degrees.begin(), found));
        }
        catch (...)
        {
            return -1;
        }
    }

    _Use_decl_annotations_
    std::vector<std::vector<uint32_t>> LearnedMoves(GeneratorSettings const& generator)
    {
        auto const count = Degrees(generator.Scale).size();
        std::vector<std::vector<uint32_t>> moves(count, std::vector<uint32_t>(count, 0));

        auto const source = LearnedSource(generator);

        // The walk treats what it learned as a loop, so the last note leads back to the first.
        for (size_t i = 0; i < source.size() && source.size() >= 2; ++i)
        {
            auto const from = MapPosition(source[i].Position, count).second;
            auto const to = MapPosition(source[(i + 1) % source.size()].Position, count).second;
            ++moves[static_cast<size_t>(from)][static_cast<size_t>(to)];
        }

        return moves;
    }

    _Use_decl_annotations_
    std::vector<uint8_t> LearnedPitches(GeneratorSettings const& generator, std::vector<LearnedNote> const& learned)
    {
        std::vector<int32_t> positions{};

        for (auto const& note : learned)
        {
            positions.push_back(note.Position);
        }

        PositionNotes const noteFor{ generator, positions };
        std::vector<uint8_t> pitches{};

        for (auto const position : positions)
        {
            pitches.push_back(noteFor(position));
        }

        return pitches;
    }

    _Use_decl_annotations_
    std::vector<StepNote> LearnPass(GeneratorSettings const& generator, uint32_t seed, int64_t steps, uint64_t pass)
    {
        auto const source = LearnedSource(generator);

        if (source.size() < 2)
        {
            return {};
        }

        // Keep is the chance a pass plays the one before it again, which gives a tune's repeats.
        auto walk = pass;

        if (generator.Keep >= 100)
        {
            walk = 0;
        }
        else
        {
            while (walk > 0 && GeneratorRoll(seed, walk, 0, 77) % 100 < generator.Keep)
            {
                --walk;
            }
        }

        std::vector<int32_t> pitches{};
        std::vector<int32_t> rhythms{};
        std::vector<int32_t> levels{};

        for (auto const& note : source)
        {
            pitches.push_back(note.Position);
            rhythms.push_back((static_cast<int32_t>(note.Gap) << 8) | note.Hold);
            levels.push_back(note.Level);
        }

        // The clip's own loud and soft, as a third chain, when it gave them. A tune or words
        // don't, so they're as loud as the moves are surprising instead.
        auto const asLearned = generator.VelocityBy == VelocityShape::Learned && std::any_of(levels.begin(), levels.end(), [](int32_t level) { return level > 0; });

        auto const n = source.size();
        auto const order = std::clamp<size_t>(generator.Order, 1, n - 1);
        auto const start = static_cast<size_t>(GeneratorRoll(seed, walk, 0, 80) % n);

        std::vector<int32_t> pitchContext{};
        std::vector<int32_t> rhythmContext{};
        std::vector<int32_t> levelContext{};

        for (size_t t = 0; t < order; ++t)
        {
            pitchContext.push_back(pitches[(start + n * 4 - (order - 1) + t) % n]);
            rhythmContext.push_back(rhythms[(start + n * 4 - (order - 1) + t) % n]);
            levelContext.push_back(levels[(start + n * 4 - (order - 1) + t) % n]);
        }

        // Where the tune's middle octave lands: on the root nearest the middle of the range.
        PositionNotes const noteFor{ generator, pitches };

        std::vector<StepNote> notes{};
        auto pitch = pitches[start];
        auto rhythm = rhythms[start];
        auto level = levels[start];

        // How often the source made the move from the note before to this one, out of all its
        // moves from that note: the first is neither expected nor a surprise.
        auto chance = 0.5;
        int64_t step{ 0 };

        for (uint64_t i = 1; step < steps; ++i)
        {
            StepNote note{};
            note.Step = step;
            note.Steps = std::max(1, rhythm & 0xFF);
            note.Number = noteFor(pitch);
            note.Velocity = VelocityForLoudness(asLearned ? static_cast<double>(std::max(1, level) - 1) / 4.0 : 0.15 + 0.85 * (1.0 - chance));
            notes.push_back(note);

            step += std::max(1, rhythm >> 8);

            auto const previous = pitch;
            pitch = NextToken(pitches, pitchContext, GeneratorRoll(seed, walk, i, 81));
            chance = MoveChance(pitches, previous, pitch);
            rhythm = NextToken(rhythms, rhythmContext, GeneratorRoll(seed, walk, i, 82));

            pitchContext.erase(pitchContext.begin());
            pitchContext.push_back(pitch);
            rhythmContext.erase(rhythmContext.begin());
            rhythmContext.push_back(rhythm);

            if (asLearned)
            {
                level = NextToken(levels, levelContext, GeneratorRoll(seed, walk, i, 83));
                levelContext.erase(levelContext.begin());
                levelContext.push_back(level);
            }
        }

        ApplyKeyRules(generator, notes);
        return notes;
    }

    // ---------------------------------------------------------------- words

    _Use_decl_annotations_
    uint32_t SeedFromWords(std::wstring_view words) noexcept
    {
        auto const first = words.find_first_not_of(L" \t\r\n");
        auto const last = words.find_last_not_of(L" \t\r\n");

        // FNV-1a over the words in lowercase, so "Midnight drive" and "midnight drive" match.
        uint32_t hash{ 0x811C9DC5u };

        if (first != std::wstring_view::npos)
        {
            for (auto const ch : words.substr(first, last - first + 1))
            {
                auto const lower = static_cast<uint32_t>(std::towlower(ch));
                hash ^= lower & 0xFF;
                hash *= 0x01000193u;
                hash ^= (lower >> 8) & 0xFF;
                hash *= 0x01000193u;
            }
        }

        return hash;
    }

    _Use_decl_annotations_
    std::optional<uint32_t> SeedFromHex(std::wstring_view text) noexcept
    {
        auto const first = text.find_first_not_of(L" \t");

        if (first == std::wstring_view::npos)
        {
            return std::nullopt;
        }

        text = text.substr(first, text.find_last_not_of(L" \t") - first + 1);

        if (text.empty() || text.size() > 8)
        {
            return std::nullopt;
        }

        uint32_t seed{ 0 };

        for (auto const ch : text)
        {
            auto const upper = std::towupper(ch);
            auto const digit = std::iswdigit(ch) ? static_cast<int32_t>(ch - L'0') : (upper >= L'A' && upper <= L'F') ? static_cast<int32_t>(upper - L'A' + 10) : -1;

            if (digit < 0)
            {
                return std::nullopt;
            }

            seed = (seed << 4) | static_cast<uint32_t>(digit);
        }

        return seed;
    }

    _Use_decl_annotations_
    std::vector<SpelledStep> SpellWords(GeneratorSettings const& generator)
    {
        auto const ladder = ScaleNotes(generator.ScaleRoot, generator.Scale, generator.LowNote, generator.HighNote);

        // A on the first root in the range, B on the next note of the key, and so on, starting
        // again after the top of the range.
        size_t first{ 0 };

        while (first < ladder.size() && ladder[first] % 12 != generator.ScaleRoot % 12)
        {
            ++first;
        }

        if (first >= ladder.size())
        {
            first = 0;
        }

        auto const span = ladder.size() - first;

        auto const nearest = [&](int32_t from, int32_t pitchClass)
        {
            auto best = ladder[first];
            auto bestGap = INT32_MAX;

            for (auto const candidate : ladder)
            {
                if (candidate % 12 == pitchClass)
                {
                    auto const gap = std::abs(static_cast<int32_t>(candidate) - from);

                    if (gap < bestGap)
                    {
                        best = candidate;
                        bestGap = gap;
                    }
                }
            }

            return best;
        };

        std::vector<SpelledStep> spelled{};
        int64_t step{ 0 };
        wchar_t previous{ 0 };
        int32_t lastNote{ ladder[first] };

        auto const rest = [&](int64_t steps)
        {
            spelled.push_back(SpelledStep{ step, steps, 0, false, 0 });
            step += steps;
            previous = 0;
        };

        for (auto const ch : generator.Text)
        {
            if (spelled.size() >= MaximumGeneratorText * 2)
            {
                break;
            }

            auto const upper = static_cast<wchar_t>(std::towupper(ch));

            if (std::iswalpha(ch) || std::iswdigit(ch))
            {
                // A letter said twice holds its note.
                if (upper == previous && !spelled.empty() && spelled.back().Number != 0)
                {
                    ++spelled.back().Steps;
                    ++step;
                    continue;
                }

                auto const index = std::iswdigit(ch) ? static_cast<size_t>(ch - L'0')
                    : (upper >= L'A' && upper <= L'Z') ? static_cast<size_t>(upper - L'A')
                    : static_cast<size_t>(ch % 26);

                auto const number = ladder[first + index % span];
                spelled.push_back(SpelledStep{ step, 1, number, std::iswupper(ch) != 0, ch });
                lastNote = number;
                previous = upper;
                ++step;
            }
            else if (ch == L' ' || ch == L'\t')
            {
                rest(1);
            }
            else if (ch == L',' || ch == L';' || ch == L':' || ch == L'\n')
            {
                rest(2);
            }
            else if (IsEnding(ch))
            {
                // A period ends on the root, a question on the fifth, and an exclamation on the
                // root, louder.
                auto const fifth = static_cast<int32_t>((generator.ScaleRoot + 7) % 12);
                auto const number = ch == L'?' ? nearest(lastNote, ScaleDegree(generator, static_cast<uint8_t>(fifth)) >= 0 ? fifth : static_cast<int32_t>(generator.ScaleRoot % 12))
                    : nearest(lastNote, generator.ScaleRoot % 12);

                spelled.push_back(SpelledStep{ step, 2, number, ch == L'!', 0 });
                lastNote = number;
                step += 2;
                rest(1);
            }

            // Anything else, such as an apostrophe or a hyphen, takes no time.
        }

        // How hard each is spoken (VelocityShape::Speech): rising through a sentence to its end,
        // each word's first letter stressed, and capitals and "!" hardest.
        size_t sentence{ 0 };

        while (sentence < spelled.size())
        {
            auto end = sentence;

            while (end < spelled.size() && !(spelled[end].Letter == 0 && spelled[end].Number != 0))
            {
                ++end;
            }

            auto const letters = static_cast<int64_t>(std::count_if(spelled.begin() + static_cast<ptrdiff_t>(sentence), spelled.begin() + static_cast<ptrdiff_t>(end),
                [](SpelledStep const& at) { return at.Letter != 0; }));
            int64_t said{ 0 };

            for (auto i = sentence; i < end; ++i)
            {
                auto& at = spelled[i];

                if (at.Letter == 0)
                {
                    continue;
                }

                auto const rise = letters > 1 ? static_cast<double>(said) / static_cast<double>(letters - 1) : 1.0;
                auto const wordStart = i == 0 || spelled[i - 1].Letter == 0;
                at.Stress = static_cast<uint8_t>(at.Accent ? 100 : std::lround(std::min(100.0, 30.0 + 30.0 * rise + (wordStart ? 25.0 : 0.0))));
                ++said;
            }

            if (end < spelled.size())
            {
                spelled[end].Stress = spelled[end].Accent ? 100 : 80;
            }

            sentence = end + 1;
        }

        return spelled;
    }

    _Use_decl_annotations_
    int64_t SpelledLength(std::vector<SpelledStep> const& spelled) noexcept
    {
        return spelled.empty() ? 0 : spelled.back().Step + spelled.back().Steps;
    }

    _Use_decl_annotations_
    std::vector<StepNote> WordsPass(GeneratorSettings const& generator, int64_t steps, uint64_t pass)
    {
        auto const spelled = SpellWords(generator);
        auto const length = SpelledLength(spelled);

        if (length <= 0)
        {
            return {};
        }

        // Words that fit start again each pass. Longer ones carry on where the last pass stopped.
        auto const start = length <= steps ? int64_t{ 0 } : static_cast<int64_t>((pass * static_cast<uint64_t>(steps)) % static_cast<uint64_t>(length));

        std::vector<StepNote> notes{};

        for (auto const& at : spelled)
        {
            if (at.Number == 0)
            {
                continue;
            }

            auto const into = ((at.Step - start) % length + length) % length;

            if (into < steps)
            {
                notes.push_back(StepNote{ into, at.Steps, at.Number, VelocityForLoudness(static_cast<double>(at.Stress) / 100.0) });
            }
        }

        return notes;
    }
}
