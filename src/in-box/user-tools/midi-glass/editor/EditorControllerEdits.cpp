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
#include "LayoutSerializer.h"

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
                left.TargetLayerId == right.TargetLayerId &&
                left.Axis == right.Axis;
        }

        bool SameFeedback(_In_ FeedbackBinding const& left, _In_ FeedbackBinding const& right) noexcept
        {
            return left.Enabled == right.Enabled &&
                left.Mode == right.Mode &&
                left.Kind == right.Kind &&
                left.DeviceName == right.DeviceName &&
                left.GroupIndex == right.GroupIndex &&
                left.ChannelIndex == right.ChannelIndex &&
                left.Number == right.Number &&
                left.MatchesChannel == right.MatchesChannel &&
                left.TempoControlId == right.TempoControlId &&
                left.HoldMilliseconds == right.HoldMilliseconds;
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
    bool EditorController::SetControlLabelStyle(std::wstring const& id, LabelStyle const& style)
    {
        auto* const control = MutableControl(id);

        if (control == nullptr)
        {
            return false;
        }

        auto const& current = control->LabelLook;

        if (current.FontFamily == style.FontFamily &&
            current.FontSize == style.FontSize &&
            current.FontWeight == style.FontWeight &&
            current.Italic == style.Italic &&
            current.Underline == style.Underline &&
            current.Color == style.Color &&
            current.Wrap == style.Wrap &&
            current.WidthPercent == style.WidthPercent)
        {
            return false;
        }

        auto const unknown = current.Unknown;

        // The box has its own setter and its own gesture, so a change of font never moves the
        // label somebody dragged into place.
        auto const boxX = current.BoxX;
        auto const boxY = current.BoxY;
        auto const boxWidth = current.BoxWidth;
        auto const boxHeight = current.BoxHeight;

        control->LabelLook = style;
        control->LabelLook.FontSize = std::clamp(style.FontSize, 0.0, 200.0);
        control->LabelLook.FontWeight = std::clamp(style.FontWeight, 0, 1000);
        control->LabelLook.WidthPercent = std::clamp(style.WidthPercent, 10.0, 400.0);
        control->LabelLook.Color = SanitizeStoredString(style.Color);
        control->LabelLook.Unknown = unknown;

        control->LabelLook.BoxX = boxX;
        control->LabelLook.BoxY = boxY;
        control->LabelLook.BoxWidth = boxWidth;
        control->LabelLook.BoxHeight = boxHeight;

        Commit(EditNames::Properties);

        return true;
    }

    _Use_decl_annotations_
    bool EditorController::SetControlLabelBox(
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

        auto const newX = std::clamp(x, -MaximumLabelBoxExtent, MaximumLabelBoxExtent);
        auto const newY = std::clamp(y, -MaximumLabelBoxExtent, MaximumLabelBoxExtent);
        auto const newWidth = std::clamp(width, MinimumLabelBoxSize, MaximumLabelBoxExtent);
        auto const newHeight = std::clamp(height, MinimumLabelBoxSize, MaximumLabelBoxExtent);

        auto& look = control->LabelLook;

        if (look.BoxX == newX && look.BoxY == newY &&
            look.BoxWidth == newWidth && look.BoxHeight == newHeight &&
            control->LabelPlaced == LabelPlacementOverride::Custom)
        {
            return false;
        }

        look.BoxX = newX;
        look.BoxY = newY;
        look.BoxWidth = newWidth;
        look.BoxHeight = newHeight;

        // The box and the placement say the same thing, so they move together. A file with one
        // and not the other would read as two answers to one question.
        control->LabelPlaced = LabelPlacementOverride::Custom;

        CommitCoalesced(EditNames::Properties, L"labelbox:" + id);

        return true;
    }

    _Use_decl_annotations_
    bool EditorController::ClearControlLabelBox(std::wstring const& id)
    {
        auto* const control = MutableControl(id);

        if (control == nullptr || !control->LabelLook.HasBox())
        {
            return false;
        }

        control->LabelLook.BoxX = 0.0;
        control->LabelLook.BoxY = 0.0;
        control->LabelLook.BoxWidth = 0.0;
        control->LabelLook.BoxHeight = 0.0;

        if (control->LabelPlaced == LabelPlacementOverride::Custom)
        {
            control->LabelPlaced = LabelPlacementOverride::UseTheme;
        }

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
    bool EditorController::SetControlReturnsToDefault(std::wstring const& id, bool returns)
    {
        auto* const control = MutableControl(id);

        if (control == nullptr || control->ReturnsToDefault == returns)
        {
            return false;
        }

        control->ReturnsToDefault = returns;
        Commit(EditNames::Properties);

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
    bool EditorController::SetControlDrag(std::wstring const& id, DragAxis drag)
    {
        auto* const control = MutableControl(id);

        if (control == nullptr || control->Drag == drag)
        {
            return false;
        }

        control->Drag = drag;
        Commit(EditNames::Properties);

        return true;
    }

    _Use_decl_annotations_
    bool EditorController::SetControlTicks(std::wstring const& id, TickMarks const& ticks)
    {
        auto* const control = MutableControl(id);

        if (control == nullptr)
        {
            return false;
        }

        auto const count = std::clamp(ticks.Count, MinimumTickCount, MaximumTickCount);

        if (control->Ticks.Show == ticks.Show && control->Ticks.Count == count)
        {
            return false;
        }

        control->Ticks.Show = ticks.Show;
        control->Ticks.Count = count;

        Commit(EditNames::Properties);

        return true;
    }

    _Use_decl_annotations_
    bool EditorController::SetControlShowDetentValues(std::wstring const& id, bool show)
    {
        auto* const control = MutableControl(id);

        if (control == nullptr || control->ShowDetentValues == show)
        {
            return false;
        }

        control->ShowDetentValues = show;
        Commit(EditNames::Properties);

        return true;
    }

    _Use_decl_annotations_
    bool EditorController::SetControlVelocityFromTouch(std::wstring const& id, bool fromTouch)
    {
        auto* const control = MutableControl(id);

        if (control == nullptr || control->VelocityFromTouch == fromTouch)
        {
            return false;
        }

        control->VelocityFromTouch = fromTouch;
        Commit(EditNames::Properties);

        return true;
    }

    _Use_decl_annotations_
    bool EditorController::SetControlPicture(std::wstring const& id, Picture const& picture)
    {
        auto* const control = MutableControl(id);

        if (control == nullptr)
        {
            return false;
        }

        // A name that is a path is refused rather than trimmed into something that looks safe.
        // The picture travels beside the layout or it does not travel at all.
        auto safe = picture;
        safe.FileName = SanitizeFileName(picture.FileName);
        safe.Opacity = std::clamp(picture.Opacity, 0.0, 1.0);

        if (control->Image.FileName == safe.FileName &&
            control->Image.Fit == safe.Fit &&
            control->Image.Opacity == safe.Opacity &&
            control->Image.Loops == safe.Loops)
        {
            return false;
        }

        control->Image.FileName = safe.FileName;
        control->Image.Fit = safe.Fit;
        control->Image.Opacity = safe.Opacity;
        control->Image.Loops = safe.Loops;

        Commit(EditNames::Properties);

        return true;
    }

    _Use_decl_annotations_
    bool EditorController::SetControlKeyboard(std::wstring const& id, KeyboardSpec const& keyboard)
    {
        auto* const control = MutableControl(id);

        if (control == nullptr)
        {
            return false;
        }

        auto wanted = keyboard;

        wanted.KeyCount = std::clamp(keyboard.KeyCount, MinimumKeyboardKeys, MaximumKeyboardKeys);
        wanted.LowestNote = std::clamp(keyboard.LowestNote, 0, 127);

        auto const& current = control->Keyboard;

        if (current.KeyCount == wanted.KeyCount &&
            current.LowestNote == wanted.LowestNote &&
            current.WhiteKeyColor == wanted.WhiteKeyColor &&
            current.BlackKeyColor == wanted.BlackKeyColor &&
            current.PressedKeyColor == wanted.PressedKeyColor &&
            current.ShowNoteNames == wanted.ShowNoteNames &&
            current.VelocityFromKeyPosition == wanted.VelocityFromKeyPosition)
        {
            return false;
        }

        control->Keyboard.KeyCount = wanted.KeyCount;
        control->Keyboard.LowestNote = wanted.LowestNote;
        control->Keyboard.WhiteKeyColor = wanted.WhiteKeyColor;
        control->Keyboard.BlackKeyColor = wanted.BlackKeyColor;
        control->Keyboard.PressedKeyColor = wanted.PressedKeyColor;
        control->Keyboard.ShowNoteNames = wanted.ShowNoteNames;
        control->Keyboard.VelocityFromKeyPosition = wanted.VelocityFromKeyPosition;

        Commit(EditNames::Properties);

        return true;
    }

    _Use_decl_annotations_
    bool EditorController::SetControlClock(std::wstring const& id, ClockSpec const& clock)
    {
        auto* const control = MutableControl(id);

        if (control == nullptr)
        {
            return false;
        }

        auto wanted = clock;

        wanted.BeatsPerMinute = std::clamp(
            clock.BeatsPerMinute, MinimumBeatsPerMinute, MaximumBeatsPerMinute);
        wanted.LowestBeatsPerMinute = std::clamp(
            clock.LowestBeatsPerMinute, MinimumBeatsPerMinute, MaximumBeatsPerMinute);
        wanted.HighestBeatsPerMinute = std::clamp(
            clock.HighestBeatsPerMinute, MinimumBeatsPerMinute, MaximumBeatsPerMinute);

        // A clock taking its tempo from itself would feed back, so it is refused rather than
        // quietly producing a control that behaves differently from what the picker showed.
        if (wanted.TempoControlId == id)
        {
            wanted.TempoControlId.clear();
        }

        auto const& current = control->Clock;

        if (current.BeatsPerMinute == wanted.BeatsPerMinute &&
            current.TempoControlId == wanted.TempoControlId &&
            current.LowestBeatsPerMinute == wanted.LowestBeatsPerMinute &&
            current.HighestBeatsPerMinute == wanted.HighestBeatsPerMinute &&
            current.StartsRunning == wanted.StartsRunning &&
            current.SendsTransport == wanted.SendsTransport)
        {
            return false;
        }

        control->Clock.BeatsPerMinute = wanted.BeatsPerMinute;
        control->Clock.TempoControlId = wanted.TempoControlId;
        control->Clock.LowestBeatsPerMinute = wanted.LowestBeatsPerMinute;
        control->Clock.HighestBeatsPerMinute = wanted.HighestBeatsPerMinute;
        control->Clock.StartsRunning = wanted.StartsRunning;
        control->Clock.SendsTransport = wanted.SendsTransport;

        Commit(EditNames::Properties);

        return true;
    }

    _Use_decl_annotations_
    bool EditorController::SetControlDefaultValueY(std::wstring const& id, double value)
    {
        auto* const control = MutableControl(id);

        if (control == nullptr)
        {
            return false;
        }

        auto const wanted = std::clamp(value, 0.0, 1.0);

        if (control->DefaultValueY == wanted)
        {
            return false;
        }

        control->DefaultValueY = wanted;
        CommitCoalesced(EditNames::Properties, L"defaultY:" + id);

        return true;
    }

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

    _Use_decl_annotations_
    bool EditorController::SetKeyboardOrderFromList(std::vector<std::wstring> const& idsInOrder)
    {
        auto* const page = MutablePage();

        if (page == nullptr || page->Controls.empty() || idsInOrder.empty())
        {
            return false;
        }

        auto next = 0;
        auto changed = false;

        for (auto const& id : idsInOrder)
        {
            auto const found = std::find_if(
                page->Controls.begin(),
                page->Controls.end(),
                [&id](Control const& control) { return control.Id == id; });

            if (found == page->Controls.end())
            {
                continue;
            }

            auto const wanted = ++next;

            changed = changed || found->KeyboardOrder != wanted;
            found->KeyboardOrder = wanted;
        }

        if (next == 0)
        {
            return false;
        }

        // Whatever was never clicked follows, keeping the order it already had among itself.
        std::vector<Control*> remaining{};

        for (auto& control : page->Controls)
        {
            if (std::find(idsInOrder.begin(), idsInOrder.end(), control.Id) == idsInOrder.end())
            {
                remaining.push_back(&control);
            }
        }

        std::stable_sort(
            remaining.begin(),
            remaining.end(),
            [](Control const* left, Control const* right)
            { return left->KeyboardOrder < right->KeyboardOrder; });

        for (auto* const control : remaining)
        {
            auto const wanted = ++next;

            changed = changed || control->KeyboardOrder != wanted;
            control->KeyboardOrder = wanted;
        }

        if (!changed)
        {
            return false;
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
    bool EditorController::MoveControlsAndRemovePage(size_t index, size_t destinationIndex)
    {
        if (index >= m_document.Pages.size() ||
            destinationIndex >= m_document.Pages.size() ||
            index == destinationIndex ||
            m_document.Pages.size() < 2)
        {
            return false;
        }

        auto moved = std::move(m_document.Pages[index].Controls);

        // A page holds a bounded number of controls, so a move that would overflow the
        // destination takes as many as fit rather than silently making an unloadable file.
        auto& destination = m_document.Pages[destinationIndex].Controls;

        auto const room = MaximumControlsPerPage > destination.size()
            ? MaximumControlsPerPage - destination.size()
            : size_t{ 0 };

        if (moved.size() > room)
        {
            moved.resize(room);
        }

        // Keyboard order is per page, so the arrivals go after whatever is already there rather
        // than interleaving with it.
        auto highest = 0;

        for (auto const& existing : destination)
        {
            highest = std::max(highest, existing.KeyboardOrder);
        }

        for (auto& control : moved)
        {
            control.KeyboardOrder = ++highest;
        }

        destination.insert(destination.end(),
            std::make_move_iterator(moved.begin()),
            std::make_move_iterator(moved.end()));

        m_document.Pages.erase(m_document.Pages.begin() + static_cast<ptrdiff_t>(index));

        // The page the controls landed on is the one worth looking at, and its index shifts when
        // the removed page was before it.
        m_pageIndex = destinationIndex > index ? destinationIndex - 1 : destinationIndex;

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
    bool EditorController::MovePage(size_t index, bool up)
    {
        if (index >= m_document.Pages.size())
        {
            return false;
        }

        auto const target = up ? index - 1 : index + 1;

        if ((up && index == 0) || target >= m_document.Pages.size())
        {
            return false;
        }

        std::swap(m_document.Pages[index], m_document.Pages[target]);

        // The page being edited follows its contents rather than staying on a slot number, or
        // reordering would silently switch which page is on the canvas.
        if (m_pageIndex == index)
        {
            m_pageIndex = target;
        }
        else if (m_pageIndex == target)
        {
            m_pageIndex = index;
        }

        Commit(EditNames::PageProperties);

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
    bool EditorController::SetBackgroundImage(std::wstring const& fileName, BackgroundFit fit)
    {
        // Only a bare file name is ever stored, so that a layout from a stranger cannot point
        // this at a file elsewhere on the PC.
        auto const safe = SanitizeFileName(fileName);

        if (m_document.BackgroundImage == safe && m_document.BackgroundFitMode == fit)
        {
            return false;
        }

        m_document.BackgroundImage = safe;
        m_document.BackgroundFitMode = fit;

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

    _Use_decl_annotations_
    bool EditorController::SetDeviceMatch(
        std::wstring const& name,
        midiapp::EndpointMatch const& match,
        midiapp::EndpointMatchMode mode)
    {
        auto const found = std::find_if(
            m_document.Devices.begin(),
            m_document.Devices.end(),
            [&name](DeviceEntry const& entry) { return entry.Name == name; });

        if (found == m_document.Devices.end())
        {
            return false;
        }

        found->Match = match;
        found->MatchMode = mode;

        Commit(EditNames::Devices);

        return true;
    }

    _Use_decl_annotations_
    bool EditorController::SetDeviceMatchMode(std::wstring const& name, midiapp::EndpointMatchMode mode)
    {
        auto const found = std::find_if(
            m_document.Devices.begin(),
            m_document.Devices.end(),
            [&name](DeviceEntry const& entry) { return entry.Name == name; });

        if (found == m_document.Devices.end() || found->MatchMode == mode)
        {
            return false;
        }

        found->MatchMode = mode;

        Commit(EditNames::Devices);

        return true;
    }

    _Use_decl_annotations_
    size_t EditorController::CountControlsUsingDevice(std::wstring const& name) const
    {
        size_t count{ 0 };

        for (auto const& page : m_document.Pages)
        {
            for (auto const& control : page.Controls)
            {
                auto used = control.Feedback.DeviceName == name;

                for (auto const& message : control.Messages)
                {
                    used = used || message.DeviceName == name;
                }

                if (used)
                {
                    ++count;
                }
            }
        }

        return count;
    }

    // ---------------------------------------------------------------- sequences

    _Use_decl_annotations_
    std::wstring EditorController::AddSequence(std::wstring const& name)
    {
        if (m_document.Sequences.size() >= MaximumSequencesPerLayout)
        {
            return {};
        }

        auto const base = name.empty() ? std::wstring{ L"Sequence" } : SanitizeStoredString(name);

        auto chosen = base;
        int32_t suffix{ 2 };

        // Two sequences with one name would make "which one does this button play" unanswerable,
        // so the second one gets a number rather than being refused.
        while (m_document.FindSequence(chosen) != nullptr)
        {
            chosen = base + L" " + std::to_wstring(suffix++);

            if (suffix > 1000)
            {
                return {};
            }
        }

        Sequence sequence{};
        sequence.Name = chosen;

        m_document.Sequences.push_back(std::move(sequence));

        Commit(EditNames::Sequence);

        return chosen;
    }

    _Use_decl_annotations_
    bool EditorController::SetSequence(std::wstring const& name, Sequence const& sequence)
    {
        auto const found = std::find_if(
            m_document.Sequences.begin(),
            m_document.Sequences.end(),
            [&name](glass::Sequence const& entry) { return entry.Name == name; });

        if (found == m_document.Sequences.end())
        {
            return false;
        }

        if (found->Steps.size() == sequence.Steps.size() && found->Mode == sequence.Mode)
        {
            // Writing back what is already there would put an entry on the undo stack for an
            // edit nobody made.
            auto same = true;

            for (size_t index = 0; index < sequence.Steps.size(); ++index)
            {
                auto const& left = found->Steps[index];
                auto const& right = sequence.Steps[index];

                if (left.Kind != right.Kind ||
                    left.WaitMilliseconds != right.WaitMilliseconds ||
                    left.RepeatCount != right.RepeatCount ||
                    left.TargetControlId != right.TargetControlId ||
                    left.Message.Kind != right.Message.Kind ||
                    left.Message.Number != right.Message.Number ||
                    left.Message.DeviceName != right.Message.DeviceName ||
                    left.Message.ChannelIndex != right.Message.ChannelIndex ||
                    left.Message.SystemExclusive != right.Message.SystemExclusive)
                {
                    same = false;
                    break;
                }
            }

            if (same)
            {
                return false;
            }
        }

        auto const unknown = found->Unknown;

        *found = sequence;
        found->Name = name;
        found->Unknown = unknown;

        if (found->Steps.size() > MaximumStepsPerSequence)
        {
            found->Steps.resize(MaximumStepsPerSequence);
        }

        Commit(EditNames::Sequence);

        return true;
    }

    _Use_decl_annotations_
    bool EditorController::RemoveSequence(std::wstring const& name)
    {
        auto const found = std::find_if(
            m_document.Sequences.begin(),
            m_document.Sequences.end(),
            [&name](Sequence const& entry) { return entry.Name == name; });

        if (found == m_document.Sequences.end())
        {
            return false;
        }

        m_document.Sequences.erase(found);

        // The controls that played it keep the name. A message pointing at a sequence that is
        // gone sends nothing, and the editor can still show what it was meant to play, which is
        // more use than silently clearing the row.
        Commit(EditNames::Sequence);

        return true;
    }
}
