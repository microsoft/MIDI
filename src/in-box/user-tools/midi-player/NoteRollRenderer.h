// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "MidiSequence.h"

namespace midiplayer
{
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
        winrt::Microsoft::UI::Composition::SpriteVisual TakeVisual(size_t index) noexcept;
        void HideFrom(size_t index) noexcept;

        winrt::Microsoft::UI::Composition::Compositor m_compositor{ nullptr };
        winrt::Microsoft::UI::Composition::ContainerVisual m_root{ nullptr };
        winrt::Microsoft::UI::Composition::ContainerVisual m_noteLayer{ nullptr };
        winrt::Microsoft::UI::Composition::SpriteVisual m_playhead{ nullptr };

        std::vector<winrt::Microsoft::UI::Composition::SpriteVisual> m_pool{};

        // One brush per track color and dim state, because a brush per note would defeat the
        // point of the pool.
        std::map<uint32_t, winrt::Microsoft::UI::Composition::CompositionColorBrush> m_brushes{};

        std::shared_ptr<midifile::MidiSequence const> m_sequence{};
        std::vector<bool> m_audible{};

        // A note display which rescales as the range grows would never sit still, so the range is
        // decided once when the file is loaded.
        uint8_t m_lowestNote{ 0 };
        uint8_t m_highestNote{ 127 };

        static constexpr size_t MaximumVisibleNotes = 2048;
    };
}
