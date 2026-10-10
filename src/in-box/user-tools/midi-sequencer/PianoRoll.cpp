// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "PianoRoll.h"

using winrt::Windows::Foundation::Point;
using winrt::Windows::Foundation::Rect;

namespace midisequencer
{
    namespace
    {
        constexpr wchar_t TextFont[] = L"Segoe UI Variable Text";

        bool IsBlackKey(int32_t note) noexcept
        {
            switch (((note % 12) + 12) % 12)
            {
            case 1: case 3: case 6: case 8: case 10:
                return true;
            default:
                return false;
            }
        }

        bool NoteOrder(Note const& a, Note const& b) noexcept
        {
            return a.Tick != b.Tick ? a.Tick < b.Tick : a.Number < b.Number;
        }

        canvasText::CanvasTextFormat MakeFormat(float size, uint16_t weight, canvasText::CanvasHorizontalAlignment horizontal = canvasText::CanvasHorizontalAlignment::Left)
        {
            canvasText::CanvasTextFormat format{};
            format.FontFamily(TextFont);
            format.FontSize(size);
            format.FontWeight(winrt::Windows::UI::Text::FontWeight{ weight });
            format.WordWrapping(canvasText::CanvasWordWrapping::NoWrap);
            format.TrimmingGranularity(canvasText::CanvasTextTrimmingGranularity::Character);
            format.TrimmingSign(canvasText::CanvasTrimmingSign::Ellipsis);
            format.HorizontalAlignment(horizontal);
            format.VerticalAlignment(canvasText::CanvasVerticalAlignment::Center);
            return format;
        }

        uint16_t VelocityAt(float y, float laneTop, float laneHeight) noexcept
        {
            auto const usable = std::max(1.0f, laneHeight - 8.0f);
            auto const fraction = 1.0f - std::clamp((y - laneTop - 4.0f) / usable, 0.0f, 1.0f);
            return static_cast<uint16_t>(std::lround(fraction * 65535.0f));
        }

        std::vector<Note> Sorted(std::vector<Note> notes)
        {
            std::sort(notes.begin(), notes.end(), NoteOrder);
            return notes;
        }
    }

    _Use_decl_annotations_
    std::wstring NoteLabel(uint8_t number)
    {
        static constexpr std::array<wchar_t const*, 12> Names{ L"C", L"C#", L"D", L"D#", L"E", L"F", L"F#", L"G", L"G#", L"A", L"A#", L"B" };
        return std::format(L"{}{} ({})", Names[number % 12], static_cast<int32_t>(number / 12) - 2, number);
    }

    _Use_decl_annotations_
    std::wstring PositionLabel(std::vector<MeterChange> const& meter, int64_t tick)
    {
        auto const position = BarPositionAtTick(meter, std::max<int64_t>(0, tick));
        return std::format(L"{}.{}.{:03}", position.Bar, position.Beat, position.TicksIntoBeat);
    }

    _Use_decl_annotations_
    std::wstring LengthLabel(int64_t ticks)
    {
        auto const bar = TicksPerQuarterNote * 4;
        ticks = std::max<int64_t>(0, ticks);
        return std::format(L"{}.{}.{:03}", ticks / bar, (ticks % bar) / TicksPerQuarterNote, ticks % TicksPerQuarterNote);
    }

    void PianoRoll::EnsureFormats()
    {
        if (m_rulerText != nullptr)
        {
            return;
        }

        m_rulerText = MakeFormat(10.0f, 400);
        m_keyText = MakeFormat(9.0f, 400, canvasText::CanvasHorizontalAlignment::Right);
        m_laneTitle = MakeFormat(11.0f, 600);
        m_laneSub = MakeFormat(11.0f, 400);
        m_emptyText = MakeFormat(12.0f, 400, canvasText::CanvasHorizontalAlignment::Center);
    }

    _Use_decl_annotations_
    void PianoRoll::SetClip(Clip const* clip, Color color)
    {
        // Compared by the id kept here: the old pointer can be left dangling when the sequence's
        // clip list grows, which is exactly when the window calls this again.
        auto const changed = clip == nullptr || clip->Id != m_clipId;

        m_clip = clip;
        m_clipId = clip != nullptr ? clip->Id : std::wstring{};
        m_color = color;

        if (changed)
        {
            m_selected.clear();
            m_gesture = Gesture::None;
        }
    }

    _Use_decl_annotations_
    void PianoRoll::SetSelection(std::vector<Note> notes)
    {
        m_selected = Sorted(std::move(notes));
    }

    void PianoRoll::SelectAll()
    {
        if (m_clip != nullptr)
        {
            m_selected = Sorted(m_clip->Notes);
        }
    }

    _Use_decl_annotations_
    bool PianoRoll::IsSelected(Note const& note) const noexcept
    {
        auto range = std::equal_range(m_selected.begin(), m_selected.end(), note, NoteOrder);
        return std::find(range.first, range.second, note) != range.second;
    }

    _Use_decl_annotations_
    int64_t PianoRoll::Snap(double tick) const noexcept
    {
        auto const value = static_cast<int64_t>(std::floor(tick));

        if (m_snap <= 0)
        {
            return value;
        }

        auto const below = value >= 0 ? (value / m_snap) * m_snap : ((value - m_snap + 1) / m_snap) * m_snap;
        return below;
    }

    _Use_decl_annotations_
    int32_t PianoRoll::NoteAtY(double y) const noexcept
    {
        return static_cast<int32_t>(std::ceil(m_topNote - (y - RollTop()) / m_rowHeight));
    }

    void PianoRoll::ClampView() noexcept
    {
        m_rowHeight = std::clamp(m_rowHeight, 4.0, 30.0);
        m_pixelsPerTick = std::clamp(m_pixelsPerTick, 0.002, 2.0);

        auto const visibleRows = std::max(1.0, static_cast<double>(RollBottom() - RollTop()) / m_rowHeight);
        m_topNote = std::clamp(m_topNote, std::min(127.0, visibleRows - 1.0), 127.0);

        auto const rollWidth = std::max(1.0, static_cast<double>(m_width - RollLeft()));
        auto const clipLength = m_clip != nullptr ? static_cast<double>(m_clip->Length) : 3840.0;
        auto const maximum = std::max(0.0, clipLength + 3840.0 - rollWidth / m_pixelsPerTick);
        m_scrollTick = std::clamp(m_scrollTick, 0.0, maximum);
    }

    _Use_decl_annotations_
    void PianoRoll::FitToClip(float width, float height)
    {
        m_width = width;
        m_height = height;

        auto const rollWidth = std::max(40.0, static_cast<double>(width - RollLeft()));
        auto const rollHeight = std::max(40.0, static_cast<double>(RollBottom() - RollTop()));
        auto const length = m_clip != nullptr ? std::max<int64_t>(m_clip->Length, TicksPerQuarterNote) : TicksPerQuarterNote * 4;

        m_pixelsPerTick = rollWidth / static_cast<double>(length);
        m_scrollTick = 0;

        int32_t low{ 60 };
        int32_t high{ 60 };

        if (m_clip != nullptr && !m_clip->Notes.empty())
        {
            low = 127;
            high = 0;

            for (auto const& note : m_clip->Notes)
            {
                low = std::min<int32_t>(low, note.Number);
                high = std::max<int32_t>(high, note.Number);
            }
        }

        auto const rows = static_cast<double>(high - low + 9);
        m_rowHeight = std::clamp(rollHeight / rows, 5.0, 14.0);

        auto const visibleRows = rollHeight / m_rowHeight;
        m_topNote = (static_cast<double>(high + low) / 2.0) + visibleRows / 2.0;

        ClampView();
    }

    _Use_decl_annotations_
    int64_t PianoRoll::NoteIndexAt(Point point) const noexcept
    {
        if (m_clip == nullptr || point.X < RollLeft() || point.Y < RollTop() || point.Y > RollBottom())
        {
            return -1;
        }

        auto const number = NoteAtY(point.Y);
        auto const tick = TickAtX(point.X);

        // Last one wins, the one drawn on top.
        int64_t found{ -1 };
        auto const& notes = m_clip->Notes;

        auto first = std::lower_bound(notes.begin(), notes.end(), static_cast<int64_t>(tick) - TicksPerQuarterNote * 64,
            [](Note const& note, int64_t value) { return note.Tick < value; });

        for (auto it = first; it != notes.end() && static_cast<double>(it->Tick) <= tick; ++it)
        {
            if (it->Number == number && static_cast<double>(it->Tick + std::max<int64_t>(1, it->Length)) >= tick)
            {
                found = std::distance(notes.begin(), it);
            }
        }

        return found;
    }

    _Use_decl_annotations_
    bool PianoRoll::IsOverNoteEdge(Point point) const noexcept
    {
        auto const index = NoteIndexAt(point);

        if (index < 0)
        {
            return false;
        }

        auto const& note = m_clip->Notes[static_cast<size_t>(index)];
        auto const right = XAtTick(static_cast<double>(note.Tick + note.Length));
        auto const left = XAtTick(static_cast<double>(note.Tick));
        return right - left >= 10.0 && point.X >= right - 6.0;
    }

    _Use_decl_annotations_
    int64_t PianoRoll::VelocityStemAt(Point point) const noexcept
    {
        if (m_clip == nullptr)
        {
            return -1;
        }

        int64_t best{ -1 };
        double bestDistance{ 5.0 };

        auto const tick = TickAtX(point.X);
        auto const& notes = m_clip->Notes;

        auto first = std::lower_bound(notes.begin(), notes.end(), static_cast<int64_t>(TickAtX(point.X - 6.0)),
            [](Note const& note, int64_t value) { return note.Tick < value; });

        for (auto it = first; it != notes.end() && static_cast<double>(it->Tick) <= TickAtX(point.X + 6.0); ++it)
        {
            auto const distance = std::abs(XAtTick(static_cast<double>(it->Tick)) + 1.5 - point.X);

            if (distance <= bestDistance)
            {
                bestDistance = distance;
                best = std::distance(notes.begin(), it);
            }
        }

        UNREFERENCED_PARAMETER(tick);
        return best;
    }

    _Use_decl_annotations_
    std::optional<uint8_t> PianoRoll::KeyAt(Point point) const noexcept
    {
        if (point.X >= KeysWidth || point.Y < RollTop() || point.Y > RollBottom())
        {
            return std::nullopt;
        }

        auto const number = NoteAtY(point.Y);

        if (number < 0 || number > 127)
        {
            return std::nullopt;
        }

        return static_cast<uint8_t>(number);
    }

    _Use_decl_annotations_
    Note PianoRoll::PreviewOf(Note const& note) const noexcept
    {
        auto preview = note;

        if (m_gesture == Gesture::Move)
        {
            preview.Tick = std::max<int64_t>(0, note.Tick + m_deltaTicks);
            preview.Number = static_cast<uint8_t>(std::clamp<int32_t>(note.Number + m_deltaNotes, 0, 127));
        }
        else if (m_gesture == Gesture::Resize)
        {
            preview.Length = std::max<int64_t>(std::max<int64_t>(10, m_snap / 4), note.Length + m_deltaLength);
        }
        else if (m_gesture == Gesture::Velocity)
        {
            preview.Velocity = m_dragVelocity;
        }

        return preview;
    }

    _Use_decl_annotations_
    bool PianoRoll::PointerPressed(Point point, bool shift, bool control, bool rightButton, bool doubleClick)
    {
        m_pressPoint = point;
        m_lastPoint = point;
        m_deltaTicks = 0;
        m_deltaNotes = 0;
        m_deltaLength = 0;
        m_gesture = Gesture::None;

        if (m_clip == nullptr || point.Y < RollTop() || point.X < RollLeft())
        {
            return false;
        }

        if (point.Y > RollBottom())
        {
            auto const index = VelocityStemAt(point);

            if (index < 0)
            {
                return false;
            }

            auto const& note = m_clip->Notes[static_cast<size_t>(index)];

            if (!IsSelected(note))
            {
                m_selected = { note };
            }

            m_gesture = Gesture::Velocity;
            m_gestureNotes = m_selected;
            m_dragVelocity = VelocityAt(point.Y, RollBottom() + 1.0f, LaneHeight);
            return true;
        }

        auto const index = NoteIndexAt(point);

        if (rightButton)
        {
            if (index >= 0)
            {
                m_gestureNotes = { m_clip->Notes[static_cast<size_t>(index)] };
                m_gesture = Gesture::Draw;
                m_drawing = Note{};
                m_drawing.Length = -1;
            }

            return index >= 0;
        }

        if (index >= 0)
        {
            auto const& note = m_clip->Notes[static_cast<size_t>(index)];

            if (control)
            {
                if (IsSelected(note))
                {
                    auto range = std::equal_range(m_selected.begin(), m_selected.end(), note, NoteOrder);
                    auto found = std::find(range.first, range.second, note);

                    if (found != range.second)
                    {
                        m_selected.erase(found);
                    }
                }
                else
                {
                    m_selected.insert(std::upper_bound(m_selected.begin(), m_selected.end(), note, NoteOrder), note);
                }

                return true;
            }

            if (!IsSelected(note))
            {
                if (shift)
                {
                    m_selected.insert(std::upper_bound(m_selected.begin(), m_selected.end(), note, NoteOrder), note);
                }
                else
                {
                    m_selected = { note };
                }
            }

            m_gesture = IsOverNoteEdge(point) ? Gesture::Resize : Gesture::Move;
            m_gestureNotes = m_selected;
            return true;
        }

        if (m_tool == RollTool::Draw || doubleClick)
        {
            auto const number = std::clamp(NoteAtY(point.Y), 0, 127);

            m_drawing = Note{};
            m_drawing.Tick = std::max<int64_t>(0, Snap(TickAtX(point.X)));
            m_drawing.Length = std::max<int64_t>(1, m_snap > 0 ? m_snap : m_lastLength);
            m_drawing.Number = static_cast<uint8_t>(number);
            m_drawing.Channel = m_clip->Notes.empty() ? uint8_t{ 0 } : m_clip->Notes.front().Channel;
            m_drawing.Velocity = 0xC000;

                // The middle, which is what a MIDI 2.0 note off means when it doesn't care.
                m_drawing.ReleaseVelocity = 0x8000;

            m_selected.clear();
            m_gesture = Gesture::Draw;
            return true;
        }

        m_bandAdds = shift || control;

        if (!m_bandAdds)
        {
            m_selected.clear();
        }

        m_gesture = Gesture::Band;
        return true;
    }

    _Use_decl_annotations_
    bool PianoRoll::PointerMoved(Point point)
    {
        m_lastPoint = point;

        auto const grid = static_cast<double>(m_snap > 0 ? m_snap : 1);
        auto const tickDelta = TickAtX(point.X) - TickAtX(m_pressPoint.X);

        switch (m_gesture)
        {
        case Gesture::Move:
            m_deltaTicks = static_cast<int64_t>(std::llround(tickDelta / grid) * static_cast<int64_t>(grid));
            m_deltaNotes = NoteAtY(point.Y) - NoteAtY(m_pressPoint.Y);
            return true;

        case Gesture::Resize:
            m_deltaLength = static_cast<int64_t>(std::llround(tickDelta / grid) * static_cast<int64_t>(grid));
            return true;

        case Gesture::Draw:
            if (m_drawing.Length >= 0)
            {
                auto const end = static_cast<int64_t>(std::ceil(TickAtX(point.X) / grid) * grid);
                m_drawing.Length = std::max<int64_t>(static_cast<int64_t>(grid), end - m_drawing.Tick);
            }
            return true;

        case Gesture::Band:
            return true;

        case Gesture::Velocity:
            m_dragVelocity = VelocityAt(point.Y, RollBottom() + 1.0f, LaneHeight);
            return true;

        default:
            return false;
        }
    }

    _Use_decl_annotations_
    std::optional<RollEdit> PianoRoll::PointerReleased(Point point)
    {
        PointerMoved(point);

        auto const gesture = m_gesture;
        std::optional<RollEdit> edit{};

        switch (gesture)
        {
        case Gesture::Move:
        case Gesture::Resize:
        case Gesture::Velocity:
        {
            if ((gesture == Gesture::Move && m_deltaTicks == 0 && m_deltaNotes == 0) ||
                (gesture == Gesture::Resize && m_deltaLength == 0))
            {
                break;
            }

            RollEdit change{};
            change.Name = gesture == Gesture::Move ? L"Move" : gesture == Gesture::Resize ? L"Length" : L"Velocity";
            change.Removed = m_gestureNotes;

            for (auto const& note : m_gestureNotes)
            {
                change.Added.push_back(PreviewOf(note));
            }

            if (change.Added == change.Removed)
            {
                break;
            }

            m_selected = Sorted(change.Added);
            edit = std::move(change);
            break;
        }

        case Gesture::Draw:
        {
            RollEdit change{};

            if (m_drawing.Length < 0)
            {
                // A right click on a note removes it.
                change.Name = L"Erase";
                change.Removed = m_gestureNotes;
                m_selected.clear();
            }
            else
            {
                change.Name = L"Add";
                change.Added = { m_drawing };
                m_lastLength = m_drawing.Length;
                m_selected = { m_drawing };
            }

            edit = std::move(change);
            break;
        }

        case Gesture::Band:
        {
            if (m_clip != nullptr)
            {
                auto const left = std::min(m_pressPoint.X, point.X);
                auto const right = std::max(m_pressPoint.X, point.X);
                auto const top = std::min(m_pressPoint.Y, point.Y);
                auto const bottom = std::max(m_pressPoint.Y, point.Y);

                auto const fromTick = TickAtX(left);
                auto const toTick = TickAtX(right);
                auto const highNote = NoteAtY(top);
                auto const lowNote = NoteAtY(bottom);

                for (auto const& note : m_clip->Notes)
                {
                    auto const start = static_cast<double>(note.Tick);
                    auto const end = static_cast<double>(note.Tick + note.Length);

                    if (end >= fromTick && start <= toTick && note.Number >= lowNote && note.Number <= highNote && !IsSelected(note))
                    {
                        m_selected.insert(std::upper_bound(m_selected.begin(), m_selected.end(), note, NoteOrder), note);
                    }
                }
            }

            break;
        }

        default:
            break;
        }

        m_gesture = Gesture::None;
        m_gestureNotes.clear();
        return edit;
    }

    _Use_decl_annotations_
    bool PianoRoll::Wheel(Point point, int32_t delta, bool shift, bool control, float width, float height)
    {
        m_width = width;
        m_height = height;

        auto const steps = static_cast<double>(delta) / 120.0;

        if (control && shift)
        {
            auto const anchorNote = m_topNote - (static_cast<double>(point.Y) - RollTop()) / m_rowHeight;
            m_rowHeight *= std::pow(1.15, steps);
            m_rowHeight = std::clamp(m_rowHeight, 4.0, 30.0);
            m_topNote = anchorNote + (static_cast<double>(point.Y) - RollTop()) / m_rowHeight;
        }
        else if (control)
        {
            auto const anchorTick = TickAtX(point.X);
            m_pixelsPerTick *= std::pow(1.2, steps);
            m_pixelsPerTick = std::clamp(m_pixelsPerTick, 0.002, 2.0);
            m_scrollTick = anchorTick - (static_cast<double>(point.X) - RollLeft()) / m_pixelsPerTick;
        }
        else if (shift)
        {
            m_scrollTick -= steps * (static_cast<double>(width - RollLeft()) * 0.15) / m_pixelsPerTick;
        }
        else
        {
            m_topNote += steps * 3.0;
        }

        ClampView();
        return true;
    }

    std::optional<RollEdit> PianoRoll::DeleteSelection()
    {
        if (m_clip == nullptr || m_selected.empty())
        {
            return std::nullopt;
        }

        RollEdit edit{};
        edit.Name = L"Delete";
        edit.Removed = m_selected;
        m_selected.clear();
        return edit;
    }

    _Use_decl_annotations_
    std::optional<RollEdit> PianoRoll::MoveSelection(int64_t ticks, int32_t semitones)
    {
        if (m_clip == nullptr || m_selected.empty())
        {
            return std::nullopt;
        }

        RollEdit edit{};
        edit.Name = L"Move";
        edit.Removed = m_selected;

        for (auto note : m_selected)
        {
            note.Tick = std::max<int64_t>(0, note.Tick + ticks);
            note.Number = static_cast<uint8_t>(std::clamp<int32_t>(note.Number + semitones, 0, 127));
            edit.Added.push_back(note);
        }

        if (edit.Added == edit.Removed)
        {
            return std::nullopt;
        }

        m_selected = Sorted(edit.Added);
        return edit;
    }

    _Use_decl_annotations_
    std::optional<RollEdit> PianoRoll::TransposeSelection(int32_t semitones)
    {
        auto edit = MoveSelection(0, semitones);

        if (edit.has_value())
        {
            edit->Name = L"Transpose";
        }

        return edit;
    }

    _Use_decl_annotations_
    std::optional<RollEdit> PianoRoll::QuantizeSelection(int64_t grid)
    {
        if (m_clip == nullptr || grid <= 0)
        {
            return std::nullopt;
        }

        // Nothing selected quantizes the whole clip, which is what the button usually means.
        auto const source = m_selected.empty() ? m_clip->Notes : m_selected;

        RollEdit edit{};
        edit.Name = L"Quantize";

        for (auto const& note : source)
        {
            auto moved = note;
            auto const below = (note.Tick / grid) * grid;
            moved.Tick = note.Tick - below >= grid / 2 ? below + grid : below;

            if (moved != note)
            {
                edit.Removed.push_back(note);
                edit.Added.push_back(moved);
            }
        }

        if (edit.Removed.empty())
        {
            return std::nullopt;
        }

        if (!m_selected.empty())
        {
            std::vector<Note> selection = m_selected;

            for (size_t i = 0; i < edit.Removed.size(); ++i)
            {
                auto found = std::find(selection.begin(), selection.end(), edit.Removed[i]);

                if (found != selection.end())
                {
                    *found = edit.Added[i];
                }
            }

            m_selected = Sorted(selection);
        }

        return edit;
    }

    // ---------------------------------------------------------------- drawing

    _Use_decl_annotations_
    void PianoRoll::Draw(canvas::CanvasDrawingSession const& ds, float width, float height)
    {
        m_width = width;
        m_height = height;

        if (m_palette == nullptr)
        {
            return;
        }

        EnsureFormats();
        ClampView();

        auto const& colors = *m_palette;

        ds.Antialiasing(canvas::CanvasAntialiasing::Aliased);

        // the roll
        ds.FillRectangle(RollLeft(), RollTop(), width - RollLeft(), RollBottom() - RollTop(), colors.RollBackground);

        auto const firstNote = std::clamp(NoteAtY(RollTop()), 0, 127);
        auto const lastNote = std::clamp(NoteAtY(RollBottom() - 0.01f), 0, 127);

        for (auto note = firstNote; note >= lastNote; --note)
        {
            auto const y = static_cast<float>(YAtNote(note));
            auto const top = std::max(y, RollTop());
            auto const bottom = std::min(y + static_cast<float>(m_rowHeight), RollBottom());

            if (bottom <= top)
            {
                continue;
            }

            ds.FillRectangle(RollLeft(), top, width - RollLeft(), bottom - top, IsBlackKey(note) ? colors.RollBlackKeyRow : colors.RollWhiteKeyRow);

            if (note % 12 == 0 && y + m_rowHeight <= RollBottom())
            {
                ds.FillRectangle(RollLeft(), std::floor(y + static_cast<float>(m_rowHeight)) - 1.0f, width - RollLeft(), 1.0f, colors.RollOctaveLine);
            }
        }

        DrawGrid(ds, RollTop(), RollBottom(), true);

        if (m_clip != nullptr)
        {
            // Past the end of the clip is shaded: nothing there plays.
            auto const endX = static_cast<float>(XAtTick(static_cast<double>(m_clip->Length)));

            if (endX < width)
            {
                auto const from = std::max(endX, RollLeft());
                ds.FillRectangle(from, RollTop(), width - from, RollBottom() - RollTop(), Rgba(0, 0, 0, colors.Light ? 0.06 : 0.22));
                ds.FillRectangle(std::floor(from), RollTop(), 1.0f, RollBottom() - RollTop(), WithAlpha(colors.Accent, 0.6));
            }
        }

        ds.Antialiasing(canvas::CanvasAntialiasing::Antialiased);

        {
            auto layer = ds.CreateLayer(1.0f, Rect{ RollLeft(), RollTop(), std::max(0.0f, width - RollLeft()), std::max(0.0f, RollBottom() - RollTop()) });
            DrawNotes(ds);

            if (m_gesture == Gesture::Band)
            {
                auto const left = std::min(m_pressPoint.X, m_lastPoint.X);
                auto const top = std::min(m_pressPoint.Y, m_lastPoint.Y);
                Rect const band{ left, top, std::abs(m_lastPoint.X - m_pressPoint.X), std::abs(m_lastPoint.Y - m_pressPoint.Y) };
                ds.FillRectangle(band, WithAlpha(colors.Accent, 0.12));
                ds.DrawRectangle(band, WithAlpha(colors.Accent, 0.7), 1.0f);
            }

            if (m_playhead >= 0)
            {
                auto const x = static_cast<float>(XAtTick(static_cast<double>(m_playhead)));
                ds.FillRectangle(x, RollTop(), 1.5f, RollBottom() - RollTop(), colors.Playhead);
            }

            layer.Close();
        }

        if (m_clip == nullptr && !m_strings.EmptyClip.empty())
        {
            ds.DrawText(m_strings.EmptyClip, Rect{ RollLeft(), RollTop(), width - RollLeft(), RollBottom() - RollTop() }, colors.Text3, m_emptyText);
        }

        DrawKeys(ds);
        DrawRuler(ds);
        DrawVelocityLane(ds);
    }

    _Use_decl_annotations_
    void PianoRoll::DrawGrid(canvas::CanvasDrawingSession const& ds, float top, float bottom, bool steps)
    {
        auto const& colors = *m_palette;

        if (m_meter == nullptr || m_meter->empty())
        {
            return;
        }

        auto const startTick = std::max<int64_t>(0, static_cast<int64_t>(TickAtX(RollLeft())));
        auto const endTick = static_cast<int64_t>(TickAtX(m_width)) + 1;

        // Sixteenth notes, beats and bars, each only when it's far enough from the next to see.
        constexpr int64_t Step = TicksPerQuarterNote / 4;
        auto const stepPixels = static_cast<double>(Step) * m_pixelsPerTick;
        auto const beatPixels = static_cast<double>(TicksPerQuarterNote) * m_pixelsPerTick;

        auto tick = (startTick / Step) * Step;

        for (int guard = 0; tick <= endTick && guard < 4000; tick += Step, ++guard)
        {
            auto const position = BarPositionAtTick(*m_meter, tick);
            auto const onBar = position.Beat == 1 && position.TicksIntoBeat == 0;
            auto const onBeat = position.TicksIntoBeat == 0;

            Color color{};

            if (onBar)
            {
                color = colors.RollBarLine;
            }
            else if (onBeat)
            {
                if (beatPixels < 5.0)
                {
                    continue;
                }

                color = colors.RollBeatLine;
            }
            else
            {
                if (!steps || stepPixels < 5.0)
                {
                    continue;
                }

                color = colors.RollStepLine;
            }

            auto const x = std::floor(static_cast<float>(XAtTick(static_cast<double>(tick))));

            if (x >= RollLeft())
            {
                ds.FillRectangle(x, top, 1.0f, bottom - top, color);
            }
        }
    }

    _Use_decl_annotations_
    void PianoRoll::DrawNotes(canvas::CanvasDrawingSession const& ds)
    {
        if (m_clip == nullptr)
        {
            return;
        }

        auto const& colors = *m_palette;
        auto const& notes = m_clip->Notes;

        auto const visibleFrom = TickAtX(RollLeft());
        auto const visibleTo = TickAtX(m_width);
        auto const moving = m_gesture == Gesture::Move || m_gesture == Gesture::Resize;

        auto const drawNote = [&](Note const& note, bool selected)
        {
            auto const x = static_cast<float>(XAtTick(static_cast<double>(note.Tick))) + 0.5f;
            auto const w = std::max(3.0f, static_cast<float>(static_cast<double>(note.Length) * m_pixelsPerTick) - 1.0f);
            auto const y = static_cast<float>(YAtNote(note.Number)) + 1.0f;
            auto const h = std::max(3.0f, static_cast<float>(m_rowHeight) - 2.0f);

            if (y > RollBottom() || y + h < RollTop())
            {
                return;
            }

            auto const opacity = 0.45 + 0.55 * (static_cast<double>(note.Velocity) / 65535.0);

            ds.FillRoundedRectangle(x, y, w, h, 2.0f, 2.0f, WithAlpha(m_color, opacity));

            if (selected)
            {
                ds.DrawRoundedRectangle(x, y, w, h, 2.0f, 2.0f, colors.SelectionStroke, 1.6f);

                if (h >= 8.0f && w >= 8.0f)
                {
                    ds.FillRoundedRectangle(x + w - 3.0f, y + 2.0f, 2.0f, h - 4.0f, 1.0f, 1.0f, colors.SelectionStroke);
                }
            }
            else
            {
                ds.DrawRoundedRectangle(x, y, w, h, 2.0f, 2.0f, colors.NoteEdge, 1.0f);
            }
        };

        auto first = std::lower_bound(notes.begin(), notes.end(), static_cast<int64_t>(visibleFrom) - TicksPerQuarterNote * 64,
            [](Note const& note, int64_t value) { return note.Tick < value; });

        for (auto it = first; it != notes.end() && static_cast<double>(it->Tick) <= visibleTo; ++it)
        {
            if (static_cast<double>(it->Tick + it->Length) < visibleFrom)
            {
                continue;
            }

            auto const selected = IsSelected(*it);

            // While a gesture moves notes, they're drawn where they're going.
            if (moving && selected)
            {
                continue;
            }

            drawNote(*it, selected);
        }

        if (moving)
        {
            for (auto const& note : m_gestureNotes)
            {
                drawNote(PreviewOf(note), true);
            }
        }

        if (m_gesture == Gesture::Draw && m_drawing.Length > 0)
        {
            drawNote(m_drawing, true);
        }
    }

    _Use_decl_annotations_
    void PianoRoll::DrawKeys(canvas::CanvasDrawingSession const& ds)
    {
        auto const& colors = *m_palette;

        ds.Antialiasing(canvas::CanvasAntialiasing::Aliased);

        auto const firstNote = std::clamp(NoteAtY(RollTop()), 0, 127);
        auto const lastNote = std::clamp(NoteAtY(RollBottom() - 0.01f), 0, 127);

        {
            auto layer = ds.CreateLayer(1.0f, Rect{ 0, RollTop(), KeysWidth, std::max(0.0f, RollBottom() - RollTop()) });

            for (auto note = firstNote; note >= lastNote; --note)
            {
                auto const y = static_cast<float>(YAtNote(note));
                auto const h = static_cast<float>(m_rowHeight);
                auto const black = IsBlackKey(note);
                auto const keyWidth = black ? KeysWidth * 0.62f : KeysWidth;

                ds.FillRectangle(0, y, keyWidth, h, black ? colors.KeyBlack : colors.KeyWhite);
                ds.DrawRectangle(0, y, keyWidth, h, colors.KeyEdge, 0.4f);

                if (note % 12 == 0 && h >= 6.0f)
                {
                    m_keyText.FontSize(std::min(10.0f, h));
                    ds.DrawText(winrt::hstring{ std::format(L"C{}", note / 12 - 2) }, 0, y, KeysWidth - 3.0f, h, colors.KeyLabel, m_keyText);
                }
            }

            layer.Close();
        }

        ds.FillRectangle(KeysWidth - 1.0f, RollTop(), 1.0f, RollBottom() - RollTop(), colors.LaneEdge);
        ds.Antialiasing(canvas::CanvasAntialiasing::Antialiased);
    }

    _Use_decl_annotations_
    void PianoRoll::DrawRuler(canvas::CanvasDrawingSession const& ds)
    {
        auto const& colors = *m_palette;

        ds.Antialiasing(canvas::CanvasAntialiasing::Aliased);
        ds.FillRectangle(KeysWidth - 1.0f, 0, 1.0f, RulerHeight, colors.LaneEdge);
        ds.FillRectangle(0, RulerHeight - 1.0f, m_width, 1.0f, colors.Divider);

        if (m_meter == nullptr || m_meter->empty())
        {
            return;
        }

        {
            auto layer = ds.CreateLayer(1.0f, Rect{ RollLeft(), 0, std::max(0.0f, m_width - RollLeft()), RulerHeight - 1.0f });

            auto const startTick = std::max<int64_t>(0, static_cast<int64_t>(TickAtX(RollLeft())));
            auto const endTick = static_cast<int64_t>(TickAtX(m_width)) + 1;
            auto const beatPixels = static_cast<double>(TicksPerQuarterNote) * m_pixelsPerTick;

            auto tick = (startTick / TicksPerQuarterNote) * TicksPerQuarterNote;

            for (int guard = 0; tick <= endTick && guard < 4000; tick += TicksPerQuarterNote, ++guard)
            {
                auto const position = BarPositionAtTick(*m_meter, tick);
                auto const x = std::floor(static_cast<float>(XAtTick(static_cast<double>(tick))));

                if (position.Beat == 1 && position.TicksIntoBeat == 0)
                {
                    ds.FillRectangle(x, 0, 1.0f, RulerHeight, colors.Light ? Rgba(0, 0, 0, 0.2) : Rgba(255, 255, 255, 0.2));
                    ds.DrawText(winrt::hstring{ std::to_wstring(position.Bar) }, x + 4.0f, 0, 40.0f, 16.0f, colors.Text3, m_rulerText);
                }
                else if (beatPixels >= 5.0)
                {
                    ds.FillRectangle(x, 12.0f, 1.0f, RulerHeight - 12.0f, colors.Light ? Rgba(0, 0, 0, 0.07) : Rgba(255, 255, 255, 0.07));
                }
            }

            if (m_clip != nullptr && m_clip->Loop)
            {
                auto const x0 = static_cast<float>(XAtTick(0));
                auto const x1 = static_cast<float>(XAtTick(static_cast<double>(m_clip->Length)));
                ds.FillRectangle(x0, RulerHeight - 6.0f, x1 - x0, 5.0f, WithAlpha(colors.Accent, colors.Light ? 0.25 : 0.30));
            }

            layer.Close();
        }

        ds.Antialiasing(canvas::CanvasAntialiasing::Antialiased);
    }

    _Use_decl_annotations_
    void PianoRoll::DrawVelocityLane(canvas::CanvasDrawingSession const& ds)
    {
        auto const& colors = *m_palette;
        auto const laneTop = RollBottom() + 1.0f;
        auto const laneHeight = m_height - laneTop;

        if (laneHeight < 8.0f)
        {
            return;
        }

        ds.Antialiasing(canvas::CanvasAntialiasing::Aliased);
        ds.FillRectangle(0, laneTop - 1.0f, m_width, 1.0f, colors.Light ? Rgba(0, 0, 0, 0.08) : Rgba(255, 255, 255, 0.10));
        ds.FillRectangle(RollLeft(), laneTop, m_width - RollLeft(), laneHeight, colors.RollBackground);
        ds.FillRectangle(KeysWidth - 1.0f, laneTop, 1.0f, laneHeight, colors.LaneEdge);

        // The lane's name and resolution, in the corner under the keys.
        ds.Antialiasing(canvas::CanvasAntialiasing::Antialiased);
        ds.DrawText(m_strings.Velocity, 8.0f, laneTop + 4.0f, KeysWidth - 12.0f, 14.0f, colors.Text, m_laneTitle);

        auto const& sub = m_valuesAs == ValueDisplay::Midi1 ? m_strings.Midi1Velocity
            : m_valuesAs == ValueDisplay::Percent ? m_strings.PercentVelocity : m_strings.Midi2Velocity;
        ds.DrawText(sub, 8.0f, laneTop + 19.0f, KeysWidth - 12.0f, 14.0f, colors.Text3, m_laneSub);

        {
            auto layer = ds.CreateLayer(1.0f, Rect{ RollLeft(), laneTop, std::max(0.0f, m_width - RollLeft()), laneHeight });

            ds.Antialiasing(canvas::CanvasAntialiasing::Aliased);

            if (m_meter != nullptr && !m_meter->empty())
            {
                auto const startTick = std::max<int64_t>(0, static_cast<int64_t>(TickAtX(RollLeft())));
                auto const endTick = static_cast<int64_t>(TickAtX(m_width)) + 1;
                auto bar = BarPositionAtTick(*m_meter, startTick).Bar;

                for (int guard = 0; guard < 2000; ++guard, ++bar)
                {
                    auto const tick = TickAtBar(*m_meter, bar);

                    if (tick > endTick)
                    {
                        break;
                    }

                    ds.FillRectangle(std::floor(static_cast<float>(XAtTick(static_cast<double>(tick)))), laneTop, 1.0f, laneHeight,
                        colors.Light ? Rgba(0, 0, 0, 0.14) : Rgba(255, 255, 255, 0.14));
                }
            }

            ds.Antialiasing(canvas::CanvasAntialiasing::Antialiased);

            canvasGeometry::CanvasStrokeStyle dashed{};
            dashed.CustomDashStyle({ 3.0f, 3.0f });
            ds.DrawLine(RollLeft(), laneTop + laneHeight / 2.0f, m_width, laneTop + laneHeight / 2.0f,
                colors.Light ? Rgba(0, 0, 0, 0.07) : Rgba(255, 255, 255, 0.07), 1.0f, dashed);

            if (m_clip != nullptr)
            {
                auto const visibleFrom = TickAtX(RollLeft() - 4.0f);
                auto const visibleTo = TickAtX(m_width);
                auto const& notes = m_clip->Notes;
                auto const velocityDrag = m_gesture == Gesture::Velocity;

                auto const drawStem = [&](Note const& note, bool selected)
                {
                    auto const x = static_cast<float>(XAtTick(static_cast<double>(note.Tick))) + 1.5f;
                    auto const top = laneTop + 4.0f + (1.0f - static_cast<float>(note.Velocity) / 65535.0f) * (laneHeight - 8.0f);
                    auto const color = selected ? colors.SelectionStroke : m_color;

                    ds.DrawLine(x, laneTop + laneHeight, x, top, color, selected ? 2.4f : 2.0f);
                    ds.FillCircle(x, top, selected ? 3.4f : 2.6f, color);
                };

                auto first = std::lower_bound(notes.begin(), notes.end(), static_cast<int64_t>(visibleFrom),
                    [](Note const& note, int64_t value) { return note.Tick < value; });

                for (auto it = first; it != notes.end() && static_cast<double>(it->Tick) <= visibleTo; ++it)
                {
                    auto const selected = IsSelected(*it);

                    if (velocityDrag && selected)
                    {
                        continue;
                    }

                    drawStem(*it, selected);
                }

                if (velocityDrag)
                {
                    for (auto const& note : m_gestureNotes)
                    {
                        drawStem(PreviewOf(note), true);
                    }
                }
            }

            layer.Close();
        }
    }
}
