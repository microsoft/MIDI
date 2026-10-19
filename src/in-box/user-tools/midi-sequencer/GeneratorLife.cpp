// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Life (design section 28): the Game of Life on a board with a column for each step and a row for
// each note of the key and range, or for each drum. Each pass is the next generation. The edges
// wrap, so a glider that walks off the end of the bar comes back in at the start.

#include "GeneratorKinds.h"
#include "SequenceGenerator.h"

#include <algorithm>
#include <cstdlib>
#include <map>
#include <mutex>

namespace midisequencer
{
    namespace
    {
        // A generation is kept every this many, so going back is quick.
        constexpr uint64_t CheckpointEvery = 64;

        // Clips with a board worked out, kept at once.
        constexpr size_t RunsKept = 8;

        using Cells = std::vector<uint8_t>;

        struct Generation
        {
            uint64_t Number{ 0 };
            Cells Board{};
            Cells Previous{};           // empty cells after a start, so every cell counts as born
            Cells BeforeThat{};
            Cells Age{};                // passes each cell has lived: 1 the pass it's born
            bool Settled{ false };
        };

        struct Run
        {
            uint64_t Key{ 0 };
            uint64_t LastUsed{ 0 };
            Cells Start{};
            Generation Latest{};
            std::vector<Generation> Checkpoints{};
        };

        struct Rule
        {
            uint16_t Born{ 0 };         // a bit for each neighbor count
            uint16_t Stays{ 0 };
        };

        Rule RuleFor(_In_ LifeRule rule) noexcept
        {
            switch (rule)
            {
            case LifeRule::HighLife:
                return Rule{ (1 << 3) | (1 << 6), (1 << 2) | (1 << 3) };

            case LifeRule::Seeds:
                return Rule{ 1 << 2, 0 };

            case LifeRule::Life:
            default:
                return Rule{ 1 << 3, (1 << 2) | (1 << 3) };
            }
        }

        bool Empty(_In_ Cells const& cells) noexcept
        {
            return std::none_of(cells.begin(), cells.end(), [](uint8_t cell) { return cell != 0; });
        }

        void Stamp(_Inout_ Cells& cells, _In_ int32_t columns, _In_ int32_t rows, _In_ std::vector<LifeCell> const& shape)
        {
            for (auto const& cell : shape)
            {
                if (cell.Column < columns && cell.Row < rows)
                {
                    cells[static_cast<size_t>(cell.Row) * static_cast<size_t>(columns) + cell.Column] = 1;
                }
            }
        }

        // A shape's cells from the top left, the way it's usually drawn, placed at a column and a
        // row and wrapped. Rows count up, so a glider drawn going down-right climbs as it goes.
        std::vector<LifeCell> Place(
            _In_ std::initializer_list<std::pair<int32_t, int32_t>> drawn,
            _In_ int32_t column,
            _In_ int32_t row,
            _In_ int32_t columns,
            _In_ int32_t rows,
            _In_ bool flip)
        {
            std::vector<LifeCell> cells{};

            for (auto const& [x, y] : drawn)
            {
                auto const across = flip ? 2 - x : x;
                cells.push_back(LifeCell{
                    static_cast<uint16_t>(((column + across) % columns + columns) % columns),
                    static_cast<uint16_t>(((row + y) % rows + rows) % rows) });
            }

            return cells;
        }

        Cells StartCells(_In_ GeneratorSettings const& generator, _In_ uint32_t seed, _In_ int32_t columns, _In_ int32_t rows)
        {
            Cells cells(static_cast<size_t>(columns) * static_cast<size_t>(rows), 0);

            if (generator.Scatter)
            {
                Stamp(cells, columns, rows, ScatteredCells(generator, seed, columns, rows));
            }
            else
            {
                Stamp(cells, columns, rows, generator.LifeCells);
            }

            return cells;
        }

        Cells Step(_In_ Cells const& cells, _In_ int32_t columns, _In_ int32_t rows, _In_ Rule rule)
        {
            Cells next(cells.size(), 0);

            for (int32_t row = 0; row < rows; ++row)
            {
                auto const up = (row + 1) % rows;
                auto const down = (row + rows - 1) % rows;

                for (int32_t column = 0; column < columns; ++column)
                {
                    auto const left = (column + columns - 1) % columns;
                    auto const right = (column + 1) % columns;

                    auto const at = [&](int32_t c, int32_t r) { return cells[static_cast<size_t>(r) * static_cast<size_t>(columns) + c] != 0 ? 1 : 0; };

                    // On a board one cell wide or tall, a neighbor can be the cell itself; it
                    // still counts once a side, as on any wrapped board.
                    auto const count =
                        at(left, up) + at(column, up) + at(right, up) +
                        at(left, row) + at(right, row) +
                        at(left, down) + at(column, down) + at(right, down);

                    auto const alive = at(column, row) != 0;
                    auto const mask = static_cast<uint16_t>(1u << count);
                    next[static_cast<size_t>(row) * static_cast<size_t>(columns) + column] = (alive ? (rule.Stays & mask) : (rule.Born & mask)) != 0 ? 1 : 0;
                }
            }

            return next;
        }

        // When a board stops changing and its rule is to add cells: a glider or an R-pentomino,
        // somewhere the seed picks.
        void AddCells(_Inout_ Cells& cells, _In_ int32_t columns, _In_ int32_t rows, _In_ uint32_t seed, _In_ uint64_t generation)
        {
            auto const column = static_cast<int32_t>(GeneratorRoll(seed, generation, 0, 51) % static_cast<uint64_t>(columns));
            auto const row = static_cast<int32_t>(GeneratorRoll(seed, generation, 1, 51) % static_cast<uint64_t>(rows));
            auto const flip = GeneratorRoll(seed, generation, 2, 51) % 2 == 0;

            if (GeneratorRoll(seed, generation, 3, 51) % 2 == 0)
            {
                Stamp(cells, columns, rows, Place({ { 1, 0 }, { 2, 1 }, { 0, 2 }, { 1, 2 }, { 2, 2 } }, column, row, columns, rows, flip));
            }
            else
            {
                Stamp(cells, columns, rows, Place({ { 1, 0 }, { 2, 0 }, { 0, 1 }, { 1, 1 }, { 1, 2 } }, column, row, columns, rows, flip));
            }
        }

        // How long each live cell has lived: a pass more than before, or 1 when it's new. A start
        // has no generation before it, so every cell in it is new.
        void CountAges(_Inout_ Generation& next, _In_opt_ Generation const* before)
        {
            next.Age.assign(next.Board.size(), 0);

            for (size_t i = 0; i < next.Board.size(); ++i)
            {
                if (next.Board[i] != 0)
                {
                    auto const lived = before != nullptr && i < before->Board.size() && before->Board[i] != 0 && i < before->Age.size() ? before->Age[i] : uint8_t{ 0 };
                    next.Age[i] = static_cast<uint8_t>(std::min(255, lived + 1));
                }
            }
        }

        Generation Advance(
            _In_ Generation const& current,
            _In_ Run const& run,
            _In_ GeneratorSettings const& generator,
            _In_ uint32_t seed,
            _In_ int32_t columns,
            _In_ int32_t rows)
        {
            Generation next{};
            next.Number = current.Number + 1;

            // Starting again every so many passes wins over everything else.
            if (generator.RestartEvery > 0 && next.Number % generator.RestartEvery == 0)
            {
                next.Board = run.Start;
                next.Previous.assign(run.Start.size(), 0);
                next.BeforeThat.assign(run.Start.size(), 0);
                CountAges(next, nullptr);
                return next;
            }

            auto board = Step(current.Board, columns, rows, RuleFor(generator.Rule));

            // Stopped changing: still, blinking between two boards, or empty.
            if (board == current.Board || board == current.Previous || Empty(board))
            {
                next.Settled = true;

                switch (generator.Settled)
                {
                case LifeSettled::StartAgain:
                    next.Board = run.Start;
                    next.Previous.assign(run.Start.size(), 0);
                    next.BeforeThat.assign(run.Start.size(), 0);
                    CountAges(next, nullptr);
                    return next;

                case LifeSettled::AddCells:
                    AddCells(board, columns, rows, seed, next.Number);
                    break;

                case LifeSettled::Hold:
                default:
                    break;
                }
            }

            next.Board = std::move(board);
            next.Previous = current.Board;
            next.BeforeThat = current.Previous;
            CountAges(next, &current);
            return next;
        }

        // The cells what was heard before a pass lands on, as places on the board.
        using Drop = std::vector<size_t>;

        Drop DropFor(_In_ GeneratorSettings const& generator, _In_ std::vector<StepNote> const& heard, _In_ int32_t columns, _In_ int32_t rows)
        {
            Drop drop{};

            for (auto const& cell : HeardCells(generator, heard, columns))
            {
                if (cell.Row < rows && cell.Column < columns)
                {
                    drop.push_back(static_cast<size_t>(cell.Row) * static_cast<size_t>(columns) + cell.Column);
                }
            }

            return drop;
        }

        // The next generation, and then what was heard before it, dropped on as new cells.
        Generation Next(
            _In_ Generation const& current,
            _In_ Run const& run,
            _In_ GeneratorSettings const& generator,
            _In_ uint32_t seed,
            _In_ int32_t columns,
            _In_ int32_t rows,
            _In_opt_ Drop const* drop)
        {
            auto next = Advance(current, run, generator, seed, columns, rows);

            if (drop != nullptr)
            {
                for (auto const index : *drop)
                {
                    if (index < next.Board.size() && next.Board[index] == 0)
                    {
                        next.Board[index] = 1;
                        next.Age.resize(next.Board.size(), 0);
                        next.Age[index] = 1;
                        next.Settled = false;
                    }
                }
            }

            return next;
        }

        int32_t RowCount(_In_ GeneratorSettings const& generator)
        {
            return static_cast<int32_t>(std::max<size_t>(1, LifeRowNotes(generator).size()));
        }

        // Passes heard at a time, with the lock let go, before stepping through them.
        constexpr uint64_t HeardAtOnce = 32;

        // Playback hears what plays now, so a pass far past what was worked out hears only this
        // many passes before it, and steps through the ones before those as they'd play alone.
        constexpr uint64_t LiveCatchUp = 64;

        // The generation on a pass, from the runs kept. One that keeps listening drops what it
        // heard before each pass onto the board. Hearing plays other clips, which in a loop can be
        // this one, so it hears with the lock let go, then steps with the lock held.
        Generation GenerationAt(_In_ GeneratorSettings const& generator, _In_ uint32_t seed, _In_ int64_t steps, _In_ uint64_t pass, _In_opt_ PassHearing const* hearing)
        {
            static std::mutex lock{};
            static std::vector<Run> runs{};
            static uint64_t clock{ 0 };

            auto const columns = static_cast<int32_t>(std::clamp<int64_t>(steps, 1, MaximumGeneratorSteps));
            auto const rows = RowCount(generator);
            auto const listening = hearing != nullptr && hearing->Before;
            auto const key = GeneratorFingerprint(generator, seed, columns) ^ (listening ? (hearing->RunKey | 1) * 0x9E3779B97F4A7C15ull : 0);

            pass = std::min(pass, MaximumSteppedPasses);

            // What was heard before each pass, from the first one it steps to.
            std::map<uint64_t, Drop> drops{};

            for (;;)
            {
                uint64_t reached{ 0 };

                {
                    std::lock_guard guard{ lock };

                    auto run = std::find_if(runs.begin(), runs.end(), [key](Run const& candidate) { return candidate.Key == key; });

                    if (run == runs.end())
                    {
                        Run made{};
                        made.Key = key;
                        made.Start = StartCells(generator, seed, columns, rows);
                        made.Latest.Board = made.Start;
                        made.Latest.Previous.assign(made.Start.size(), 0);
                        made.Latest.BeforeThat.assign(made.Start.size(), 0);
                        CountAges(made.Latest, nullptr);
                        made.Checkpoints.push_back(made.Latest);

                        if (runs.size() >= RunsKept)
                        {
                            auto oldest = std::min_element(runs.begin(), runs.end(), [](Run const& a, Run const& b) { return a.LastUsed < b.LastUsed; });
                            *oldest = std::move(made);
                            run = oldest;
                        }
                        else
                        {
                            runs.push_back(std::move(made));
                            run = runs.end() - 1;
                        }
                    }

                    run->LastUsed = ++clock;

                    // From the latest generation, or the last one kept before the pass.
                    Generation current = pass >= run->Latest.Number
                        ? run->Latest
                        : run->Checkpoints[std::min<size_t>(static_cast<size_t>(pass / CheckpointEvery), run->Checkpoints.size() - 1)];

                    auto heardAll = true;

                    while (current.Number < pass)
                    {
                        Drop const* drop{ nullptr };

                        if (listening)
                        {
                            auto const found = drops.find(current.Number + 1);

                            if (found == drops.end())
                            {
                                heardAll = false;
                                break;
                            }

                            drop = &found->second;
                        }

                        current = Next(current, *run, generator, seed, columns, rows, drop);

                        if (current.Number % CheckpointEvery == 0 && current.Number / CheckpointEvery == run->Checkpoints.size())
                        {
                            run->Checkpoints.push_back(current);
                        }
                    }

                    if (current.Number >= run->Latest.Number)
                    {
                        run->Latest = current;
                    }

                    if (heardAll)
                    {
                        return current;
                    }

                    reached = current.Number;
                }

                // What was heard before the next few passes, with the lock let go. Each time round
                // hears at least one pass more, so this ends.
                for (auto next = reached + 1; next <= std::min(pass, reached + HeardAtOnce); ++next)
                {
                    if (!drops.contains(next))
                    {
                        auto const skipped = hearing->Live && pass > next + LiveCatchUp;
                        drops.emplace(next, skipped ? Drop{} : DropFor(generator, hearing->Before(next), columns, rows));
                    }
                }
            }
        }
    }

    std::vector<uint8_t> DefaultDrumRows()
    {
        return { 36, 38, 39, 42, 46, 45, 50, 51 };
    }

    _Use_decl_annotations_
    std::vector<uint8_t> LifeRowNotes(GeneratorSettings const& generator)
    {
        if (generator.Type == GeneratorType::LifeDrums)
        {
            return generator.DrumRows.empty() ? DefaultDrumRows() : generator.DrumRows;
        }

        auto notes = ScaleNotes(generator.ScaleRoot, generator.Scale, generator.LowNote, generator.HighNote);

        if (notes.size() > MaximumLifeRows)
        {
            notes.resize(MaximumLifeRows);
        }

        return notes;
    }

    _Use_decl_annotations_
    std::vector<LifeCell> ScatteredCells(GeneratorSettings const& generator, uint32_t seed, int32_t columns, int32_t rows)
    {
        std::vector<LifeCell> cells{};

        for (int32_t row = 0; row < rows; ++row)
        {
            for (int32_t column = 0; column < columns; ++column)
            {
                auto const index = static_cast<uint64_t>(row) * static_cast<uint64_t>(columns) + static_cast<uint64_t>(column);

                if (GeneratorRoll(seed, 0, index, 41) % 100 < generator.Density)
                {
                    cells.push_back(LifeCell{ static_cast<uint16_t>(column), static_cast<uint16_t>(row) });
                }
            }
        }

        // A board too small to get any gets an R-pentomino, so it always starts with something.
        if (cells.empty())
        {
            cells = LifeShapeCells(LifeShape::RPentomino, columns, rows);
        }

        return cells;
    }

    _Use_decl_annotations_
    std::vector<LifeCell> LifeShapeCells(LifeShape shape, int32_t columns, int32_t rows)
    {
        columns = std::max(1, columns);
        rows = std::max(1, rows);

        auto const column = columns / 2 - 1;
        auto const row = rows / 2 - 1;

        std::vector<LifeCell> cells{};

        switch (shape)
        {
        case LifeShape::Blinker:
            cells = Place({ { 0, 1 }, { 1, 1 }, { 2, 1 } }, column, row, columns, rows, false);
            break;

        case LifeShape::RPentomino:
            cells = Place({ { 1, 0 }, { 2, 0 }, { 0, 1 }, { 1, 1 }, { 1, 2 } }, column, row, columns, rows, false);
            break;

        case LifeShape::Glider:
        default:
            cells = Place({ { 1, 0 }, { 2, 1 }, { 0, 2 }, { 1, 2 }, { 2, 2 } }, column, row, columns, rows, false);
            break;
        }

        std::sort(cells.begin(), cells.end());
        cells.erase(std::unique(cells.begin(), cells.end()), cells.end());
        return cells;
    }

    _Use_decl_annotations_
    std::vector<LifeCell> HeardCells(GeneratorSettings const& generator, std::vector<StepNote> const& heard, int32_t columns)
    {
        columns = std::max(1, columns);

        auto const notes = LifeRowNotes(generator);
        std::vector<LifeCell> cells{};

        if (notes.empty())
        {
            return cells;
        }

        auto const lowest = static_cast<int32_t>(*std::min_element(notes.begin(), notes.end()));
        auto const highest = static_cast<int32_t>(*std::max_element(notes.begin(), notes.end()));

        for (auto const& note : heard)
        {
            // An octave at a time into the rows' range, so a bass line still lands on a melodic
            // board, then the row whose note it is, or the nearest: the lower on a tie.
            auto number = static_cast<int32_t>(note.Number);

            while (number > highest && number - 12 >= lowest)
            {
                number -= 12;
            }

            while (number < lowest && number + 12 <= highest)
            {
                number += 12;
            }

            size_t row{ 0 };
            auto nearest = INT32_MAX;

            for (size_t i = 0; i < notes.size(); ++i)
            {
                auto const distance = std::abs(static_cast<int32_t>(notes[i]) - number);

                if (distance < nearest || (distance == nearest && notes[i] < notes[row]))
                {
                    nearest = distance;
                    row = i;
                }
            }

            auto const column = static_cast<int32_t>(((note.Step % columns) + columns) % columns);
            cells.push_back(LifeCell{ static_cast<uint16_t>(column), static_cast<uint16_t>(row) });
        }

        std::sort(cells.begin(), cells.end());
        cells.erase(std::unique(cells.begin(), cells.end()), cells.end());
        return cells;
    }

    _Use_decl_annotations_
    LifeBoard LifeBoardAt(GeneratorSettings const& generator, uint32_t seed, int64_t steps, uint64_t pass, PassHearing const* hearing)
    {
        auto const generation = GenerationAt(generator, seed, steps, generator.ChangesEachPass ? pass : 0, hearing);

        LifeBoard board{};
        board.Columns = static_cast<int32_t>(std::clamp<int64_t>(steps, 1, MaximumGeneratorSteps));
        board.Rows = RowCount(generator);
        board.Settled = generation.Settled;
        board.Cells.assign(generation.Board.size(), 0);

        for (size_t i = 0; i < generation.Board.size(); ++i)
        {
            if (generation.Board[i] != 0)
            {
                board.Cells[i] = i < generation.Previous.size() && generation.Previous[i] != 0 ? uint8_t{ 1 } : uint8_t{ 2 };
            }
        }

        board.Ages = generation.Age;
        board.Ages.resize(generation.Board.size(), 0);
        return board;
    }

    _Use_decl_annotations_
    std::vector<StepNote> LifePass(GeneratorSettings const& generator, uint32_t seed, int64_t steps, uint64_t pass, PassHearing const* hearing)
    {
        auto const board = LifeBoardAt(generator, seed, steps, pass, hearing);
        auto const notes = LifeRowNotes(generator);
        auto const drums = generator.Type == GeneratorType::LifeDrums;

        // How loud a cell plays: by how many live neighbors it has, across the wrapped edges, or
        // by its age, a new cell loudest and an old one, on drums, a ghost note.
        auto const loudness = [&board, &generator](int32_t column, int32_t row)
        {
            if (generator.VelocityBy == VelocityShape::Crowding)
            {
                int32_t neighbors{ 0 };

                for (int32_t down = -1; down <= 1; ++down)
                {
                    for (int32_t across = -1; across <= 1; ++across)
                    {
                        if ((down != 0 || across != 0) &&
                            board.At((column + across + board.Columns) % board.Columns, (row + down + board.Rows) % board.Rows) != 0)
                        {
                            ++neighbors;
                        }
                    }
                }

                return std::min(1.0, static_cast<double>(neighbors) / 4.0);
            }

            static constexpr std::array<double, 4> ByAge{ 1.0, 0.6, 0.35, 0.1 };
            auto const index = static_cast<size_t>(row) * static_cast<size_t>(board.Columns) + static_cast<size_t>(column);
            auto const age = index < board.Ages.size() ? std::max<uint8_t>(1, board.Ages[index]) : uint8_t{ 1 };
            return ByAge[std::min<size_t>(static_cast<size_t>(age) - 1, ByAge.size() - 1)];
        };

        std::vector<StepNote> played{};

        // A note still sounding on each row, which the next column can carry on.
        std::vector<size_t> sounding(static_cast<size_t>(board.Rows), SIZE_MAX);

        for (int32_t column = 0; column < board.Columns; ++column)
        {
            std::vector<int32_t> chosen{};

            for (int32_t row = 0; row < board.Rows; ++row)
            {
                auto const cell = board.At(column, row);

                if (cell != 0 && (generator.Births != LifeBirths::Only || cell == 2))
                {
                    chosen.push_back(row);
                }
            }

            if (!drums && !chosen.empty())
            {
                if (generator.Voice == LifeVoice::Lowest)
                {
                    chosen = { chosen.front() };
                }
                else if (generator.Voice == LifeVoice::Highest)
                {
                    chosen = { chosen.back() };
                }
                else if (chosen.size() > 8)
                {
                    chosen.resize(8);
                }
            }

            std::vector<size_t> carried(static_cast<size_t>(board.Rows), SIZE_MAX);

            for (auto const row : chosen)
            {
                auto const index = static_cast<size_t>(row);

                // Side by side in a row: one longer note.
                if (!drums && generator.Join && sounding[index] != SIZE_MAX)
                {
                    ++played[sounding[index]].Steps;
                    carried[index] = sounding[index];
                    continue;
                }

                StepNote note{};
                note.Step = column;
                note.Steps = 1;
                note.Number = notes[std::min(index, notes.size() - 1)];
                note.Velocity = VelocityForLoudness(loudness(column, row));

                carried[index] = played.size();
                played.push_back(note);
            }

            sounding = std::move(carried);
        }

        return played;
    }
}
