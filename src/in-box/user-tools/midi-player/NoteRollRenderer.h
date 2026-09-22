// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "midi_file_sequence.h"

namespace midiplayer
{
    // A note is drawn as two visuals: an outer body which supplies the border, and an inner fill
    // in the track color. Without a border, two notes which touch - a pair of thirty-second notes,
    // say - are indistinguishable from one note of twice the length.
    struct NoteVisuals
    {
        winrt::Microsoft::UI::Composition::SpriteVisual Body{ nullptr };
        winrt::Microsoft::UI::Composition::SpriteVisual Fill{ nullptr };
    };

    // Shared by both views, so a note is drawn the same way in each.
    winrt::Windows::UI::Color NoteBorderColor(_In_ winrt::Windows::UI::Color const& fill) noexcept;

    NoteVisuals CreateNoteVisuals(
        _In_ winrt::Microsoft::UI::Composition::Compositor const& compositor,
        _In_ winrt::Microsoft::UI::Composition::ContainerVisual const& layer) noexcept;

    void SizeNoteVisuals(_In_ NoteVisuals const& note, float left, float top, float width, float height) noexcept;

    // The scrolling note display, drawn with composition visuals rather than XAML elements.
    //
    // A file can hold millions of notes, so nothing is created per note. A fixed pool of sprite
    // visuals is positioned each frame to cover only the notes inside the visible time window,
    // which makes the cost of a frame depend on how much music is on screen and not at all on how
    // long the file is.
    class NoteRollRenderer
    {
    public:
        void Initialize(_In_ winrt::Microsoft::UI::Xaml::UIElement const& host) noexcept;
        void Shutdown() noexcept;

        void SetSequence(_In_ std::shared_ptr<midifile::MidiSequence const> const& sequence) noexcept;

        // Tracks which are silent are drawn dimmed rather than hidden, so the shape of the music
        // stays recognizable while a track is muted.
        void SetAudibleTracks(_In_ std::vector<bool> const& audible) noexcept;

        void Render(uint64_t positionMicroseconds, double width, double height) noexcept;

        // Colors are assigned per track, matching the track list.
        static winrt::Windows::UI::Color TrackColor(uint16_t trackIndex) noexcept;

        // How much music is on screen, and where the playhead sits within it.
        static constexpr double SecondsBehind = 1.5;
        static constexpr double SecondsAhead = 5.5;

    private:
        NoteVisuals TakeVisual(size_t index) noexcept;
        void HideFrom(size_t index) noexcept;

        // state packs the track, whether the track is audible and whether the note has already
        // been played; border selects the darker shade drawn around the note.
        winrt::Microsoft::UI::Composition::CompositionColorBrush NoteBrush(uint32_t state, bool border) noexcept;

        // Bar and beat lines, so the roll can be read as music rather than as a stripe chart.
        void RenderGrid(
            uint32_t startTick,
            uint32_t endTick,
            double windowStart,
            double spanSeconds,
            double width,
            double height) noexcept;

        winrt::Microsoft::UI::Composition::SpriteVisual TakeGridVisual(size_t index) noexcept;
        void HideGridFrom(size_t index) noexcept;

        winrt::Microsoft::UI::Composition::Compositor m_compositor{ nullptr };
        winrt::Microsoft::UI::Composition::ContainerVisual m_root{ nullptr };
        winrt::Microsoft::UI::Composition::ContainerVisual m_gridLayer{ nullptr };
        winrt::Microsoft::UI::Composition::ContainerVisual m_noteLayer{ nullptr };
        winrt::Microsoft::UI::Composition::SpriteVisual m_playhead{ nullptr };

        std::vector<NoteVisuals> m_pool{};
        std::vector<winrt::Microsoft::UI::Composition::SpriteVisual> m_gridPool{};

        winrt::Microsoft::UI::Composition::CompositionColorBrush m_barBrush{ nullptr };
        winrt::Microsoft::UI::Composition::CompositionColorBrush m_beatBrush{ nullptr };

        // One brush per track color, dim state and border, because a brush per note would defeat
        // the point of the pool.
        std::map<uint32_t, winrt::Microsoft::UI::Composition::CompositionColorBrush> m_brushes{};

        std::shared_ptr<midifile::MidiSequence const> m_sequence{};
        std::vector<bool> m_audible{};

        // Reused every frame so the grid does not allocate.
        std::vector<midifile::GridLine> m_gridLines{};

        // A note display which rescales as the range grows would never sit still, so the range is
        // decided once when the file is loaded.
        uint8_t m_lowestNote{ 0 };
        uint8_t m_highestNote{ 127 };

        static constexpr size_t MaximumVisibleNotes = 2048;

        // A dense time signature or a very high division could otherwise ask for a line per pixel.
        static constexpr size_t MaximumGridLines = 256;

        // Below this spacing the beats are drawn as a smear, so only bar lines are kept.
        static constexpr double MinimumBeatSpacing = 9.0;
    };
}
