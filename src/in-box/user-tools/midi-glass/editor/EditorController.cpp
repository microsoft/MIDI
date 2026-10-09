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
#include "MackieControl.h"
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

        // A control a newer version made is never selected, which keeps every edit away from it.
        bool IsPlaceholder(_In_ Page const* page, _In_ std::wstring const& id) noexcept
        {
            if (page == nullptr)
            {
                return false;
            }

            return std::any_of(page->Controls.begin(), page->Controls.end(), [&id](Control const& control)
                {
                    return control.Kind == ControlKind::Placeholder && control.Id == id;
                });
        }

        // One thing to arrange, and the controls that move with it.
        struct ArrangeBlock
        {
            EditRect Bounds{};
            std::vector<std::wstring> Ids{};
            bool Locked{ false };
        };

        // In the page's own order, so an arrangement gives the same answer whichever order the
        // controls were picked in.
        std::vector<Control const*> ControlsWithIds(_In_ Page const* page, _In_ std::vector<std::wstring> const& ids)
        {
            std::vector<Control const*> controls{};

            if (page == nullptr)
            {
                return controls;
            }

            for (auto const& control : page->Controls)
            {
                if (std::find(ids.begin(), ids.end(), control.Id) != ids.end())
                {
                    controls.push_back(&control);
                }
            }

            return controls;
        }

        // A group is one block. When every control is in the same group, each is its own block.
        std::vector<ArrangeBlock> BlocksOf(_In_ std::vector<Control const*> const& controls)
        {
            std::vector<ArrangeBlock> blocks{};

            if (controls.empty())
            {
                return blocks;
            }

            auto const& firstGroup = controls.front()->GroupId;

            auto const oneGroup = !firstGroup.empty() &&
                std::all_of(controls.begin(), controls.end(),
                    [&firstGroup](Control const* control) { return control->GroupId == firstGroup; });

            std::vector<std::wstring> keys{};

            for (auto const* const control : controls)
            {
                auto const& key = (oneGroup || control->GroupId.empty()) ? control->Id : control->GroupId;
                auto const found = std::find(keys.begin(), keys.end(), key);

                if (found == keys.end())
                {
                    keys.push_back(key);
                    blocks.push_back({ RectOf(*control), { control->Id }, control->Locked });
                    continue;
                }

                auto& block = blocks[static_cast<size_t>(found - keys.begin())];

                auto const left = std::min(block.Bounds.X, control->X);
                auto const top = std::min(block.Bounds.Y, control->Y);
                auto const right = std::max(block.Bounds.Right(), control->X + control->Width);
                auto const bottom = std::max(block.Bounds.Bottom(), control->Y + control->Height);

                block.Bounds = { left, top, right - left, bottom - top };
                block.Ids.push_back(control->Id);
                block.Locked = block.Locked || control->Locked;
            }

            return blocks;
        }

        std::vector<EditRect> BoundsOf(_In_ std::vector<ArrangeBlock> const& blocks)
        {
            std::vector<EditRect> rects{};
            rects.reserve(blocks.size());

            for (auto const& block : blocks)
            {
                rects.push_back(block.Bounds);
            }

            return rects;
        }

        // Every member keeps its place inside its block. A block holding a locked control is
        // arranged against and never moved, so a group never comes apart around one.
        bool PlaceBlocks(
            _Inout_ Page& page,
            _In_ std::vector<ArrangeBlock> const& blocks,
            _In_ std::vector<EditRect> const& placed)
        {
            auto moved = false;

            for (size_t index = 0; index < blocks.size() && index < placed.size(); ++index)
            {
                auto const& block = blocks[index];

                if (block.Locked || (placed[index].X == block.Bounds.X && placed[index].Y == block.Bounds.Y))
                {
                    continue;
                }

                for (auto& control : page.Controls)
                {
                    if (std::find(block.Ids.begin(), block.Ids.end(), control.Id) != block.Ids.end())
                    {
                        control.X = placed[index].X + (control.X - block.Bounds.X);
                        control.Y = placed[index].Y + (control.Y - block.Bounds.Y);
                        moved = true;
                    }
                }
            }

            return moved;
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
        if (m_batchDepth > 0)
        {
            m_undo.CommitCoalesced(m_document, name, L"batch:" + std::to_wstring(m_batchNumber));
            m_dirty = true;
            return;
        }

        m_undo.Commit(m_document, name);
        m_dirty = true;
    }

    void EditorController::CommitCoalesced(_In_ wchar_t const* name, _In_ std::wstring const& key)
    {
        m_undo.CommitCoalesced(
            m_document, name, m_batchDepth > 0 ? L"batch:" + std::to_wstring(m_batchNumber) : key);
        m_dirty = true;
    }

    void EditorController::BeginEditBatch()
    {
        if (m_batchDepth++ == 0)
        {
            ++m_batchNumber;

            // Typing that was still being gathered into one entry is its own edit, not part of
            // this one.
            m_undo.EndCoalescing();
        }
    }

    void EditorController::EndEditBatch()
    {
        if (m_batchDepth > 0 && --m_batchDepth == 0)
        {
            m_undo.EndCoalescing();
        }
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

        if (!id.empty() && !IsPlaceholder(CurrentPage(), id))
        {
            m_selection.push_back(id);
        }
    }

    _Use_decl_annotations_
    void EditorController::AddToSelection(std::wstring const& id)
    {
        if (!id.empty() && !IsSelected(id) && !IsPlaceholder(CurrentPage(), id))
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
            if (!IsPlaceholder(CurrentPage(), id))
            {
                m_selection.push_back(id);
            }
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

        // A locked control is left out, the same as a click on the page leaves it out.
        for (auto const& control : page->Controls)
        {
            if (!control.Locked)
            {
                m_selection.push_back(control.Id);
            }
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
            if (IsOutsidePage(RectOf(control), m_document.PageWidth, m_document.PageHeight) &&
                control.Kind != ControlKind::Placeholder)
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
            if (!control.Locked && Intersects(area, RectOf(control)))
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

    EditRect EditorController::SelectionBounds() const
    {
        auto const selected = SelectedControls();

        if (selected.empty())
        {
            return {};
        }

        auto left = selected.front()->X;
        auto top = selected.front()->Y;
        auto right = selected.front()->X + selected.front()->Width;
        auto bottom = selected.front()->Y + selected.front()->Height;

        for (auto const* const control : selected)
        {
            left = std::min(left, control->X);
            top = std::min(top, control->Y);
            right = std::max(right, control->X + control->Width);
            bottom = std::max(bottom, control->Y + control->Height);
        }

        return { left, top, right - left, bottom - top };
    }

    // ---------------------------------------------------------------- locking

    _Use_decl_annotations_
    bool EditorController::SetSelectionLocked(bool locked)
    {
        auto changed = false;

        for (auto const& id : m_selection)
        {
            auto* const control = MutableControl(id);

            if (control != nullptr && control->Locked != locked)
            {
                control->Locked = locked;
                changed = true;
            }
        }

        if (!changed)
        {
            return false;
        }

        Commit(locked ? EditNames::Lock : EditNames::Unlock);

        return true;
    }

    bool EditorController::SelectionIsLocked() const
    {
        auto const selected = SelectedControls();

        return !selected.empty() &&
            std::all_of(selected.begin(), selected.end(), [](Control const* control) { return control->Locked; });
    }

    bool EditorController::SelectionHasLocked() const
    {
        auto const selected = SelectedControls();

        return std::any_of(selected.begin(), selected.end(), [](Control const* control) { return control->Locked; });
    }

    // ---------------------------------------------------------------- groups

    bool EditorController::GroupSelection()
    {
        auto const selected = SelectedControls();

        if (selected.size() < 2 || SelectionIsOneGroup())
        {
            return false;
        }

        // A named group taken in whole keeps its name, so adding one more control to Drums still
        // leaves a group called Drums.
        std::wstring name{};

        for (auto const* const control : selected)
        {
            if (name.empty() && !control->GroupId.empty() && IsWholeGroupSelected(control->GroupId))
            {
                name = GroupName(control->GroupId);
            }
        }

        auto const group = LayoutDocument::NewId();

        for (auto const* const control : selected)
        {
            if (auto* const member = MutableControl(control->Id))
            {
                member->GroupId = group;
            }
        }

        if (auto* const page = MutablePage())
        {
            PruneControlGroups(*page);

            if (!name.empty())
            {
                page->Groups.push_back({ group, name, nullptr });
            }
        }

        Commit(EditNames::Group);

        return true;
    }

    bool EditorController::UngroupSelection()
    {
        auto changed = false;

        for (auto const* const control : SelectedControls())
        {
            if (auto* const member = MutableControl(control->Id); member != nullptr && !member->GroupId.empty())
            {
                member->GroupId.clear();
                changed = true;
            }
        }

        if (changed)
        {
            if (auto* const page = MutablePage())
            {
                PruneControlGroups(*page);
            }

            Commit(EditNames::Ungroup);
        }

        return changed;
    }

    bool EditorController::SelectionIsOneGroup() const
    {
        auto const selected = SelectedControls();

        if (selected.size() < 2 || selected.front()->GroupId.empty())
        {
            return false;
        }

        auto const& group = selected.front()->GroupId;

        for (auto const* const control : selected)
        {
            if (control->GroupId != group)
            {
                return false;
            }
        }

        // And none of the group left out.
        auto const* const page = CurrentPage();

        for (auto const& control : page->Controls)
        {
            if (control.GroupId == group && !IsSelected(control.Id))
            {
                return false;
            }
        }

        return true;
    }

    bool EditorController::SelectionHasGroup() const
    {
        for (auto const* const control : SelectedControls())
        {
            if (!control->GroupId.empty())
            {
                return true;
            }
        }

        return false;
    }

    std::wstring EditorController::SelectedGroupId() const
    {
        if (!SelectionIsOneGroup())
        {
            return {};
        }

        return SelectedControls().front()->GroupId;
    }

    _Use_decl_annotations_
    std::wstring EditorController::GroupName(std::wstring const& groupId) const
    {
        auto const* const page = CurrentPage();

        if (page == nullptr || groupId.empty())
        {
            return {};
        }

        auto const* const group = page->FindGroup(groupId);

        return group != nullptr ? group->Name : std::wstring{};
    }

    _Use_decl_annotations_
    int32_t EditorController::GroupNumber(std::wstring const& groupId) const
    {
        for (auto const& row : OutlineRows())
        {
            if (row.IsGroupHeading && row.Id == groupId)
            {
                return row.GroupNumber;
            }
        }

        return 0;
    }

    _Use_decl_annotations_
    bool EditorController::SetGroupName(std::wstring const& groupId, std::wstring const& name)
    {
        auto* const page = MutablePage();

        if (page == nullptr ||
            groupId.empty() ||
            name.size() > MaximumStringLength ||
            GroupName(groupId) == name ||
            std::none_of(page->Controls.begin(), page->Controls.end(),
                [&groupId](Control const& control) { return control.GroupId == groupId; }))
        {
            return false;
        }

        if (auto* const group = page->FindGroup(groupId))
        {
            group->Name = name;
        }
        else
        {
            page->Groups.push_back({ groupId, name, nullptr });
        }

        // A name cleared back to nothing leaves nothing to keep.
        PruneControlGroups(*page);

        CommitCoalesced(EditNames::RenameGroup, L"group:" + groupId);

        return true;
    }

    _Use_decl_annotations_
    void EditorController::SelectGroupOf(std::wstring const& id)
    {
        SelectOnly(id);
        ExpandSelectionToGroups();
    }

    _Use_decl_annotations_
    void EditorController::ToggleGroupOf(std::wstring const& id)
    {
        auto const* const control = m_document.FindControl(id);
        auto const* const page = CurrentPage();

        if (control == nullptr || page == nullptr || control->GroupId.empty())
        {
            ToggleSelected(id);
            return;
        }

        auto everyMember = true;

        for (auto const& member : page->Controls)
        {
            if (member.GroupId == control->GroupId && !IsSelected(member.Id))
            {
                everyMember = false;
                break;
            }
        }

        // All of it on comes off together; anything less is finished off.
        for (auto const& member : page->Controls)
        {
            if (member.GroupId != control->GroupId)
            {
                continue;
            }

            if (everyMember)
            {
                m_selection.erase(std::remove(m_selection.begin(), m_selection.end(), member.Id), m_selection.end());
            }
            else
            {
                AddToSelection(member.Id);
            }
        }
    }

    void EditorController::ExpandSelectionToGroups()
    {
        auto const* const page = CurrentPage();

        if (page == nullptr)
        {
            return;
        }

        std::vector<std::wstring> groups{};

        for (auto const* const control : SelectedControls())
        {
            if (!control->GroupId.empty() &&
                std::find(groups.begin(), groups.end(), control->GroupId) == groups.end())
            {
                groups.push_back(control->GroupId);
            }
        }

        for (auto const& control : page->Controls)
        {
            if (!control.GroupId.empty() &&
                std::find(groups.begin(), groups.end(), control.GroupId) != groups.end())
            {
                AddToSelection(control.Id);
            }
        }
    }

    _Use_decl_annotations_
    void EditorController::AddGroupToSelection(std::wstring const& groupId)
    {
        auto const* const page = CurrentPage();

        if (page == nullptr || groupId.empty())
        {
            return;
        }

        for (auto const& control : page->Controls)
        {
            if (control.GroupId == groupId)
            {
                AddToSelection(control.Id);
            }
        }
    }

    _Use_decl_annotations_
    bool EditorController::IsWholeGroupSelected(std::wstring const& groupId) const
    {
        auto const* const page = CurrentPage();

        if (page == nullptr || groupId.empty())
        {
            return false;
        }

        size_t members{ 0 };

        for (auto const& control : page->Controls)
        {
            if (control.GroupId != groupId)
            {
                continue;
            }

            if (!IsSelected(control.Id))
            {
                return false;
            }

            ++members;
        }

        return members > 0;
    }

    std::vector<OutlineRow> EditorController::OutlineRows() const
    {
        std::vector<OutlineRow> rows{};

        auto const* const page = CurrentPage();

        if (page == nullptr)
        {
            return rows;
        }

        std::vector<Control const*> ordered{};
        ordered.reserve(page->Controls.size());

        for (auto const& control : page->Controls)
        {
            ordered.push_back(&control);
        }

        std::stable_sort(
            ordered.begin(),
            ordered.end(),
            [](Control const* left, Control const* right)
            {
                return left->KeyboardOrder < right->KeyboardOrder;
            });

        std::vector<std::wstring> listed{};
        int32_t groupNumber{ 0 };

        for (auto const* const control : ordered)
        {
            if (!control->GroupId.empty())
            {
                // Already listed under its heading.
                if (std::find(listed.begin(), listed.end(), control->GroupId) != listed.end())
                {
                    continue;
                }

                std::vector<Control const*> members{};

                for (auto const* const candidate : ordered)
                {
                    if (candidate->GroupId == control->GroupId)
                    {
                        members.push_back(candidate);
                    }
                }

                if (members.size() > 1)
                {
                    listed.push_back(control->GroupId);
                    ++groupNumber;

                    auto const name = GroupName(control->GroupId);

                    rows.push_back({ control->GroupId, true, 0, groupNumber, members.size(), name });

                    for (auto const* const member : members)
                    {
                        rows.push_back({ member->Id, false, 1, groupNumber, 0, name });
                    }

                    continue;
                }
            }

            rows.push_back({ control->Id, false, 0, 0, 0 });
        }

        return rows;
    }

    _Use_decl_annotations_
    bool EditorController::SetSelectionBounds(double x, double y, double width, double height)
    {
        if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(width) || !std::isfinite(height) ||
            SelectionHasLocked())
        {
            return false;
        }

        auto const from = SelectionBounds();

        if (from.Width <= 0.0 || from.Height <= 0.0)
        {
            return false;
        }

        EditRect const to{ x, y, std::max(MinimumControlSize, width), std::max(MinimumControlSize, height) };

        if (to.X == from.X && to.Y == from.Y && to.Width == from.Width && to.Height == from.Height)
        {
            return false;
        }

        std::vector<DragOrigin> origins{};

        for (auto const* const control : SelectedControls())
        {
            origins.push_back({ control->Id, RectOf(*control) });
        }

        PlaceWithin(origins, from, to);

        // Typing a number arrives a character at a time. One entry for the run.
        CommitCoalesced(EditNames::Resize, L"selectionbounds");

        return true;
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

        WaitForMackieFunction(control, m_document);

        auto const settings = EffectiveSnap();

        if (settings.GridEnabled)
        {
            control.X = SnapToGrid(control.X, settings.GridSize);
            control.Y = SnapToGrid(control.Y, settings.GridSize);
        }

        auto const id = control.Id;

        page->Controls.push_back(std::move(control));

        // A grouping panel dropped on top of the controls it is meant to frame would hide them,
        // and nobody drops one meaning that. It goes to the back on the way in; the z-order
        // commands move it from there.
        if (kind == ControlKind::Panel && page->Controls.size() > 1)
        {
            auto added = std::move(page->Controls.back());
            page->Controls.pop_back();
            page->Controls.insert(page->Controls.begin(), std::move(added));
        }

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

        PruneControlGroups(*page);

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

        auto const regrouped = RegroupCopies(copies);

        for (auto& copy : copies)
        {
            page->Controls.push_back(std::move(copy));
        }

        NameCopiedGroups(*page, page->Groups, regrouped);

        m_selection = std::move(newSelection);
        Commit(EditNames::Duplicate);

        return true;
    }

    // ---------------------------------------------------------------- moving and resizing

    void EditorController::BeginDrag()
    {
        m_dragOrigins.clear();

        // A locked control stays where it is while the rest of the selection moves.
        for (auto const* const control : SelectedControls())
        {
            if (!control->Locked)
            {
                m_dragOrigins.push_back({ control->Id, RectOf(*control) });
            }
        }

        m_dragging = !m_dragOrigins.empty();
        m_resizeHandle = ResizeHandle::None;
    }

    _Use_decl_annotations_
    SnapOutcome EditorController::UpdateDrag(double deltaX, double deltaY, bool straightLine)
    {
        SnapOutcome outcome{};

        if (!m_dragging || m_dragOrigins.empty())
        {
            return outcome;
        }

        auto const holdY = straightLine && std::abs(deltaX) >= std::abs(deltaY);
        auto const holdX = straightLine && !holdY;

        if (holdY)
        {
            deltaY = 0.0;
        }

        if (holdX)
        {
            deltaX = 0.0;
        }

        // The whole selection follows one rectangle, so a bank keeps its own spacing rather
        // than every control snapping to a different guide.
        auto lead = m_dragOrigins.front().Rect;
        lead.X += deltaX;
        lead.Y += deltaY;

        outcome = SnapMove(lead, OtherRects(m_selection), PageRect(), EffectiveSnap());

        // A guide may pull on the axis that is being held. A straight line stays straight, and
        // a guide for an axis that cannot move would only be a promise it cannot keep.
        if (holdX || holdY)
        {
            if (holdX)
            {
                outcome.X = m_dragOrigins.front().Rect.X;
            }

            if (holdY)
            {
                outcome.Y = m_dragOrigins.front().Rect.Y;
            }

            auto const heldAxis = holdX ? GuideAxis::Vertical : GuideAxis::Horizontal;

            outcome.Guides.erase(
                std::remove_if(outcome.Guides.begin(), outcome.Guides.end(),
                    [heldAxis](SnapGuide const& guide) { return guide.Axis == heldAxis; }),
                outcome.Guides.end());
        }

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

        m_resizeBounds = SelectionBounds();
        m_resizeHandle = handle;

        // The box is drawn around the whole selection, so one locked control holds all of it.
        m_dragging = !m_dragOrigins.empty() && handle != ResizeHandle::None && !SelectionHasLocked();
    }

    _Use_decl_annotations_
    SnapOutcome EditorController::UpdateResize(double deltaX, double deltaY, bool preserveAspect)
    {
        SnapOutcome outcome{};

        if (!m_dragging || m_resizeHandle == ResizeHandle::None || m_dragOrigins.empty())
        {
            return outcome;
        }

        // Several controls are resized as one box drawn around them all.
        auto const many = m_dragOrigins.size() > 1;

        auto const& lead = m_dragOrigins.front();
        auto const base = many ? m_resizeBounds : lead.Rect;

        auto const* const leadControl = MutableControl(lead.Id);
        auto const locked = preserveAspect || (!many && leadControl != nullptr && leadControl->AspectLocked);

        auto const dragged = ApplyResize(base, m_resizeHandle, deltaX, deltaY, locked);

        outcome = SnapResize(dragged, m_resizeHandle, OtherRects(m_selection), PageRect(), EffectiveSnap());

        // Turn the snapped edge back into a delta, then run the resize once more so the aspect
        // rule sees the final numbers rather than being applied to an unsnapped rectangle.
        auto snappedDeltaX = deltaX;
        auto snappedDeltaY = deltaY;

        if (MovesLeftEdge(m_resizeHandle))
        {
            snappedDeltaX = outcome.X - base.X;
        }
        else if (MovesRightEdge(m_resizeHandle))
        {
            snappedDeltaX = outcome.X - base.Right();
        }

        if (MovesTopEdge(m_resizeHandle))
        {
            snappedDeltaY = outcome.Y - base.Y;
        }
        else if (MovesBottomEdge(m_resizeHandle))
        {
            snappedDeltaY = outcome.Y - base.Bottom();
        }

        auto const resized = ApplyResize(base, m_resizeHandle, snappedDeltaX, snappedDeltaY, locked);

        if (many)
        {
            PlaceWithin(m_dragOrigins, base, resized);
        }
        else if (auto* const control = MutableControl(lead.Id))
        {
            ApplyRect(*control, resized);
        }

        CommitCoalesced(EditNames::Resize, L"resize");

        return outcome;
    }

    _Use_decl_annotations_
    void EditorController::PlaceWithin(
        std::vector<DragOrigin> const& origins,
        EditRect const& from,
        EditRect const& to)
    {
        auto const scaleX = from.Width > 0.0 ? to.Width / from.Width : 1.0;
        auto const scaleY = from.Height > 0.0 ? to.Height / from.Height : 1.0;

        for (auto const& origin : origins)
        {
            auto* const control = MutableControl(origin.Id);

            if (control == nullptr)
            {
                continue;
            }

            auto width = origin.Rect.Width * scaleX;
            auto height = origin.Rect.Height * scaleY;

            if (control->AspectLocked)
            {
                auto const even = std::min(scaleX, scaleY);

                width = origin.Rect.Width * even;
                height = origin.Rect.Height * even;
            }

            width = std::max(MinimumControlSize, width);
            height = std::max(MinimumControlSize, height);

            auto const centerX = to.X + ((origin.Rect.CenterX() - from.X) * scaleX);
            auto const centerY = to.Y + ((origin.Rect.CenterY() - from.Y) * scaleY);

            ApplyRect(*control, { centerX - (width / 2.0), centerY - (height / 2.0), width, height });
        }
    }

    void EditorController::EndResize()
    {
        m_dragging = false;
        m_resizeHandle = ResizeHandle::None;
        m_dragOrigins.clear();
        m_undo.EndCoalescing();
    }

    void EditorController::EndCoalescing() noexcept
    {
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

        auto moved = false;

        for (auto& control : page->Controls)
        {
            if (IsSelected(control.Id) && !control.Locked)
            {
                control.X += deltaX;
                control.Y += deltaY;
                moved = true;
            }
        }

        if (!moved)
        {
            return false;
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

        if (control == nullptr || control->Locked ||
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
        auto* const page = MutablePage();
        auto const blocks = BlocksOf(SelectedControls());

        // A locked control is lined up against, never moved.
        if (page == nullptr || blocks.size() < 2 || !PlaceBlocks(*page, blocks, AlignRects(BoundsOf(blocks), edge)))
        {
            return false;
        }

        Commit(EditNames::Arrange);

        return true;
    }

    _Use_decl_annotations_
    bool EditorController::ChangeZOrder(ZOrderMove move)
    {
        auto* const page = MutablePage();

        if (page == nullptr || page->Controls.size() < 2 || m_selection.empty())
        {
            return false;
        }

        std::vector<size_t> selected{};

        for (size_t index = 0; index < page->Controls.size(); ++index)
        {
            if (IsSelected(page->Controls[index].Id))
            {
                selected.push_back(index);
            }
        }

        if (selected.empty() || selected.size() == page->Controls.size())
        {
            return false;
        }

        auto const order = ReorderForZ(page->Controls.size(), selected, move);

        std::vector<Control> reordered{};
        reordered.reserve(order.size());

        bool changed{ false };

        for (size_t index = 0; index < order.size(); ++index)
        {
            changed = changed || order[index] != index;
            reordered.push_back(page->Controls[order[index]]);
        }

        if (!changed)
        {
            return false;
        }

        page->Controls = std::move(reordered);

        Commit(EditNames::ZOrder);

        return true;
    }

    _Use_decl_annotations_
    bool EditorController::DistributeSelection(ArrangeAxis axis)
    {
        auto* const page = MutablePage();
        auto const blocks = BlocksOf(SelectedControls());

        if (page == nullptr || blocks.size() < 3 || !PlaceBlocks(*page, blocks, DistributeEvenly(BoundsOf(blocks), axis)))
        {
            return false;
        }

        Commit(EditNames::Arrange);

        return true;
    }

    _Use_decl_annotations_
    bool EditorController::SetSelectionGap(ArrangeAxis axis, double gap)
    {
        return SpaceControls(m_selection, axis, gap);
    }

    _Use_decl_annotations_
    bool EditorController::SpaceControls(std::vector<std::wstring> const& ids, ArrangeAxis axis, double gap)
    {
        auto* const page = MutablePage();
        auto const blocks = BlocksOf(ControlsWithIds(page, ids));

        if (page == nullptr ||
            blocks.size() < 2 ||
            !std::isfinite(gap) ||
            !PlaceBlocks(*page, blocks, SetGap(BoundsOf(blocks), axis, gap)))
        {
            return false;
        }

        Commit(EditNames::Arrange);

        return true;
    }

    size_t EditorController::SelectionBlockCount() const
    {
        return BlocksOf(SelectedControls()).size();
    }

    _Use_decl_annotations_
    std::vector<double> EditorController::SelectionGaps(ArrangeAxis axis) const
    {
        return MeasureGaps(BoundsOf(BlocksOf(SelectedControls())), axis);
    }

    std::optional<SpacingReadout> EditorController::SelectionSpacing() const
    {
        return ReadSpacing(BoundsOf(BlocksOf(SelectedControls())));
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

        NameCopiedGroups(*page, page->Groups, plan.Regrouped);

        m_selection = std::move(newSelection);
        Commit(EditNames::Repeat);

        return true;
    }
}
