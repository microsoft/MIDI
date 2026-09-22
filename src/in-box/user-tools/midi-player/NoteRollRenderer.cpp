// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "NoteRollRenderer.h"

namespace composition = winrt::Microsoft::UI::Composition;

namespace midiplayer
{
    namespace
    {
        constexpr double MicrosecondsPerSecond = 1000000.0;

        // Distinct hues that stay legible on both a light and a dark background. Tracks beyond
        // the end of the table wrap around, which is fine: a file with more than sixteen tracks
        // is being read as a shape, not as sixteen identifiable lines.
        constexpr uint32_t TrackPalette[]
        {
            0xFF4FC3F7, 0xFF81C784, 0xFFFFB74D, 0xFFE57373,
            0xFFBA68C8, 0xFF4DD0E1, 0xFFAED581, 0xFFFFD54F,
            0xFFF06292, 0xFF9575CD, 0xFF4DB6AC, 0xFFDCE775,
            0xFFFFA726, 0xFF7986CB, 0xFF64B5F6, 0xFFA1887F
        };

        constexpr float PlayheadFraction = 0.22f;

        // Thin enough not to eat a small note, thick enough that two touching notes show a clear
        // two pixel division between them.
        constexpr float NoteBorderThickness = 1.0f;

        // A border on each side plus at least two pixels of fill. Below this the note is drawn
        // without a border on that axis, because a dark sliver is worse than a missing edge.
        constexpr float MinimumBorderedSize = NoteBorderThickness * 2.0f + 2.0f;

        // A note shorter than this would otherwise be invisible.
        constexpr double MinimumNoteWidth = 2.0;

        winrt::Windows::UI::Color ColorFromArgb(uint32_t argb) noexcept
        {
            return winrt::Windows::UI::Color
            {
                static_cast<uint8_t>((argb >> 24) & 0xFF),
                static_cast<uint8_t>((argb >> 16) & 0xFF),
                static_cast<uint8_t>((argb >> 8) & 0xFF),
                static_cast<uint8_t>(argb & 0xFF)
            };
        }
    }

    _Use_decl_annotations_
    winrt::Windows::UI::Color NoteBorderColor(winrt::Windows::UI::Color const& fill) noexcept
    {
        // Dark enough to read as an edge where two notes meet, light enough to still be the same
        // track color.
        constexpr double Shade = 0.55;

        return winrt::Windows::UI::Color
        {
            fill.A,
            static_cast<uint8_t>(fill.R * Shade),
            static_cast<uint8_t>(fill.G * Shade),
            static_cast<uint8_t>(fill.B * Shade)
        };
    }

    _Use_decl_annotations_
    NoteVisuals CreateNoteVisuals(
        composition::Compositor const& compositor,
        composition::ContainerVisual const& layer) noexcept
    {
        NoteVisuals note{};

        note.Body = compositor.CreateSpriteVisual();
        note.Fill = compositor.CreateSpriteVisual();

        note.Body.Children().InsertAtTop(note.Fill);
        layer.Children().InsertAtTop(note.Body);

        return note;
    }

    _Use_decl_annotations_
    void SizeNoteVisuals(NoteVisuals const& note, float left, float top, float width, float height) noexcept
    {
        note.Body.Offset({ left, top, 0.0f });
        note.Body.Size({ width, height });

        auto const insetX = width >= MinimumBorderedSize ? NoteBorderThickness : 0.0f;
        auto const insetY = height >= MinimumBorderedSize ? NoteBorderThickness : 0.0f;

        note.Fill.Offset({ insetX, insetY, 0.0f });
        note.Fill.Size({ width - insetX * 2.0f, height - insetY * 2.0f });
    }

    _Use_decl_annotations_
    winrt::Windows::UI::Color NoteRollRenderer::TrackColor(uint16_t trackIndex) noexcept
    {
        return ColorFromArgb(TrackPalette[trackIndex % ARRAYSIZE(TrackPalette)]);
    }

    _Use_decl_annotations_
    void NoteRollRenderer::Initialize(winrt::Microsoft::UI::Xaml::UIElement const& host) noexcept
    {
        try
        {
            auto const hostVisual = xaml::Hosting::ElementCompositionPreview::GetElementVisual(host);

            if (hostVisual == nullptr)
            {
                return;
            }

            m_compositor = hostVisual.Compositor();

            m_root = m_compositor.CreateContainerVisual();
            m_gridLayer = m_compositor.CreateContainerVisual();
            m_noteLayer = m_compositor.CreateContainerVisual();

            // The grid goes in first so the notes always draw over it.
            m_root.Children().InsertAtTop(m_gridLayer);
            m_root.Children().InsertAtTop(m_noteLayer);

            m_barBrush = m_compositor.CreateColorBrush(ColorFromArgb(0x38FFFFFF));
            m_beatBrush = m_compositor.CreateColorBrush(ColorFromArgb(0x18FFFFFF));

            m_playhead = m_compositor.CreateSpriteVisual();
            m_playhead.Brush(m_compositor.CreateColorBrush(ColorFromArgb(0xFFFF7043)));

            m_root.Children().InsertAtTop(m_playhead);

            xaml::Hosting::ElementCompositionPreview::SetElementChildVisual(host, m_root);
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to set up the note display.")
    }

    void NoteRollRenderer::Shutdown() noexcept
    {
        try
        {
            m_pool.clear();
            m_gridPool.clear();
            m_brushes.clear();

            m_barBrush = nullptr;
            m_beatBrush = nullptr;
            m_playhead = nullptr;
            m_noteLayer = nullptr;
            m_gridLayer = nullptr;
            m_root = nullptr;
            m_compositor = nullptr;

            m_sequence.reset();
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to tear down the note display.")
    }

    _Use_decl_annotations_
    void NoteRollRenderer::SetSequence(std::shared_ptr<midifile::MidiSequence const> const& sequence) noexcept
    {
        m_sequence = sequence;
        m_audible.clear();

        if (m_sequence == nullptr || m_sequence->Notes.empty())
        {
            m_lowestNote = 0;
            m_highestNote = 127;
        }
        else
        {
            // A little air above and below, so the highest and lowest notes are not drawn hard
            // against the edge.
            auto const low = static_cast<int32_t>(m_sequence->LowestNote) - 2;
            auto const high = static_cast<int32_t>(m_sequence->HighestNote) + 2;

            m_lowestNote = static_cast<uint8_t>(low < 0 ? 0 : low);
            m_highestNote = static_cast<uint8_t>(high > 127 ? 127 : high);

            // A file playing three notes would otherwise draw them as three enormous bars.
            if (m_highestNote - m_lowestNote < 24)
            {
                auto const center = (m_highestNote + m_lowestNote) / 2;
                auto const lowered = center - 12;
                auto const raised = center + 12;

                m_lowestNote = static_cast<uint8_t>(lowered < 0 ? 0 : lowered);
                m_highestNote = static_cast<uint8_t>(raised > 127 ? 127 : raised);
            }
        }

        HideFrom(0);
        HideGridFrom(0);
    }

    _Use_decl_annotations_
    void NoteRollRenderer::SetAudibleTracks(std::vector<bool> const& audible) noexcept
    {
        m_audible = audible;
    }

    _Use_decl_annotations_
    void NoteRollRenderer::Render(uint64_t positionMicroseconds, double width, double height) noexcept
    {
        try
        {
            if (m_root == nullptr || width <= 0.0 || height <= 0.0)
            {
                return;
            }

            m_root.Size({ static_cast<float>(width), static_cast<float>(height) });

            auto const playheadX = static_cast<float>(width * PlayheadFraction);

            m_playhead.Offset({ playheadX, 0.0f, 0.0f });
            m_playhead.Size({ 2.0f, static_cast<float>(height) });

            if (m_sequence == nullptr || m_sequence->Notes.empty())
            {
                HideFrom(0);
                HideGridFrom(0);
                return;
            }

            auto const spanSeconds = SecondsBehind + SecondsAhead;

            auto const position = static_cast<double>(positionMicroseconds) / MicrosecondsPerSecond;
            auto const windowStart = position - SecondsBehind;
            auto const windowEnd = position + SecondsAhead;

            auto const startTick = m_sequence->TickAtMicroseconds(
                windowStart <= 0.0 ? 0 : static_cast<uint64_t>(windowStart * MicrosecondsPerSecond));

            auto const endTick = m_sequence->TickAtMicroseconds(
                static_cast<uint64_t>((windowEnd < 0.0 ? 0.0 : windowEnd) * MicrosecondsPerSecond));

            RenderGrid(startTick, endTick, windowStart, spanSeconds, width, height);

            // Back the search up far enough to catch a long note which started before the window.
            auto const searchTick = startTick > m_sequence->LongestNoteTicks
                ? startTick - m_sequence->LongestNoteTicks
                : 0u;

            auto const first = std::lower_bound(
                m_sequence->Notes.begin(),
                m_sequence->Notes.end(),
                searchTick,
                [](midifile::Note const& note, uint32_t value) noexcept { return note.StartTick < value; });

            auto const noteCount = static_cast<double>(m_highestNote - m_lowestNote + 1);
            auto const noteHeight = height / noteCount;
            auto const drawHeight = static_cast<float>(noteHeight > 3.0 ? noteHeight - 1.0 : noteHeight);

            size_t used = 0;

            for (auto entry = first; entry != m_sequence->Notes.end(); ++entry)
            {
                if (entry->StartTick > endTick)
                {
                    break;
                }

                if (entry->EndTick < startTick ||
                    entry->NoteNumber < m_lowestNote ||
                    entry->NoteNumber > m_highestNote)
                {
                    continue;
                }

                if (used >= MaximumVisibleNotes)
                {
                    break;
                }

                auto const noteStart = static_cast<double>(m_sequence->MicrosecondsAtTick(entry->StartTick))
                    / MicrosecondsPerSecond;

                auto const noteEnd = static_cast<double>(m_sequence->MicrosecondsAtTick(entry->EndTick))
                    / MicrosecondsPerSecond;

                auto const left = ((noteStart - windowStart) / spanSeconds) * width;
                auto const right = ((noteEnd - windowStart) / spanSeconds) * width;

                auto const clampedLeft = left < 0.0 ? 0.0 : left;
                auto const clampedRight = right > width ? width : right;

                if (clampedRight <= clampedLeft && right < 0.0)
                {
                    continue;
                }

                auto const barWidth = clampedRight - clampedLeft;

                auto const note = TakeVisual(used);

                if (note.Body == nullptr)
                {
                    break;
                }

                auto const top = height - ((entry->NoteNumber - m_lowestNote + 1) * noteHeight);

                SizeNoteVisuals(
                    note,
                    static_cast<float>(clampedLeft),
                    static_cast<float>(top),
                    static_cast<float>(barWidth < MinimumNoteWidth ? MinimumNoteWidth : barWidth),
                    drawHeight);

                auto const audible = entry->TrackIndex >= m_audible.size() || m_audible[entry->TrackIndex];

                // A note that has already been played is dimmed, so the playhead reads as a
                // boundary between what was heard and what is coming.
                auto const played = noteStart <= position;

                uint32_t const state = (static_cast<uint32_t>(entry->TrackIndex % ARRAYSIZE(TrackPalette)) << 2)
                    | (audible ? 0x2u : 0x0u)
                    | (played ? 0x1u : 0x0u);

                auto const fill = NoteBrush(state, false);
                auto const border = NoteBrush(state, true);

                if (fill == nullptr || border == nullptr)
                {
                    break;
                }

                note.Fill.Brush(fill);
                note.Body.Brush(border);
                note.Body.IsVisible(true);

                ++used;
            }

            HideFrom(used);
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to draw the note display.")
    }

    _Use_decl_annotations_
    void NoteRollRenderer::RenderGrid(
        uint32_t startTick,
        uint32_t endTick,
        double windowStart,
        double spanSeconds,
        double width,
        double height) noexcept
    {
        size_t used = 0;

        try
        {
            if (m_sequence == nullptr || m_gridLayer == nullptr || endTick <= startTick)
            {
                HideGridFrom(0);
                return;
            }

            // Decide bar-only or bar-and-beat once for the whole frame, from the signature at the
            // left edge. Switching partway across the window would look like a rendering fault.
            auto const& leading = m_sequence->TimeSignatureAtTick(startTick);

            if (leading.TicksPerBar == 0 || leading.Numerator == 0)
            {
                HideGridFrom(0);
                return;
            }

            auto const windowTicks = static_cast<double>(endTick - startTick);
            auto const leadingBeatTicks = static_cast<double>(leading.TicksPerBar) / leading.Numerator;
            auto const beatSpacing = (leadingBeatTicks / windowTicks) * width;

            m_sequence->CollectGridLines(
                startTick,
                endTick,
                beatSpacing >= MinimumBeatSpacing,
                MaximumGridLines,
                m_gridLines);

            for (auto const& line : m_gridLines)
            {
                auto const seconds = static_cast<double>(m_sequence->MicrosecondsAtTick(line.Tick))
                    / MicrosecondsPerSecond;

                auto const x = ((seconds - windowStart) / spanSeconds) * width;

                if (x < 0.0 || x > width)
                {
                    continue;
                }

                auto visual = TakeGridVisual(used);

                if (visual == nullptr)
                {
                    break;
                }

                visual.Offset({ static_cast<float>(x), 0.0f, 0.0f });
                visual.Size({ line.IsBar ? 1.5f : 1.0f, static_cast<float>(height) });
                visual.Brush(line.IsBar ? m_barBrush : m_beatBrush);
                visual.IsVisible(true);

                ++used;
            }
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to draw the bar lines.")

        HideGridFrom(used);
    }

    _Use_decl_annotations_
    composition::SpriteVisual NoteRollRenderer::TakeGridVisual(size_t index) noexcept
    {
        if (index < m_gridPool.size())
        {
            return m_gridPool[index];
        }

        if (m_compositor == nullptr || m_gridLayer == nullptr)
        {
            return nullptr;
        }

        auto visual = m_compositor.CreateSpriteVisual();

        m_gridLayer.Children().InsertAtTop(visual);
        m_gridPool.push_back(visual);

        return visual;
    }

    _Use_decl_annotations_
    void NoteRollRenderer::HideGridFrom(size_t index) noexcept
    {
        for (auto entry = index; entry < m_gridPool.size(); ++entry)
        {
            m_gridPool[entry].IsVisible(false);
        }
    }

    _Use_decl_annotations_
    NoteVisuals NoteRollRenderer::TakeVisual(size_t index) noexcept
    {
        if (index < m_pool.size())
        {
            return m_pool[index];
        }

        if (m_compositor == nullptr || m_noteLayer == nullptr)
        {
            return {};
        }

        auto note = CreateNoteVisuals(m_compositor, m_noteLayer);

        m_pool.push_back(note);

        return note;
    }

    _Use_decl_annotations_
    composition::CompositionColorBrush NoteRollRenderer::NoteBrush(uint32_t state, bool border) noexcept
    {
        uint32_t const key = (state << 1) | (border ? 0x1u : 0x0u);

        auto const found = m_brushes.find(key);

        if (found != m_brushes.end())
        {
            return found->second;
        }

        if (m_compositor == nullptr)
        {
            return nullptr;
        }

        auto color = TrackColor(static_cast<uint16_t>(state >> 2));

        if ((state & 0x2u) == 0)
        {
            color.A = 48;
        }
        else if ((state & 0x1u) != 0)
        {
            color.A = 150;
        }

        if (border)
        {
            color = NoteBorderColor(color);
        }

        auto created = m_compositor.CreateColorBrush(color);

        // insert_or_assign, not operator[], because a WinRT type has no default constructor for
        // the map to build first.
        m_brushes.insert_or_assign(key, created);

        return created;
    }

    _Use_decl_annotations_
    void NoteRollRenderer::HideFrom(size_t index) noexcept
    {
        for (size_t entry = index; entry < m_pool.size(); ++entry)
        {
            m_pool[entry].Body.IsVisible(false);
        }
    }
}
