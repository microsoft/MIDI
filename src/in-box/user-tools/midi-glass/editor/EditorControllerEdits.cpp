// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Deliberately free of pch.h and XAML, so the unit tests compile it unchanged.
//
// The other half of EditorController: control properties, what a control sends, keyboard order,
// pages, the page size and the device table.

#include "EditorController.h"
#include "ControlFactory.h"
#include "PageTemplates.h"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace glass
{
    namespace
    {
        EditRect RectOf(_In_ Control const& control) noexcept
        {
            return { control.X, control.Y, control.Width, control.Height };
        }

        bool SameValue(_In_ MessageValue const& left, _In_ MessageValue const& right) noexcept
        {
            return left.Value == right.Value && left.Scaling == right.Scaling;
        }

        bool SameDetents(_In_ MessageDetents const& left, _In_ MessageDetents const& right) noexcept
        {
            return left.Mode == right.Mode &&
                left.Scaling == right.Scaling &&
                left.Step == right.Step &&
                left.Stops == right.Stops;
        }

        // Whether writing this back would change anything at all.
        //
        // Every setter here has to answer that, because the inspector fills itself in from the
        // document and a control raises its changed event when it is filled in. Without the
        // comparison, showing a control's properties records an edit that changed nothing, and
        // an edit that changed nothing still throws away the redo branch.
        bool SameMessage(_In_ ControlMessage const& left, _In_ ControlMessage const& right) noexcept
        {
            return left.Trigger == right.Trigger &&
                left.Kind == right.Kind &&
                left.DeviceName == right.DeviceName &&
                left.GroupIndex == right.GroupIndex &&
                left.ChannelIndex == right.ChannelIndex &&
                left.Number == right.Number &&
                SameValue(left.Minimum, right.Minimum) &&
                SameValue(left.Maximum, right.Maximum) &&
                SameDetents(left.Detents, right.Detents) &&
                left.SystemExclusive == right.SystemExclusive &&
                left.RawWords == right.RawWords &&
                left.UseMidi1Protocol == right.UseMidi1Protocol &&
                left.SequenceName == right.SequenceName &&
                left.TargetPageId == right.TargetPageId &&
                left.TargetLayerId == right.TargetLayerId;
        }

        bool SameFeedback(_In_ FeedbackBinding const& left, _In_ FeedbackBinding const& right) noexcept
        {
            return left.Enabled == right.Enabled &&
                left.Kind == right.Kind &&
                left.DeviceName == right.DeviceName &&
                left.GroupIndex == right.GroupIndex &&
                left.ChannelIndex == right.ChannelIndex &&
                left.Number == right.Number;
        }
    }

    // ---------------------------------------------------------------- control properties

    _Use_decl_annotations_
    bool EditorController::SetControlLabel(std::wstring const& id, std::wstring const& label)
    {
        auto* const control = MutableControl(id);

        if (control == nullptr || label.size() > MaximumStringLength || control->Label == label)
        {
            return false;
        }

        control->Label = label;

        // Typing in a text box arrives a character at a time. One entry, not one per keystroke.
        CommitCoalesced(EditNames::Properties, L"label:" + id);

        return true;
    }

    _Use_decl_annotations_
    bool EditorController::SetControlKind(std::wstring const& id, ControlKind kind)
    {
        auto* const control = MutableControl(id);

        if (control == nullptr || control->Kind == kind)
        {
            return false;
        }

        control->Kind = kind;
        control->AspectLocked = IsSquareByNature(kind);

        // Changing a fader into a lamp leaves messages that no longer have anything to send
        // them. They are kept rather than thrown away, because changing back has to be free.
        Commit(EditNames::Properties);

        return true;
    }

    _Use_decl_annotations_
    bool EditorController::SetControlHueSlot(std::wstring const& id, int32_t slot)
    {
        auto* const control = MutableControl(id);

        if (control == nullptr || (slot != LiteralHue && (slot < 0 || slot >= HueSlotCount)))
        {
            return false;
        }

        if (control->HueSlot == slot)
        {
            return false;
        }

        control->HueSlot = slot;
        Commit(EditNames::Properties);

        return true;
    }

    _Use_decl_annotations_
    bool EditorController::SetControlAspectLocked(std::wstring const& id, bool locked)
    {
        auto* const control = MutableControl(id);

        if (control == nullptr || control->AspectLocked == locked)
        {
            return false;
        }

        control->AspectLocked = locked;
        Commit(EditNames::Properties);

        return true;
    }

    _Use_decl_annotations_
    bool EditorController::SetControlStyle(std::wstring const& id, ControlStyleOverride style)
    {
        auto* const control = MutableControl(id);

        if (control == nullptr || control->Style == style)
        {
            return false;
        }

        control->Style = style;
        Commit(EditNames::Properties);

        return true;
    }

    _Use_decl_annotations_
    bool EditorController::SetControlLabelPlacement(std::wstring const& id, LabelPlacementOverride placement)
    {
        auto* const control = MutableControl(id);

        if (control == nullptr || control->LabelPlaced == placement)
        {
            return false;
        }

        control->LabelPlaced = placement;
        Commit(EditNames::Properties);

        return true;
    }

    _Use_decl_annotations_
    bool EditorController::SetControlShowValue(std::wstring const& id, ShowValueOverride showValue)
    {
        auto* const control = MutableControl(id);

        if (control == nullptr || control->ShowValue == showValue)
        {
            return false;
        }

        control->ShowValue = showValue;
        Commit(EditNames::Properties);

        return true;
    }

    _Use_decl_annotations_
    bool EditorController::SetControlDefaultValue(std::wstring const& id, double value)
    {
        auto* const control = MutableControl(id);

        if (control == nullptr || !std::isfinite(value))
        {
            return false;
        }

        auto const clamped = std::clamp(value, 0.0, 1.0);

        if (control->DefaultValue == clamped)
        {
            return false;
        }

        control->DefaultValue = clamped;
        CommitCoalesced(EditNames::Properties, L"default:" + id);

        return true;
    }

    _Use_decl_annotations_
    bool EditorController::SetControlSendsValueOnStart(std::wstring const& id, bool sends)
    {
        auto* const control = MutableControl(id);

        if (control == nullptr || control->SendsValueOnStart == sends)
        {
            return false;
        }

        control->SendsValueOnStart = sends;
        Commit(EditNames::Properties);

        return true;
    }

    _Use_decl_annotations_
    bool EditorController::SetControlSendInterval(std::wstring const& id, int32_t milliseconds)
    {
        auto* const control = MutableControl(id);

        if (control == nullptr || milliseconds < 0)
        {
            return false;
        }

        if (control->SendIntervalMilliseconds == milliseconds)
        {
            return false;
        }

        control->SendIntervalMilliseconds = milliseconds;
        CommitCoalesced(EditNames::Properties, L"interval:" + id);

        return true;
    }

    _Use_decl_annotations_
    bool EditorController::SetControlPickup(std::wstring const& id, PickupMode pickup)
    {
        auto* const control = MutableControl(id);

        if (control == nullptr || control->Pickup == pickup)
        {
            return false;
        }

        control->Pickup = pickup;
        Commit(EditNames::Properties);

        return true;
    }

    // ---------------------------------------------------------------- what a control sends

    _Use_decl_annotations_
    bool EditorController::AddMessage(std::wstring const& id)
    {
        auto* const page = MutablePage();
        auto* const control = MutableControl(id);

        if (page == nullptr || control == nullptr || control->Messages.size() >= MaximumMessagesPerControl)
        {
            return false;
        }

        ControlMessage message{};

        // A new row starts from the one above it, because the ordinary case is a second message
        // to the same place rather than to somewhere unrelated.
        if (!control->Messages.empty())
        {
            message = control->Messages.back();
            message.Unknown = nullptr;
        }
        else
        {
            if (!m_document.Devices.empty())
            {
                message.DeviceName = m_document.Devices[0].Name;
            }

            message.Trigger = MessageTrigger::Changes;
            message.Kind = MessageKind::ControlChange;
            message.Number = NextFreeNumber(*page, MessageKind::ControlChange, 1);
        }

        control->Messages.push_back(std::move(message));
        Commit(EditNames::Messages);

        return true;
    }

    _Use_decl_annotations_
    bool EditorController::RemoveMessage(std::wstring const& id, size_t index)
    {
        auto* const control = MutableControl(id);

        if (control == nullptr || index >= control->Messages.size())
        {
            return false;
        }

        control->Messages.erase(control->Messages.begin() + static_cast<ptrdiff_t>(index));
        Commit(EditNames::Messages);

        return true;
    }

    _Use_decl_annotations_
    bool EditorController::SetMessage(std::wstring const& id, size_t index, ControlMessage const& message)
    {
        auto* const control = MutableControl(id);

        if (control == nullptr || index >= control->Messages.size())
        {
            return false;
        }

        if (message.SystemExclusive.size() > MaximumSystemExclusiveBytes)
        {
            return false;
        }

        if (SameMessage(control->Messages[index], message))
        {
            return false;
        }

        // Keys this build did not understand belong to the row, not to whatever the inspector
        // handed back, so an older build editing a newer file does not drop them.
        auto updated = message;
        updated.Unknown = control->Messages[index].Unknown;

        control->Messages[index] = std::move(updated);
        CommitCoalesced(EditNames::Messages, L"message:" + id + L":" + std::to_wstring(index));

        return true;
    }

    _Use_decl_annotations_
    bool EditorController::SetFeedback(std::wstring const& id, FeedbackBinding const& feedback)
    {
        auto* const control = MutableControl(id);

        if (control == nullptr || SameFeedback(control->Feedback, feedback))
        {
            return false;
        }

        auto updated = feedback;
        updated.Unknown = control->Feedback.Unknown;

        control->Feedback = std::move(updated);
        CommitCoalesced(EditNames::Messages, L"feedback:" + id);

        return true;
    }

    // ---------------------------------------------------------------- keyboard order

    _Use_decl_annotations_
    bool EditorController::SetKeyboardOrder(std::wstring const& id, int32_t order)
    {
        auto* const page = MutablePage();
        auto* const control = MutableControl(id);

        if (page == nullptr || control == nullptr || order < 1)
        {
            return false;
        }

        auto const target = std::min(order, static_cast<int32_t>(page->Controls.size()));
        auto const current = control->KeyboardOrder;

        if (target == current)
        {
            return false;
        }

        // Everything between the old position and the new one shifts by one, the way dragging a
        // badge feels. Setting one number and leaving duplicates behind would be worse than
        // leaving it alone.
        for (auto& other : page->Controls)
        {
            if (other.Id == id)
            {
                continue;
            }

            if (target < current && other.KeyboardOrder >= target && other.KeyboardOrder < current)
            {
                ++other.KeyboardOrder;
            }
            else if (target > current && other.KeyboardOrder > current && other.KeyboardOrder <= target)
            {
                --other.KeyboardOrder;
            }
        }

        control->KeyboardOrder = target;

        Commit(EditNames::KeyboardOrder);

        return true;
    }

    bool EditorController::RenumberKeyboardOrder()
    {
        auto* const page = MutablePage();

        if (page == nullptr || page->Controls.empty())
        {
            return false;
        }

        std::vector<size_t> order(page->Controls.size());
        std::iota(order.begin(), order.end(), size_t{ 0 });

        std::stable_sort(
            order.begin(),
            order.end(),
            [page](size_t left, size_t right)
            {
                return page->Controls[left].KeyboardOrder < page->Controls[right].KeyboardOrder;
            });

        for (size_t index = 0; index < order.size(); ++index)
        {
            page->Controls[order[index]].KeyboardOrder = static_cast<int32_t>(index) + 1;
        }

        Commit(EditNames::KeyboardOrder);

        return true;
    }

    bool EditorController::SortKeyboardOrderByPosition()
    {
        auto* const page = MutablePage();

        if (page == nullptr || page->Controls.empty())
        {
            return false;
        }

        std::vector<size_t> order(page->Controls.size());
        std::iota(order.begin(), order.end(), size_t{ 0 });

        // Reading order, in bands. Comparing Y outright would put a fader whose top edge is two
        // pixels higher than its neighbor's before the whole row beside it, which is not how
        // anyone reads a mixer.
        auto const band = std::max(24.0, m_snap.GridSize * 4.0);

        std::stable_sort(
            order.begin(),
            order.end(),
            [page, band](size_t left, size_t right)
            {
                auto const& a = page->Controls[left];
                auto const& b = page->Controls[right];

                auto const rowA = std::floor(a.Y / band);
                auto const rowB = std::floor(b.Y / band);

                if (rowA != rowB)
                {
                    return rowA < rowB;
                }

                return a.X < b.X;
            });

        for (size_t index = 0; index < order.size(); ++index)
        {
            page->Controls[order[index]].KeyboardOrder = static_cast<int32_t>(index) + 1;
        }

        Commit(EditNames::KeyboardOrder);

        return true;
    }

    // ---------------------------------------------------------------- pages

    _Use_decl_annotations_
    bool EditorController::AddPage(std::wstring const& name)
    {
        if (m_document.Pages.size() >= MaximumPagesPerLayout || name.size() > MaximumStringLength)
        {
            return false;
        }

        Page page{};
        page.Id = LayoutDocument::NewId();
        page.Name = name;

        m_document.Pages.push_back(std::move(page));

        m_pageIndex = m_document.Pages.size() - 1;
        m_selection.clear();

        Commit(EditNames::PageAdd);

        return true;
    }

    _Use_decl_annotations_
    bool EditorController::RemovePage(size_t index)
    {
        // A layout with no pages has nothing to run, so the last one stays.
        if (index >= m_document.Pages.size() || m_document.Pages.size() < 2)
        {
            return false;
        }

        m_document.Pages.erase(m_document.Pages.begin() + static_cast<ptrdiff_t>(index));

        if (m_pageIndex >= m_document.Pages.size())
        {
            m_pageIndex = m_document.Pages.size() - 1;
        }

        m_selection.clear();
        Commit(EditNames::PageRemove);

        return true;
    }

    _Use_decl_annotations_
    bool EditorController::RenamePage(size_t index, std::wstring const& name)
    {
        if (index >= m_document.Pages.size() || name.size() > MaximumStringLength)
        {
            return false;
        }

        if (m_document.Pages[index].Name == name)
        {
            return false;
        }

        m_document.Pages[index].Name = name;
        CommitCoalesced(EditNames::PageProperties, L"pagename:" + std::to_wstring(index));

        return true;
    }

    _Use_decl_annotations_
    bool EditorController::SetPageIsSharedBand(size_t index, bool shared)
    {
        if (index >= m_document.Pages.size() || m_document.Pages[index].IsSharedBand == shared)
        {
            return false;
        }

        m_document.Pages[index].IsSharedBand = shared;
        Commit(EditNames::PageProperties);

        return true;
    }

    // ---------------------------------------------------------------- layout properties

    _Use_decl_annotations_
    bool EditorController::SetLayoutName(std::wstring const& name)
    {
        if (name.empty() || name.size() > MaximumStringLength || m_document.Name == name)
        {
            return false;
        }

        m_document.Name = name;
        CommitCoalesced(EditNames::LayoutProperties, L"layoutname");

        return true;
    }

    _Use_decl_annotations_
    bool EditorController::SetLayoutDescription(std::wstring const& description)
    {
        if (description.size() > MaximumStringLength || m_document.Description == description)
        {
            return false;
        }

        m_document.Description = description;
        CommitCoalesced(EditNames::LayoutProperties, L"layoutdescription");

        return true;
    }

    _Use_decl_annotations_
    bool EditorController::SetThemeName(std::wstring const& themeName)
    {
        if (themeName.size() > MaximumStringLength || m_document.ThemeName == themeName)
        {
            return false;
        }

        m_document.ThemeName = themeName;
        Commit(EditNames::LayoutProperties);

        return true;
    }

    _Use_decl_annotations_
    bool EditorController::SetSuppressAllStartupValues(bool suppress)
    {
        if (m_document.SuppressAllStartupValues == suppress)
        {
            return false;
        }

        m_document.SuppressAllStartupValues = suppress;
        Commit(EditNames::LayoutProperties);

        return true;
    }

    // ---------------------------------------------------------------- the page size

    _Use_decl_annotations_
    size_t EditorController::CountOutsideAfterResize(PageResizeRequest const& request) const
    {
        auto const transform = ComputePageResize(
            m_document.PageWidth,
            m_document.PageHeight,
            request.NewWidth,
            request.NewHeight,
            request.Anchor,
            request.ScaleContents);

        std::vector<EditRect> rects{};

        for (auto const& page : m_document.Pages)
        {
            for (auto const& control : page.Controls)
            {
                rects.push_back(RectOf(control));
            }
        }

        return CountOutsidePage(rects, transform, request.NewWidth, request.NewHeight);
    }

    _Use_decl_annotations_
    bool EditorController::ResizePage(PageResizeRequest const& request)
    {
        if (request.NewWidth < PixelQuantum || request.NewHeight < PixelQuantum)
        {
            return false;
        }

        if (request.NewWidth == m_document.PageWidth && request.NewHeight == m_document.PageHeight)
        {
            return false;
        }

        auto const transform = ComputePageResize(
            m_document.PageWidth,
            m_document.PageHeight,
            request.NewWidth,
            request.NewHeight,
            request.Anchor,
            request.ScaleContents);

        for (auto& page : m_document.Pages)
        {
            for (auto& control : page.Controls)
            {
                auto const moved = ApplyTransform(RectOf(control), transform);

                control.X = moved.X;
                control.Y = moved.Y;
                control.Width = std::max(MinimumControlSize, moved.Width);
                control.Height = std::max(MinimumControlSize, moved.Height);
            }
        }

        m_document.PageWidth = request.NewWidth;
        m_document.PageHeight = request.NewHeight;

        // The canvas is the page plus room around it, worked out when it is drawn. The stored
        // numbers only have to be at least the page.
        m_document.CanvasWidth = std::max(m_document.CanvasWidth, request.NewWidth);
        m_document.CanvasHeight = std::max(m_document.CanvasHeight, request.NewHeight);

        Commit(EditNames::PageSize);

        return true;
    }

    EditRect EditorController::WorkArea() const
    {
        std::vector<EditRect> rects{};

        auto const* const page = CurrentPage();

        if (page != nullptr)
        {
            for (auto const& control : page->Controls)
            {
                rects.push_back(RectOf(control));
            }
        }

        return ComputeWorkArea(m_document.PageWidth, m_document.PageHeight, rects);
    }

    std::vector<Control const*> EditorController::ControlsOutsidePage() const
    {
        std::vector<Control const*> outside{};

        auto const* const page = CurrentPage();

        if (page == nullptr)
        {
            return outside;
        }

        for (auto const& control : page->Controls)
        {
            if (IsOutsidePage(RectOf(control), m_document.PageWidth, m_document.PageHeight))
            {
                outside.push_back(&control);
            }
        }

        return outside;
    }

    // ---------------------------------------------------------------- devices

    _Use_decl_annotations_
    bool EditorController::AddDevice(DeviceEntry const& device)
    {
        if (device.Name.empty() ||
            device.Name.size() > MaximumStringLength ||
            m_document.Devices.size() >= MaximumDevicesPerLayout ||
            m_document.FindDevice(device.Name) != nullptr)
        {
            return false;
        }

        m_document.Devices.push_back(device);
        Commit(EditNames::Devices);

        return true;
    }

    _Use_decl_annotations_
    bool EditorController::RemoveDevice(std::wstring const& name)
    {
        auto const found = std::find_if(
            m_document.Devices.begin(),
            m_document.Devices.end(),
            [&name](DeviceEntry const& entry) { return entry.Name == name; });

        if (found == m_document.Devices.end())
        {
            return false;
        }

        m_document.Devices.erase(found);

        // Messages keep the name they pointed at. A destination that no longer resolves is
        // reported at run time; silently rewriting two hundred controls would be worse.
        Commit(EditNames::Devices);

        return true;
    }

    _Use_decl_annotations_
    bool EditorController::RenameDevice(std::wstring const& oldName, std::wstring const& newName)
    {
        if (newName.empty() || newName.size() > MaximumStringLength || oldName == newName)
        {
            return false;
        }

        if (m_document.FindDevice(newName) != nullptr)
        {
            return false;
        }

        auto const found = std::find_if(
            m_document.Devices.begin(),
            m_document.Devices.end(),
            [&oldName](DeviceEntry const& entry) { return entry.Name == oldName; });

        if (found == m_document.Devices.end())
        {
            return false;
        }

        found->Name = newName;

        // Destinations are named rather than wired, which is the whole point of the table:
        // moving a layout to different hardware is one edit, not a hunt through the controls.
        for (auto& page : m_document.Pages)
        {
            for (auto& control : page.Controls)
            {
                for (auto& message : control.Messages)
                {
                    if (message.DeviceName == oldName)
                    {
                        message.DeviceName = newName;
                    }
                }

                if (control.Feedback.DeviceName == oldName)
                {
                    control.Feedback.DeviceName = newName;
                }
            }
        }

        if (m_document.Tempo.DeviceName == oldName)
        {
            m_document.Tempo.DeviceName = newName;
        }

        Commit(EditNames::Devices);

        return true;
    }
}
