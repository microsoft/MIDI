// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Deliberately free of pch.h and XAML, so the unit tests compile it unchanged.
//
// This file holds the document, the selection, placing a control and moving one. Properties,
// messages, pages and the device table are in EditorControllerEdits.cpp.

#include "EditorController.h"
#include "ControlFactory.h"
#include "PageTemplates.h"

#include <algorithm>
#include <cmath>

namespace glass
{
    namespace
    {
        EditRect RectOf(_In_ Control const& control) noexcept
        {
            return { control.X, control.Y, control.Width, control.Height };
        }

        void ApplyRect(_Inout_ Control& control, _In_ EditRect const& rect) noexcept
        {
            control.X = rect.X;
            control.Y = rect.Y;
            control.Width = rect.Width;
            control.Height = rect.Height;
        }
    }

    // ---------------------------------------------------------------- the document

    _Use_decl_annotations_
    void EditorController::Load(LayoutDocument document)
    {
        m_document = std::move(document);

        if (m_document.Pages.empty())
        {
            Page page{};
            page.Id = LayoutDocument::NewId();
            m_document.Pages.push_back(std::move(page));
        }

        m_pageIndex = 0;
        m_selection.clear();
        m_dragOrigins.clear();
        m_dragging = false;
        m_resizeHandle = ResizeHandle::None;
        m_snapSuspended = false;
        m_dirty = false;

        m_snap.GridSize = DefaultGridSize;

        m_undo.Reset(m_document);
    }

    void EditorController::Commit(_In_ wchar_t const* name)
    {
        m_undo.Commit(m_document, name);
        m_dirty = true;
    }

    void EditorController::CommitCoalesced(_In_ wchar_t const* name, _In_ std::wstring const& key)
    {
        m_undo.CommitCoalesced(m_document, name, key);
        m_dirty = true;
    }

    bool EditorController::Undo()
    {
        if (!m_undo.Undo(m_document))
        {
            return false;
        }

        if (m_pageIndex >= m_document.Pages.size())
        {
            m_pageIndex = m_document.Pages.empty() ? 0 : m_document.Pages.size() - 1;
        }

        DropSelectionOfMissingControls();
        m_dirty = true;

        return true;
    }

    bool EditorController::Redo()
    {
        if (!m_undo.Redo(m_document))
        {
            return false;
        }

        if (m_pageIndex >= m_document.Pages.size())
        {
            m_pageIndex = m_document.Pages.empty() ? 0 : m_document.Pages.size() - 1;
        }

        DropSelectionOfMissingControls();
        m_dirty = true;

        return true;
    }

    // ---------------------------------------------------------------- pages

    _Use_decl_annotations_
    void EditorController::SetPageIndex(size_t index) noexcept
    {
        if (index >= m_document.Pages.size() || index == m_pageIndex)
        {
            return;
        }

        m_pageIndex = index;
        m_selection.clear();
    }

    Page const* EditorController::CurrentPage() const noexcept
    {
        return m_pageIndex < m_document.Pages.size() ? &m_document.Pages[m_pageIndex] : nullptr;
    }

    Page* EditorController::MutablePage() noexcept
    {
        return m_pageIndex < m_document.Pages.size() ? &m_document.Pages[m_pageIndex] : nullptr;
    }

    Control* EditorController::MutableControl(_In_ std::wstring const& id) noexcept
    {
        auto* const page = MutablePage();

        if (page == nullptr)
        {
            return nullptr;
        }

        for (auto& control : page->Controls)
        {
            if (control.Id == id)
            {
                return &control;
            }
        }

        return nullptr;
    }

    // ---------------------------------------------------------------- selection

    _Use_decl_annotations_
    bool EditorController::IsSelected(std::wstring const& id) const noexcept
    {
        return std::find(m_selection.begin(), m_selection.end(), id) != m_selection.end();
    }

    void EditorController::ClearSelection()
    {
        m_selection.clear();
    }

    _Use_decl_annotations_
    void EditorController::SelectOnly(std::wstring const& id)
    {
        m_selection.clear();

        if (!id.empty())
        {
            m_selection.push_back(id);
        }
    }

    _Use_decl_annotations_
    void EditorController::AddToSelection(std::wstring const& id)
    {
        if (!id.empty() && !IsSelected(id))
        {
            m_selection.push_back(id);
        }
    }

    _Use_decl_annotations_
    void EditorController::ToggleSelected(std::wstring const& id)
    {
        auto const found = std::find(m_selection.begin(), m_selection.end(), id);

        if (found == m_selection.end())
        {
            m_selection.push_back(id);
        }
        else
        {
            m_selection.erase(found);
        }
    }

    void EditorController::SelectAll()
    {
        m_selection.clear();

        auto const* const page = CurrentPage();

        if (page == nullptr)
        {
            return;
        }

        for (auto const& control : page->Controls)
        {
            m_selection.push_back(control.Id);
        }
    }

    void EditorController::SelectOutsidePage()
    {
        m_selection.clear();

        auto const* const page = CurrentPage();

        if (page == nullptr)
        {
            return;
        }

        for (auto const& control : page->Controls)
        {
            if (IsOutsidePage(RectOf(control), m_document.PageWidth, m_document.PageHeight))
            {
                m_selection.push_back(control.Id);
            }
        }
    }

    _Use_decl_annotations_
    void EditorController::SelectInRectangle(EditRect const& area, bool add)
    {
        if (!add)
        {
            m_selection.clear();
        }

        auto const* const page = CurrentPage();

        if (page == nullptr)
        {
            return;
        }

        for (auto const& control : page->Controls)
        {
            if (Intersects(area, RectOf(control)))
            {
                AddToSelection(control.Id);
            }
        }
    }

    std::vector<Control const*> EditorController::SelectedControls() const
    {
        std::vector<Control const*> selected{};

        auto const* const page = CurrentPage();

        if (page == nullptr)
        {
            return selected;
        }

        // In the page's own order rather than the order they were clicked, so an arrange
        // operation gives the same answer whichever way somebody built the selection.
        for (auto const& control : page->Controls)
        {
            if (IsSelected(control.Id))
            {
                selected.push_back(&control);
            }
        }

        return selected;
    }

    _Use_decl_annotations_
    int32_t EditorController::ControlIndexOf(std::wstring const& id) const
    {
        int32_t index{ 0 };

        for (auto const& page : m_document.Pages)
        {
            for (auto const& control : page.Controls)
            {
                if (control.Id == id)
                {
                    return index;
                }

                index++;
            }
        }

        return -1;
    }

    void EditorController::DropSelectionOfMissingControls()
    {
        auto const* const page = CurrentPage();

        if (page == nullptr)
        {
            m_selection.clear();
            return;
        }

        std::vector<std::wstring> kept{};

        for (auto const& id : m_selection)
        {
            for (auto const& control : page->Controls)
            {
                if (control.Id == id)
                {
                    kept.push_back(id);
                    break;
                }
            }
        }

        m_selection = std::move(kept);
    }

    // ---------------------------------------------------------------- snapping

    _Use_decl_annotations_
    void EditorController::SetGridEnabled(bool enabled) noexcept
    {
        m_snap.GridEnabled = enabled;
    }

    _Use_decl_annotations_
    void EditorController::SetGridSize(double size) noexcept
    {
        if (std::isfinite(size) && size > 0.0)
        {
            m_snap.GridSize = size;
        }
    }

    SnapSettings EditorController::EffectiveSnap() const noexcept
    {
        auto settings = m_snap;

        if (m_snapSuspended)
        {
            settings.GridEnabled = false;
            settings.GuidesEnabled = false;
        }

        return settings;
    }

    EditRect EditorController::PageRect() const noexcept
    {
        return { 0.0, 0.0, static_cast<double>(m_document.PageWidth), static_cast<double>(m_document.PageHeight) };
    }

    _Use_decl_annotations_
    std::vector<EditRect> EditorController::OtherRects(std::vector<std::wstring> const& excluding) const
    {
        std::vector<EditRect> rects{};

        auto const* const page = CurrentPage();

        if (page == nullptr)
        {
            return rects;
        }

        for (auto const& control : page->Controls)
        {
            if (std::find(excluding.begin(), excluding.end(), control.Id) == excluding.end())
            {
                rects.push_back(RectOf(control));
            }
        }

        return rects;
    }

    // ---------------------------------------------------------------- placing controls

    _Use_decl_annotations_
    std::wstring EditorController::AddControl(ControlKind kind, double x, double y)
    {
        auto* const page = MutablePage();

        if (page == nullptr || page->Controls.size() >= MaximumControlsPerPage)
        {
            return {};
        }

        std::wstring deviceName{};

        if (!m_document.Devices.empty())
        {
            deviceName = m_document.Devices[0].Name;
        }

        auto control = MakeNewControl(kind, x, y, m_document.PageWidth, m_document.PageHeight, deviceName, *page);

        auto const settings = EffectiveSnap();

        if (settings.GridEnabled)
        {
            control.X = SnapToGrid(control.X, settings.GridSize);
            control.Y = SnapToGrid(control.Y, settings.GridSize);
        }

        auto const id = control.Id;

        page->Controls.push_back(std::move(control));

        Commit(EditNames::Add);
        SelectOnly(id);

        return id;
    }

    _Use_decl_annotations_
    std::wstring EditorController::AddControlCentered(ControlKind kind, double centerX, double centerY)
    {
        auto const size = DefaultControlSize(kind, m_document.PageWidth, m_document.PageHeight);

        return AddControl(kind, centerX - size.Width / 2.0, centerY - size.Height / 2.0);
    }

    _Use_decl_annotations_
    std::wstring EditorController::AddControlInRectangle(ControlKind kind, EditRect const& area)
    {
        auto* const page = MutablePage();

        if (page == nullptr || page->Controls.size() >= MaximumControlsPerPage)
        {
            return {};
        }

        std::wstring deviceName{};

        if (!m_document.Devices.empty())
        {
            deviceName = m_document.Devices[0].Name;
        }

        auto control = MakeNewControl(
            kind, area.X, area.Y, m_document.PageWidth, m_document.PageHeight, deviceName, *page);

        auto const settings = EffectiveSnap();

        auto x = area.X;
        auto y = area.Y;
        auto width = std::max(MinimumControlSize, area.Width);
        auto height = std::max(MinimumControlSize, area.Height);

        if (settings.GridEnabled)
        {
            x = SnapToGrid(x, settings.GridSize);
            y = SnapToGrid(y, settings.GridSize);
            width = std::max(settings.GridSize, SnapToGrid(width, settings.GridSize));
            height = std::max(settings.GridSize, SnapToGrid(height, settings.GridSize));
        }

        // A knob drawn as a wide rectangle is still a knob, so the shorter side wins rather than
        // the control arriving as an ellipse nobody asked for.
        if (control.AspectLocked)
        {
            width = height = std::min(width, height);
        }

        control.X = x;
        control.Y = y;
        control.Width = width;
        control.Height = height;

        auto const id = control.Id;

        page->Controls.push_back(std::move(control));

        // One entry. Drawing a control out to a size is one action, and undoing it must not
        // leave a default-sized one behind.
        Commit(EditNames::Add);
        SelectOnly(id);

        return id;
    }

    _Use_decl_annotations_
    std::wstring EditorController::AddControlAtFreeSpot(ControlKind kind)
    {
        auto const* const page = CurrentPage();

        if (page == nullptr)
        {
            return {};
        }

        auto const size = DefaultControlSize(kind, m_document.PageWidth, m_document.PageHeight);

        auto const step = m_snap.GridSize > 0.0 ? m_snap.GridSize : DefaultGridSize;
        auto const margin = step * 2.0;

        auto const existing = OtherRects({});

        for (double y = margin; y + size.Height <= m_document.PageHeight - margin; y += step)
        {
            for (double x = margin; x + size.Width <= m_document.PageWidth - margin; x += step)
            {
                EditRect candidate{ x, y, static_cast<double>(size.Width), static_cast<double>(size.Height) };

                auto clear = true;

                for (auto const& other : existing)
                {
                    if (Intersects(candidate, other))
                    {
                        clear = false;
                        break;
                    }
                }

                if (clear)
                {
                    return AddControl(kind, x, y);
                }
            }
        }

        // A full page still has to accept one more rather than silently refusing. It lands in
        // the corner, visibly overlapping, which is a problem somebody can see and fix.
        return AddControl(kind, margin, margin);
    }

    bool EditorController::DeleteSelection()
    {
        auto* const page = MutablePage();

        if (page == nullptr || m_selection.empty())
        {
            return false;
        }

        auto const before = page->Controls.size();

        page->Controls.erase(
            std::remove_if(
                page->Controls.begin(),
                page->Controls.end(),
                [this](Control const& control) { return IsSelected(control.Id); }),
            page->Controls.end());

        if (page->Controls.size() == before)
        {
            return false;
        }

        m_selection.clear();
        Commit(EditNames::Delete);

        return true;
    }

    bool EditorController::DuplicateSelection()
    {
        auto* const page = MutablePage();

        if (page == nullptr || m_selection.empty())
        {
            return false;
        }

        auto const selected = SelectedControls();

        if (page->Controls.size() + selected.size() > MaximumControlsPerPage)
        {
            return false;
        }

        // Offset by one grid cell so the copy is visibly a second control rather than sitting
        // exactly on top of the first.
        auto const offset = m_snap.GridSize > 0.0 ? m_snap.GridSize * 2.0 : 16.0;

        std::vector<Control> copies{};
        std::vector<std::wstring> newSelection{};

        auto order = 0;

        for (auto const& control : page->Controls)
        {
            order = std::max(order, control.KeyboardOrder);
        }

        for (auto const* const source : selected)
        {
            auto copy = *source;

            copy.Id = LayoutDocument::NewId();
            copy.X += offset;
            copy.Y += offset;
            copy.KeyboardOrder = ++order;

            newSelection.push_back(copy.Id);
            copies.push_back(std::move(copy));
        }

        for (auto& copy : copies)
        {
            page->Controls.push_back(std::move(copy));
        }

        m_selection = std::move(newSelection);
        Commit(EditNames::Duplicate);

        return true;
    }

    // ---------------------------------------------------------------- moving and resizing

    void EditorController::BeginDrag()
    {
        m_dragOrigins.clear();

        for (auto const* const control : SelectedControls())
        {
            m_dragOrigins.push_back({ control->Id, RectOf(*control) });
        }

        m_dragging = !m_dragOrigins.empty();
        m_resizeHandle = ResizeHandle::None;
    }

    _Use_decl_annotations_
    SnapOutcome EditorController::UpdateDrag(double deltaX, double deltaY)
    {
        SnapOutcome outcome{};

        if (!m_dragging || m_dragOrigins.empty())
        {
            return outcome;
        }

        // The whole selection follows one rectangle, so a bank keeps its own spacing rather
        // than every control snapping to a different guide.
        auto lead = m_dragOrigins.front().Rect;
        lead.X += deltaX;
        lead.Y += deltaY;

        outcome = SnapMove(lead, OtherRects(m_selection), PageRect(), EffectiveSnap());

        auto const adjustedX = outcome.X - m_dragOrigins.front().Rect.X;
        auto const adjustedY = outcome.Y - m_dragOrigins.front().Rect.Y;

        for (auto const& origin : m_dragOrigins)
        {
            auto* const control = MutableControl(origin.Id);

            if (control != nullptr)
            {
                control->X = origin.Rect.X + adjustedX;
                control->Y = origin.Rect.Y + adjustedY;
            }
        }

        CommitCoalesced(EditNames::Move, L"move");

        return outcome;
    }

    void EditorController::EndDrag()
    {
        m_dragging = false;
        m_dragOrigins.clear();
        m_undo.EndCoalescing();
    }

    _Use_decl_annotations_
    void EditorController::BeginResize(ResizeHandle handle)
    {
        m_dragOrigins.clear();

        for (auto const* const control : SelectedControls())
        {
            m_dragOrigins.push_back({ control->Id, RectOf(*control) });
        }

        m_resizeHandle = handle;
        m_dragging = !m_dragOrigins.empty() && handle != ResizeHandle::None;
    }

    _Use_decl_annotations_
    SnapOutcome EditorController::UpdateResize(double deltaX, double deltaY, bool preserveAspect)
    {
        SnapOutcome outcome{};

        if (!m_dragging || m_resizeHandle == ResizeHandle::None || m_dragOrigins.empty())
        {
            return outcome;
        }

        auto const& lead = m_dragOrigins.front();

        auto const* const leadControl = MutableControl(lead.Id);
        auto const locked = preserveAspect || (leadControl != nullptr && leadControl->AspectLocked);

        auto const dragged = ApplyResize(lead.Rect, m_resizeHandle, deltaX, deltaY, locked);

        outcome = SnapResize(dragged, m_resizeHandle, OtherRects(m_selection), PageRect(), EffectiveSnap());

        // Turn the snapped edge back into a delta, then run the resize once more so the aspect
        // rule sees the final numbers rather than being applied to an unsnapped rectangle.
        auto snappedDeltaX = deltaX;
        auto snappedDeltaY = deltaY;

        if (MovesLeftEdge(m_resizeHandle))
        {
            snappedDeltaX = outcome.X - lead.Rect.X;
        }
        else if (MovesRightEdge(m_resizeHandle))
        {
            snappedDeltaX = outcome.X - lead.Rect.Right();
        }

        if (MovesTopEdge(m_resizeHandle))
        {
            snappedDeltaY = outcome.Y - lead.Rect.Y;
        }
        else if (MovesBottomEdge(m_resizeHandle))
        {
            snappedDeltaY = outcome.Y - lead.Rect.Bottom();
        }

        auto const leadRect = ApplyResize(lead.Rect, m_resizeHandle, snappedDeltaX, snappedDeltaY, locked);

        auto const scaleX = lead.Rect.Width > 0.0 ? leadRect.Width / lead.Rect.Width : 1.0;
        auto const scaleY = lead.Rect.Height > 0.0 ? leadRect.Height / lead.Rect.Height : 1.0;

        for (auto const& origin : m_dragOrigins)
        {
            auto* const control = MutableControl(origin.Id);

            if (control == nullptr)
            {
                continue;
            }

            if (origin.Id == lead.Id)
            {
                ApplyRect(*control, leadRect);
                continue;
            }

            // Everything else in the selection follows the same proportions, measured from the
            // handle's own corner, so resizing a bank keeps it a bank.
            EditRect rect{};

            rect.Width = std::max(MinimumControlSize, origin.Rect.Width * scaleX);
            rect.Height = std::max(MinimumControlSize, origin.Rect.Height * scaleY);
            rect.X = leadRect.X + (origin.Rect.X - lead.Rect.X) * scaleX;
            rect.Y = leadRect.Y + (origin.Rect.Y - lead.Rect.Y) * scaleY;

            ApplyRect(*control, rect);
        }

        CommitCoalesced(EditNames::Resize, L"resize");

        return outcome;
    }

    void EditorController::EndResize()
    {
        m_dragging = false;
        m_resizeHandle = ResizeHandle::None;
        m_dragOrigins.clear();
        m_undo.EndCoalescing();
    }

    _Use_decl_annotations_
    bool EditorController::NudgeSelection(double deltaX, double deltaY)
    {
        if (m_selection.empty())
        {
            return false;
        }

        auto* const page = MutablePage();

        if (page == nullptr)
        {
            return false;
        }

        for (auto& control : page->Controls)
        {
            if (IsSelected(control.Id))
            {
                control.X += deltaX;
                control.Y += deltaY;
            }
        }

        // A run of arrow presses is one entry, so taking a nudge back does not need twenty
        // presses of Ctrl+Z.
        CommitCoalesced(EditNames::Move, L"nudge");

        return true;
    }

    _Use_decl_annotations_
    bool EditorController::SetControlBounds(
        std::wstring const& id,
        double x,
        double y,
        double width,
        double height)
    {
        auto* const control = MutableControl(id);

        if (control == nullptr ||
            !std::isfinite(x) || !std::isfinite(y) ||
            !std::isfinite(width) || !std::isfinite(height))
        {
            return false;
        }

        auto const newWidth = std::max(MinimumControlSize, width);
        auto const newHeight = std::max(MinimumControlSize, height);

        if (control->X == x && control->Y == y &&
            control->Width == newWidth && control->Height == newHeight)
        {
            return false;
        }

        control->X = x;
        control->Y = y;
        control->Width = newWidth;
        control->Height = newHeight;

        CommitCoalesced(EditNames::Resize, L"bounds:" + id);

        return true;
    }

    // ---------------------------------------------------------------- arranging

    _Use_decl_annotations_
    bool EditorController::AlignSelection(AlignEdge edge)
    {
        auto const selected = SelectedControls();

        if (selected.size() < 2)
        {
            return false;
        }

        std::vector<EditRect> rects{};
        rects.reserve(selected.size());

        for (auto const* const control : selected)
        {
            rects.push_back(RectOf(*control));
        }

        auto const aligned = AlignRects(rects, edge);

        for (size_t index = 0; index < selected.size(); ++index)
        {
            auto* const control = MutableControl(selected[index]->Id);

            if (control != nullptr)
            {
                control->X = aligned[index].X;
                control->Y = aligned[index].Y;
            }
        }

        Commit(EditNames::Arrange);

        return true;
    }

    _Use_decl_annotations_
    bool EditorController::DistributeSelection(ArrangeAxis axis)
    {
        auto const selected = SelectedControls();

        if (selected.size() < 3)
        {
            return false;
        }

        std::vector<EditRect> rects{};
        rects.reserve(selected.size());

        for (auto const* const control : selected)
        {
            rects.push_back(RectOf(*control));
        }

        auto const spread = DistributeEvenly(rects, axis);

        for (size_t index = 0; index < selected.size(); ++index)
        {
            auto* const control = MutableControl(selected[index]->Id);

            if (control != nullptr)
            {
                control->X = spread[index].X;
                control->Y = spread[index].Y;
            }
        }

        Commit(EditNames::Arrange);

        return true;
    }

    _Use_decl_annotations_
    bool EditorController::SetSelectionGap(ArrangeAxis axis, double gap)
    {
        auto const selected = SelectedControls();

        if (selected.size() < 2 || !std::isfinite(gap))
        {
            return false;
        }

        std::vector<EditRect> rects{};
        rects.reserve(selected.size());

        for (auto const* const control : selected)
        {
            rects.push_back(RectOf(*control));
        }

        auto const spaced = SetGap(rects, axis, gap);

        for (size_t index = 0; index < selected.size(); ++index)
        {
            auto* const control = MutableControl(selected[index]->Id);

            if (control != nullptr)
            {
                control->X = spaced[index].X;
                control->Y = spaced[index].Y;
            }
        }

        Commit(EditNames::Arrange);

        return true;
    }

    _Use_decl_annotations_
    std::vector<double> EditorController::SelectionGaps(ArrangeAxis axis) const
    {
        auto const selected = SelectedControls();

        std::vector<EditRect> rects{};
        rects.reserve(selected.size());

        for (auto const* const control : selected)
        {
            rects.push_back(RectOf(*control));
        }

        return MeasureGaps(rects, axis);
    }

    _Use_decl_annotations_
    bool EditorController::RepeatSelection(RepeatOptions const& options)
    {
        auto* const page = MutablePage();

        if (page == nullptr || m_selection.empty())
        {
            return false;
        }

        std::vector<Control> source{};

        for (auto const* const control : SelectedControls())
        {
            source.push_back(*control);
        }

        auto const plan = BuildRepeat(source, options);

        if (page->Controls.size() + plan.Copies.size() > MaximumControlsPerPage)
        {
            return false;
        }

        for (auto const& updated : plan.UpdatedSource)
        {
            auto* const control = MutableControl(updated.Id);

            if (control != nullptr)
            {
                *control = updated;
            }
        }

        auto order = 0;

        for (auto const& control : page->Controls)
        {
            order = std::max(order, control.KeyboardOrder);
        }

        std::vector<std::wstring> newSelection = m_selection;

        for (auto copy : plan.Copies)
        {
            copy.KeyboardOrder = ++order;
            newSelection.push_back(copy.Id);
            page->Controls.push_back(std::move(copy));
        }

        m_selection = std::move(newSelection);
        Commit(EditNames::Repeat);

        return true;
    }
}
