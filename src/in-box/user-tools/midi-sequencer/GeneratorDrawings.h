// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// The drawings of design section 28's generators: the key and range as a keyboard, a Life board
// and the passes it grows through, the moves Learn learned, spelled words, Evolve's copies and
// scores, the infinity series, an L-system's turtle, change ringing's blue line, and how loud each
// note plays. Win2D, from data the window hands over, like GeneratorView.

#include "GeneratorKinds.h"
#include "ThemePalette.h"

namespace midisequencer
{
    // ---- the key and range ----

    struct KeyboardSpan
    {
        uint8_t First{ 36 };
        uint8_t Last{ 107 };
    };

    // Whole octaves, from below the range to above it.
    KeyboardSpan MeasureKeyboard(_In_ uint8_t low, _In_ uint8_t high) noexcept;

    // The key under a point, or -1.
    int32_t KeyAtPoint(_In_ KeyboardSpan span, _In_ float width, _In_ float height, _In_ float x, _In_ float y) noexcept;

    // The keys, with the range lit, its notes of the key brighter, and the roots marked. Laid out
    // for the range, or as `layout` says: a range being dragged keeps the keys where they were.
    void DrawKeyboard(
        _In_ canvas::CanvasDrawingSession const& ds,
        _In_ float width,
        _In_ float height,
        _In_ uint8_t root,
        _In_ ScaleKind scale,
        _In_ uint8_t low,
        _In_ uint8_t high,
        _In_ Color color,
        _In_ Palette const& colors,
        _In_opt_ KeyboardSpan const* layout = nullptr);

    // ---- Life ----

    struct BoardLayout
    {
        float Left{ 0 };                // where the cells start, after the row names
        float Cell{ 12 };
        int32_t Columns{ 16 };
        int32_t Rows{ 8 };

        float Width() const noexcept { return Left + Cell * static_cast<float>(Columns); }
        float Height() const noexcept { return Cell * static_cast<float>(Rows); }
    };

    BoardLayout MeasureLifeBoard(_In_ float width, _In_ float height, _In_ int32_t columns, _In_ int32_t rows, _In_ float labelWidth) noexcept;

    // The cell under a point. False when it's on none.
    bool CellAtPoint(_In_ BoardLayout const& layout, _In_ float x, _In_ float y, _Out_ int32_t& column, _Out_ int32_t& row) noexcept;

    // Live cells in the clip's color, those born this pass brighter. Each row is named down the
    // left, lowest at the bottom, and the playing step is lit.
    void DrawLifeBoard(
        _In_ canvas::CanvasDrawingSession const& ds,
        _In_ LifeBoard const& board,
        _In_ BoardLayout const& layout,
        _In_ std::vector<std::wstring> const& names,
        _In_ std::vector<bool> const& roots,
        _In_ int64_t playingStep,
        _In_ Color color,
        _In_ Palette const& colors);

    struct FilmLayout
    {
        uint64_t First{ 0 };
        size_t Count{ 0 };
        float Frame{ 20 };              // one board's width
        float Gap{ 6 };
    };

    FilmLayout MeasureFilmstrip(_In_ float width, _In_ float height, _In_ int32_t columns, _In_ int32_t rows, _In_ uint64_t around) noexcept;

    // The pass a point is on in the filmstrip, or UINT64_MAX.
    uint64_t FilmPassAtPoint(_In_ FilmLayout const& layout, _In_ float x) noexcept;

    // Small boards for a run of passes, numbered, the one shown framed and the settled ones dotted.
    void DrawFilmstrip(
        _In_ canvas::CanvasDrawingSession const& ds,
        _In_ float height,
        _In_ std::vector<LifeBoard> const& boards,
        _In_ FilmLayout const& layout,
        _In_ uint64_t shown,
        _In_ uint64_t playing,
        _In_ Color color,
        _In_ Palette const& colors);

    // ---- Learn ----

    // The scale's notes round a circle, the root at the top, and a line for each move it learned,
    // thicker the more often it made it. The moves of the pass shown are lit, and so is the note
    // that's playing.
    void DrawLearnedMoves(
        _In_ canvas::CanvasDrawingSession const& ds,
        _In_ float width,
        _In_ float height,
        _In_ std::vector<std::vector<uint32_t>> const& moves,
        _In_ std::vector<std::wstring> const& names,
        _In_ std::vector<int32_t> const& walk,
        _In_ int32_t playing,
        _In_ Color color,
        _In_ Palette const& colors);

    // ---- Words ----

    // Each note with its letter on it, from step `from` for `count` steps.
    void DrawSpelling(
        _In_ canvas::CanvasDrawingSession const& ds,
        _In_ float width,
        _In_ float height,
        _In_ std::vector<SpelledStep> const& spelled,
        _In_ int64_t from,
        _In_ int64_t count,
        _In_ uint8_t low,
        _In_ uint8_t high,
        _In_ int64_t playing,
        _In_ Color color,
        _In_ Palette const& colors);

    // ---- Evolve ----

    // The pass before on top, and the eight copies made from it below, the one chosen framed, each
    // with a bar for its score.
    void DrawFamily(
        _In_ canvas::CanvasDrawingSession const& ds,
        _In_ float width,
        _In_ float height,
        _In_ EvolveFamily const& family,
        _In_ int64_t steps,
        _In_ uint8_t low,
        _In_ uint8_t high,
        _In_ Color color,
        _In_ Palette const& colors);

    // Each pass's score as a line, with the pass shown marked.
    void DrawScores(
        _In_ canvas::CanvasDrawingSession const& ds,
        _In_ float width,
        _In_ float height,
        _In_ std::vector<double> const& scores,
        _In_ uint64_t shown,
        _In_ Color color,
        _In_ Palette const& colors);

    // ---- the infinity series ----

    // A dot for each value of the series from `first`, every fourth one (where the series hides
    // itself) in the clip's color, and the pass's own values joined up.
    void DrawSeries(
        _In_ canvas::CanvasDrawingSession const& ds,
        _In_ float width,
        _In_ float height,
        _In_ uint64_t first,
        _In_ uint64_t count,
        _In_ uint64_t passFirst,
        _In_ int64_t steps,
        _In_ int64_t playing,
        _In_ Color color,
        _In_ Palette const& colors);

    // ---- L-systems ----

    // The whole drawing, faint, fitted to the space, with the pass's own lines in the clip's color.
    void DrawTurtle(
        _In_ canvas::CanvasDrawingSession const& ds,
        _In_ float width,
        _In_ float height,
        _In_ TurtleDrawing const& drawing,
        _In_ int64_t passFirst,
        _In_ int64_t steps,
        _In_ int64_t playing,
        _In_ Color color,
        _In_ Palette const& colors);

    // ---- change ringing ----

    // Rows of bell numbers, with the treble's path in red and the second's in blue, the way
    // ringers draw a method. The playing row and bell are lit.
    void DrawBlueLine(
        _In_ canvas::CanvasDrawingSession const& ds,
        _In_ float width,
        _In_ float height,
        _In_ std::vector<std::vector<uint8_t>> const& course,
        _In_ size_t firstRow,
        _In_ size_t rowCount,
        _In_ size_t playingRow,
        _In_ int32_t playingPlace,
        _In_ bool gaps,
        _In_ Color color,
        _In_ Palette const& colors);

    // ---- velocity ----

    // The room on the right of the velocity drawing for the band's grips.
    inline constexpr float VelocityGripRoom = 24.0f;

    // Where a velocity sits in the velocity drawing, 0 at the bottom and 65535 at the top, and the
    // velocity at a height.
    float VelocityY(_In_ float height, _In_ uint16_t velocity) noexcept;
    uint16_t VelocityAtY(_In_ float height, _In_ float y) noexcept;

    // A stem for each note of the pass, as tall as the note is loud, over a band for the range
    // they play in, with a grip on each edge of the band. Each beat is marked, and the notes
    // sounding at the playing step are lit.
    void DrawVelocities(
        _In_ canvas::CanvasDrawingSession const& ds,
        _In_ float width,
        _In_ float height,
        _In_ std::vector<StepNote> const& notes,
        _In_ int64_t steps,
        _In_ int64_t stepTicks,
        _In_ uint16_t low,
        _In_ uint16_t high,
        _In_ int64_t playing,
        _In_ Color color,
        _In_ Palette const& colors);
}
