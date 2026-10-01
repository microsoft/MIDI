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
#include "MackieControl.h"
#include "PageTemplates.h"
#include "LayoutSerializer.h"
#include "PadGrid.h"
#include "StepPattern.h"

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
                left.Axis == right.Axis &&
                left.Position == right.Position;
        }

        // A row moving from one protocol to another keeps what it means wherever it can.
        void ConvertRow(
            _Inout_ ControlMessage& row,
            _In_ ControlKind controlKind,
            _In_ DeviceProtocol from,
            _In_ DeviceProtocol to)
        {
            if (from == to)
            {
                return;
            }

            if (to == DeviceProtocol::MackieControl)
            {
                auto const function = MackieFunctionOf(row);

                if (function != MackieNoFunction && MackieFunctionFits(function, controlKind))
                {
                    row = MakeMackieRow(function, row.DeviceName, row.GroupIndex);
                    return;
                }
            }

            if (row.Kind == MessageKind::MackieControl)
            {
                if (to != DeviceProtocol::MackieControl)
                {
                    row = PlainRowFor(row);
                }

                return;
            }

            // The device decides the protocol from here on, not the row.
            ShareExactValues(row, from);
            row.UseMidi1Protocol = false;
        }

        // A turn is sent from how far a control moved, so a knob sending one comes back to the
        // middle when let go, the way a platter does, or it would run out of travel.
        void SpringForTurns(_Inout_ Control& control) noexcept
        {
            for (auto const& row : control.Messages)
            {
                if (row.Kind == MessageKind::MackieControl &&
                    ShapeOfMackieFunction(row.Number) == MackieShape::Encoder)
                {
                    control.ReturnsToDefault = true;
                    control.DefaultValue = 0.5;
                    return;
                }
            }
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

        // One value that several rows either all agree on or do not.
        template <typename T>
        class Agreement
        {
        public:
            void Add(_In_ T const& value)
            {
                if (m_mixed)
                {
                    return;
                }

                if (!m_value.has_value())
                {
                    m_value = value;
                }
                else if (*m_value != value)
                {
                    m_value.reset();
                    m_mixed = true;
                }
            }

            std::optional<T> const& Value() const noexcept { return m_value; }

        private:
            std::optional<T> m_value{};
            bool m_mixed{ false };
        };

        // A device has to be in the layout's own table, so a row is never pointed at a name
        // that means nothing on this layout. Listening can also take no device at all, which
        // means any device.
        bool IsValidDestination(
            _In_ LayoutDocument const& document,
            _In_ DestinationFields const& change,
            _In_ bool noDeviceMeansAny) noexcept
        {
            if (change.DeviceName.has_value())
            {
                auto const& name = *change.DeviceName;

                if (name.empty() ? !noDeviceMeansAny : document.FindDevice(name) == nullptr)
                {
                    return false;
                }
            }

            if (change.GroupIndex.has_value() &&
                *change.GroupIndex != AllGroups &&
                (*change.GroupIndex < 0 || *change.GroupIndex >= MaximumGroupCount))
            {
                return false;
            }

            if (change.ChannelIndex.has_value() && (*change.ChannelIndex < 0 || *change.ChannelIndex > 15))
            {
                return false;
            }

            return true;
        }

        // A tempo listener follows the clock of a whole device, so it has no channel to set.
        bool ListensOnAChannel(_In_ FeedbackBinding const& feedback) noexcept
        {
            return feedback.Mode != FeedbackMode::Tempo;
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

        auto const previous = control->Kind;

        control->Kind = kind;
        control->AspectLocked = IsSquareByNature(kind);

        // Square pads and hexagons want different intervals out of the box. Switching between
        // the two carries the other one's starting layout across, but only while nobody has
        // changed it: a layout somebody chose is theirs.
        if (IsPadGrid(previous) && IsPadGrid(kind))
        {
            PadGridSpec const square{};

            auto& pads = control->Pads;

            if (IsHexPadGrid(kind) &&
                pads.RightInterval == square.RightInterval &&
                pads.RowInterval == square.RowInterval)
            {
                pads.RightInterval = WickiHaydenLayout.RightInterval;
                pads.RowInterval = WickiHaydenLayout.RowInterval;
            }
            else if (!IsHexPadGrid(kind) &&
                pads.RightInterval == WickiHaydenLayout.RightInterval &&
                pads.RowInterval == WickiHaydenLayout.RowInterval)
            {
                pads.RightInterval = square.RightInterval;
                pads.RowInterval = square.RowInterval;
            }
        }

        // A control turned into a sequencer has no steps to play, which would look like a
        // sequencer that is broken. It gets the same pattern a new one does, and a note row to
        // play it on, pointed wherever the control was already sending.
        if (kind == ControlKind::Steps)
        {
            if (control->Steps.Pattern.empty())
            {
                FillStarterPattern(control->Steps, 48);
            }

            auto const plays = std::any_of(control->Messages.begin(), control->Messages.end(),
                [](ControlMessage const& message) { return message.Kind == MessageKind::Note; });

            if (!plays && control->Messages.size() < MaximumMessagesPerControl)
            {
                ControlMessage row{};

                if (!control->Messages.empty())
                {
                    row.DeviceName = control->Messages.front().DeviceName;
                    row.GroupIndex = control->Messages.front().GroupIndex;
                    row.ChannelIndex = control->Messages.front().ChannelIndex;
                }

                row.Trigger = MessageTrigger::Changes;
                row.Kind = MessageKind::Note;

                control->Messages.push_back(std::move(row));
            }
        }

        // Changing a fader into a lamp leaves messages that no longer have anything to send
        // them. They are kept rather than thrown away, because changing back has to be free.
        Commit(EditNames::Properties);

        return true;
    }

    _Use_decl_annotations_
    bool EditorController::SetControlHueSlot(std::wstring const& id, int32_t slot)
    {
        auto* const control = MutableControl(id);

        if (control == nullptr || !IsSlotInRange(slot))
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

        if (!style.FontFamily.empty() && !IsSafeFontFamilyName(style.FontFamily))
        {
            control->LabelLook.FontFamily.clear();
        }
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
    bool EditorController::SetControlLightsFromCenter(std::wstring const& id, bool fromCenter)
    {
        auto* const control = MutableControl(id);

        if (control == nullptr || control->LightsFromCenter == fromCenter)
        {
            return false;
        }

        control->LightsFromCenter = fromCenter;

        if (fromCenter && control->DefaultValue == 0.0)
        {
            control->DefaultValue = 0.5;
        }

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
    bool EditorController::SetControlPicture(
        std::wstring const& id,
        Picture const& picture,
        bool coalesce)
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
        safe.Zoom = std::clamp(picture.Zoom, MinimumPictureZoom, MaximumPictureZoom);
        safe.CenterX = std::clamp(picture.CenterX, 0.0, 1.0);
        safe.CenterY = std::clamp(picture.CenterY, 0.0, 1.0);
        safe.TintStrength = std::clamp(picture.TintStrength, 0.0, 1.0);

        auto const seconds = [](double value) noexcept
            {
                return std::isfinite(value) ? std::clamp(value, 0.0, MaximumVideoSeconds) : 0.0;
            };

        safe.VideoStartSeconds = seconds(picture.VideoStartSeconds);
        safe.VideoEndSeconds = seconds(picture.VideoEndSeconds);

        // A stop that is not after the start would play nothing, so it means the end of the file.
        if (safe.VideoEndSeconds > 0.0 && safe.VideoEndSeconds <= safe.VideoStartSeconds)
        {
            safe.VideoEndSeconds = 0.0;
        }

        if (control->Image.FileName == safe.FileName &&
            control->Image.Fit == safe.Fit &&
            control->Image.Opacity == safe.Opacity &&
            control->Image.Loops == safe.Loops &&
            control->Image.Zoom == safe.Zoom &&
            control->Image.CenterX == safe.CenterX &&
            control->Image.CenterY == safe.CenterY &&
            control->Image.TintColor == safe.TintColor &&
            control->Image.TintStrength == safe.TintStrength &&
            control->Image.VideoStartSeconds == safe.VideoStartSeconds &&
            control->Image.VideoEndSeconds == safe.VideoEndSeconds &&
            control->Image.AutoPlays == safe.AutoPlays &&
            control->Image.ClickToPlay == safe.ClickToPlay &&
            control->Image.ShowsScrubber == safe.ShowsScrubber)
        {
            return false;
        }

        control->Image.FileName = safe.FileName;
        control->Image.Fit = safe.Fit;
        control->Image.Opacity = safe.Opacity;
        control->Image.Loops = safe.Loops;
        control->Image.Zoom = safe.Zoom;
        control->Image.CenterX = safe.CenterX;
        control->Image.CenterY = safe.CenterY;
        control->Image.TintColor = safe.TintColor;
        control->Image.TintStrength = safe.TintStrength;
        control->Image.VideoStartSeconds = safe.VideoStartSeconds;
        control->Image.VideoEndSeconds = safe.VideoEndSeconds;
        control->Image.AutoPlays = safe.AutoPlays;
        control->Image.ClickToPlay = safe.ClickToPlay;
        control->Image.ShowsScrubber = safe.ShowsScrubber;

        if (coalesce)
        {
            CommitCoalesced(EditNames::Properties, L"picture:" + id);
        }
        else
        {
            Commit(EditNames::Properties);
        }

        return true;
    }

    _Use_decl_annotations_
    bool EditorController::SetSwitchPositionName(std::wstring const& id, size_t index, std::wstring const& name)
    {
        auto* const control = MutableControl(id);

        if (control == nullptr || index >= control->Switch.Positions.size() || name.size() > MaximumStringLength)
        {
            return false;
        }

        auto clean = SanitizeStoredString(name);

        if (control->Switch.Positions[index] == clean)
        {
            return false;
        }

        control->Switch.Positions[index] = std::move(clean);
        Commit(EditNames::Properties);

        return true;
    }

    _Use_decl_annotations_
    bool EditorController::AddSwitchPosition(std::wstring const& id)
    {
        auto* const control = MutableControl(id);

        if (control == nullptr ||
            control->Kind != ControlKind::Switch ||
            control->Switch.Positions.size() >= static_cast<size_t>(MaximumSwitchPositions))
        {
            return false;
        }

        // A switch read from a file with fewer names than it shows gets its missing names first,
        // so the new one lands after the positions somebody could already see.
        while (control->Switch.Positions.size() < static_cast<size_t>(MinimumSwitchPositions))
        {
            control->Switch.Positions.push_back(std::to_wstring(control->Switch.Positions.size() + 1));
        }

        auto const added = static_cast<int32_t>(control->Switch.Positions.size());

        control->Switch.Positions.push_back(std::to_wstring(added + 1));

        // A copy of the last position's row, so the new position sends the same kind of thing to
        // the same place and only its value needs changing.
        auto const last = std::find_if(control->Messages.rbegin(), control->Messages.rend(),
            [added](ControlMessage const& message) { return message.Position == added - 1; });

        if (last != control->Messages.rend() && control->Messages.size() < MaximumMessagesPerControl)
        {
            auto row = *last;

            row.Position = added;
            row.Unknown = nullptr;

            control->Messages.push_back(std::move(row));
        }

        Commit(EditNames::Properties);

        return true;
    }

    _Use_decl_annotations_
    bool EditorController::RemoveSwitchPosition(std::wstring const& id, size_t index)
    {
        auto* const control = MutableControl(id);

        if (control == nullptr ||
            control->Kind != ControlKind::Switch ||
            index >= control->Switch.Positions.size() ||
            control->Switch.Positions.size() <= static_cast<size_t>(MinimumSwitchPositions))
        {
            return false;
        }

        auto const removed = static_cast<int32_t>(index);

        control->Switch.Positions.erase(control->Switch.Positions.begin() + static_cast<ptrdiff_t>(index));

        control->Messages.erase(
            std::remove_if(control->Messages.begin(), control->Messages.end(),
                [removed](ControlMessage const& message) { return message.Position == removed; }),
            control->Messages.end());

        for (auto& message : control->Messages)
        {
            if (message.Position > removed)
            {
                --message.Position;
            }
        }

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
    bool EditorController::SetControlLfo(std::wstring const& id, LfoSpec const& lfo)
    {
        auto* const control = MutableControl(id);

        if (control == nullptr)
        {
            return false;
        }

        auto wanted = lfo;

        wanted.BeatsPerCycle = std::clamp(
            lfo.BeatsPerCycle, MinimumBeatsPerCycle, MaximumBeatsPerCycle);
        wanted.Lowest = std::clamp(lfo.Lowest, 0.0, 1.0);
        wanted.Highest = std::clamp(lfo.Highest, 0.0, 1.0);
        wanted.UpdateIntervalMilliseconds = std::clamp(
            lfo.UpdateIntervalMilliseconds,
            MinimumLfoIntervalMilliseconds,
            MaximumLfoIntervalMilliseconds);

        auto const& current = control->Lfo;

        if (current.Wave == wanted.Wave &&
            current.BeatsPerCycle == wanted.BeatsPerCycle &&
            current.Lowest == wanted.Lowest &&
            current.Highest == wanted.Highest &&
            current.UpdateIntervalMilliseconds == wanted.UpdateIntervalMilliseconds &&
            current.Latching == wanted.Latching &&
            current.StartsRunning == wanted.StartsRunning &&
            current.ReturnsToRestWhenStopped == wanted.ReturnsToRestWhenStopped)
        {
            return false;
        }

        control->Lfo.Wave = wanted.Wave;
        control->Lfo.BeatsPerCycle = wanted.BeatsPerCycle;
        control->Lfo.Lowest = wanted.Lowest;
        control->Lfo.Highest = wanted.Highest;
        control->Lfo.UpdateIntervalMilliseconds = wanted.UpdateIntervalMilliseconds;
        control->Lfo.Latching = wanted.Latching;
        control->Lfo.StartsRunning = wanted.StartsRunning;
        control->Lfo.ReturnsToRestWhenStopped = wanted.ReturnsToRestWhenStopped;

        Commit(EditNames::Properties);

        return true;
    }

    _Use_decl_annotations_
    bool EditorController::SetStepsSettings(std::wstring const& id, StepsSpec const& settings)
    {
        auto* const control = MutableControl(id);

        if (control == nullptr)
        {
            return false;
        }

        auto const perBeat = std::clamp(settings.StepsPerBeat, MinimumStepsPerBeat, MaximumStepsPerBeat);
        auto const gate = std::clamp(settings.Gate, MinimumStepGate, MaximumStepGate);
        auto const swing = std::clamp(settings.Swing, MinimumStepSwing, MaximumStepSwing);

        auto& current = control->Steps;

        if (current.StepsPerBeat == perBeat &&
            current.Gate == gate &&
            current.Swing == swing &&
            current.Direction == settings.Direction &&
            current.Latching == settings.Latching &&
            current.StartsRunning == settings.StartsRunning)
        {
            return false;
        }

        current.StepsPerBeat = perBeat;
        current.Gate = gate;
        current.Swing = swing;
        current.Direction = settings.Direction;
        current.Latching = settings.Latching;
        current.StartsRunning = settings.StartsRunning;

        // A slider being dragged is one undo step, not one per notch.
        CommitCoalesced(EditNames::Properties, L"steps:" + id);

        return true;
    }

    _Use_decl_annotations_
    bool EditorController::SetStepCount(std::wstring const& id, int32_t count)
    {
        auto* const control = MutableControl(id);

        if (control == nullptr)
        {
            return false;
        }

        auto const wanted = static_cast<size_t>(std::clamp(count, MinimumSequencerSteps, MaximumSequencerSteps));
        auto& pattern = control->Steps.Pattern;

        if (pattern.size() == wanted)
        {
            return false;
        }

        if (pattern.size() > wanted)
        {
            pattern.resize(wanted);
        }
        else
        {
            auto copy = pattern.empty() ? SequencerStep{} : pattern.back();

            copy.Unknown = nullptr;

            pattern.resize(wanted, copy);
        }

        Commit(EditNames::Properties);

        return true;
    }

    _Use_decl_annotations_
    bool EditorController::SetStep(std::wstring const& id, int32_t index, SequencerStep const& step, bool coalesce)
    {
        auto* const control = MutableControl(id);

        if (control == nullptr || index < 0 || static_cast<size_t>(index) >= control->Steps.Pattern.size())
        {
            return false;
        }

        auto& current = control->Steps.Pattern[static_cast<size_t>(index)];

        auto const note = std::clamp(step.Note, 0, 127);
        auto const velocity = std::isfinite(step.Velocity) ? std::clamp(step.Velocity, 0.0, 1.0) : current.Velocity;

        if (current.On == step.On && current.Note == note && current.Velocity == velocity)
        {
            return false;
        }

        current.On = step.On;
        current.Note = note;
        current.Velocity = velocity;

        if (coalesce)
        {
            CommitCoalesced(EditNames::Properties, L"step:" + id + L":" + std::to_wstring(index));
        }
        else
        {
            Commit(EditNames::Properties);
        }

        return true;
    }

    _Use_decl_annotations_
    bool EditorController::SetControlTurntable(std::wstring const& id, TurntableSpec const& turntable)
    {
        auto* const control = MutableControl(id);

        if (control == nullptr)
        {
            return false;
        }

        auto const degrees = std::clamp(
            turntable.DegreesForFullRange, MinimumTurntableDegrees, MaximumTurntableDegrees);

        if (control->Turntable.DegreesForFullRange == degrees &&
            control->Turntable.ShowsGrip == turntable.ShowsGrip)
        {
            return false;
        }

        control->Turntable.DegreesForFullRange = degrees;
        control->Turntable.ShowsGrip = turntable.ShowsGrip;

        Commit(EditNames::Properties);

        return true;
    }

    _Use_decl_annotations_
    bool EditorController::SetControlLine(std::wstring const& id, LineSpec const& line)
    {
        auto* const control = MutableControl(id);

        if (control == nullptr)
        {
            return false;
        }

        auto const thickness = std::clamp(line.Thickness, MinimumLineThickness, MaximumLineThickness);

        if (control->Line.Thickness == thickness &&
            control->Line.Color == line.Color &&
            control->Line.Ends == line.Ends)
        {
            return false;
        }

        // Field by field, so anything a newer version wrote into the line survives the edit.
        control->Line.Thickness = thickness;
        control->Line.Color = line.Color;
        control->Line.Ends = line.Ends;

        Commit(EditNames::Properties);

        return true;
    }

    _Use_decl_annotations_
    bool EditorController::SetControlPads(std::wstring const& id, PadGridSpec const& pads)
    {
        auto* const control = MutableControl(id);

        if (control == nullptr ||
            pads.RootColor.size() > MaximumStringLength ||
            pads.InKeyColor.size() > MaximumStringLength ||
            pads.OutOfKeyColor.size() > MaximumStringLength ||
            pads.PressedColor.size() > MaximumStringLength)
        {
            return false;
        }

        auto wanted = pads;

        wanted.PadCount = std::clamp(pads.PadCount, MinimumPadCount, MaximumPadCount);
        wanted.PadSize = std::isfinite(pads.PadSize)
            ? std::clamp(pads.PadSize, MinimumPadSize, MaximumPadSize)
            : control->Pads.PadSize;
        wanted.StartNote = std::clamp(pads.StartNote, 0, 127);
        wanted.RightInterval = std::clamp(pads.RightInterval, MinimumRightInterval, MaximumPadInterval);
        wanted.RowInterval = std::clamp(pads.RowInterval, MinimumRowInterval, MaximumPadInterval);
        wanted.KeyRoot = pads.KeyRoot >= 0 && pads.KeyRoot <= 11 ? pads.KeyRoot : NoKey;
        wanted.NoteNameSize = std::isfinite(pads.NoteNameSize)
            ? std::clamp(pads.NoteNameSize, 0.0, MaximumPadNoteNameSize)
            : 0.0;
        wanted.BendRangeSemitones = std::clamp(
            pads.BendRangeSemitones, MinimumBendRangeSemitones, MaximumBendRangeSemitones);

        auto const& current = control->Pads;

        if (current.PadCount == wanted.PadCount &&
            current.PadSize == wanted.PadSize &&
            current.StartNote == wanted.StartNote &&
            current.RightInterval == wanted.RightInterval &&
            current.RowInterval == wanted.RowInterval &&
            current.KeyRoot == wanted.KeyRoot &&
            current.Scale == wanted.Scale &&
            current.NoteNames == wanted.NoteNames &&
            current.NoteNameSize == wanted.NoteNameSize &&
            current.RootColor == wanted.RootColor &&
            current.InKeyColor == wanted.InKeyColor &&
            current.OutOfKeyColor == wanted.OutOfKeyColor &&
            current.PressedColor == wanted.PressedColor &&
            current.Glide == wanted.Glide &&
            current.BendRangeSemitones == wanted.BendRangeSemitones)
        {
            return false;
        }

        // Everything but the fields this build does not know, which stay as they were.
        wanted.Unknown = current.Unknown;
        control->Pads = std::move(wanted);

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

            // A Mackie Control device takes a function, and which one is the customer's to say.
            if (m_document.ProtocolOf(message.DeviceName) == DeviceProtocol::MackieControl)
            {
                message = MakeMackieRow(MackieNoFunction, message.DeviceName, message.GroupIndex);
            }
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

        // Moved to another device, the row becomes something that device understands.
        if (updated.DeviceName != control->Messages[index].DeviceName)
        {
            ConvertRow(
                updated,
                control->Kind,
                m_document.ProtocolOf(control->Messages[index].DeviceName),
                m_document.ProtocolOf(updated.DeviceName));
        }

        control->Messages[index] = std::move(updated);
        SpringForTurns(*control);
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

    // ---------------------------------------------------------------- where several send

    _Use_decl_annotations_
    SharedDestination EditorController::SharedSendDestination(std::vector<std::wstring> const& ids) const
    {
        SharedDestination shared{};

        Agreement<std::wstring> device{};
        Agreement<int32_t> group{};
        Agreement<int32_t> channel{};

        for (auto const& id : ids)
        {
            auto const* const control = m_document.FindControl(id);

            if (control == nullptr)
            {
                continue;
            }

            for (auto const& message : control->Messages)
            {
                if (!SendsToADevice(message.Kind))
                {
                    continue;
                }

                ++shared.DeviceRows;
                device.Add(message.DeviceName);
                group.Add(message.GroupIndex);

                if (CarriesAChannel(message.Kind))
                {
                    ++shared.ChannelRows;
                    channel.Add(message.ChannelIndex);
                }
            }
        }

        shared.Fields.DeviceName = device.Value();
        shared.Fields.GroupIndex = group.Value();
        shared.Fields.ChannelIndex = channel.Value();

        return shared;
    }

    _Use_decl_annotations_
    bool EditorController::SetSendDestination(std::vector<std::wstring> const& ids, DestinationFields const& change)
    {
        if (!IsValidDestination(m_document, change, false))
        {
            return false;
        }

        auto changed = false;

        for (auto const& id : ids)
        {
            auto* const control = MutableControl(id);

            if (control == nullptr)
            {
                continue;
            }

            for (auto& message : control->Messages)
            {
                if (!SendsToADevice(message.Kind))
                {
                    continue;
                }

                if (change.DeviceName.has_value() && message.DeviceName != *change.DeviceName)
                {
                    message.DeviceName = *change.DeviceName;
                    changed = true;
                }

                if (change.GroupIndex.has_value() && message.GroupIndex != *change.GroupIndex)
                {
                    message.GroupIndex = *change.GroupIndex;
                    changed = true;
                }

                if (change.ChannelIndex.has_value() &&
                    CarriesAChannel(message.Kind) &&
                    message.ChannelIndex != *change.ChannelIndex)
                {
                    message.ChannelIndex = *change.ChannelIndex;
                    changed = true;
                }
            }
        }

        if (changed)
        {
            Commit(EditNames::Messages);
        }

        return changed;
    }

    _Use_decl_annotations_
    SharedDestination EditorController::SharedListenDestination(std::vector<std::wstring> const& ids) const
    {
        SharedDestination shared{};

        Agreement<std::wstring> device{};
        Agreement<int32_t> group{};
        Agreement<int32_t> channel{};

        for (auto const& id : ids)
        {
            auto const* const control = m_document.FindControl(id);

            if (control == nullptr || !control->Feedback.Enabled)
            {
                continue;
            }

            auto const& feedback = control->Feedback;

            ++shared.DeviceRows;
            device.Add(feedback.DeviceName);
            group.Add(feedback.GroupIndex);

            if (ListensOnAChannel(feedback))
            {
                ++shared.ChannelRows;
                channel.Add(feedback.ChannelIndex);
            }
        }

        shared.Fields.DeviceName = device.Value();
        shared.Fields.GroupIndex = group.Value();
        shared.Fields.ChannelIndex = channel.Value();

        return shared;
    }

    _Use_decl_annotations_
    bool EditorController::SetListenDestination(std::vector<std::wstring> const& ids, DestinationFields const& change)
    {
        if (!IsValidDestination(m_document, change, true))
        {
            return false;
        }

        auto changed = false;

        for (auto const& id : ids)
        {
            auto* const control = MutableControl(id);

            // Turning listening on is left to each control, because what it listens for is its own.
            if (control == nullptr || !control->Feedback.Enabled)
            {
                continue;
            }

            auto& feedback = control->Feedback;

            if (change.DeviceName.has_value() && feedback.DeviceName != *change.DeviceName)
            {
                feedback.DeviceName = *change.DeviceName;
                changed = true;
            }

            if (change.GroupIndex.has_value() && feedback.GroupIndex != *change.GroupIndex)
            {
                feedback.GroupIndex = *change.GroupIndex;
                changed = true;
            }

            if (change.ChannelIndex.has_value() &&
                ListensOnAChannel(feedback) &&
                feedback.ChannelIndex != *change.ChannelIndex)
            {
                feedback.ChannelIndex = *change.ChannelIndex;
                changed = true;
            }
        }

        if (changed)
        {
            Commit(EditNames::Messages);
        }

        return changed;
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

        // The names go with their groups.
        auto& destinationPage = m_document.Pages[destinationIndex];

        for (auto& group : m_document.Pages[index].Groups)
        {
            if (destinationPage.FindGroup(group.Id) == nullptr)
            {
                destinationPage.Groups.push_back(std::move(group));
            }
        }

        PruneControlGroups(destinationPage);

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
    bool EditorController::ChooseTheme(Theme const& theme)
    {
        if (m_document.ThemeName == theme.Name && !m_document.HasOwnTheme)
        {
            return false;
        }

        m_document.ThemeName = theme.Name;

        // Picking one out of the gallery drops whatever the layout was carrying. The layout now
        // points at a theme by name again, which is what lets a later improvement to that theme
        // reach it.
        m_document.HasOwnTheme = false;
        m_document.OwnTheme = {};

        Commit(EditNames::LayoutProperties);

        return true;
    }

    _Use_decl_annotations_
    bool EditorController::SetOwnTheme(Theme const& theme)
    {
        m_document.OwnTheme = theme;
        m_document.HasOwnTheme = true;

        // A theme carried by a layout is not a built-in one, whatever it started as. Leaving the
        // flag set would let an edited copy shadow a shipped theme in the picker and make itself
        // unoverwritable.
        m_document.OwnTheme.IsBuiltIn = false;

        // Dragging a slider is one edit, not sixty. The coalescing key is the property itself,
        // so moving the glow and then moving the corner radius are two entries in the stack.
        CommitCoalesced(EditNames::LayoutProperties, L"theme");

        return true;
    }

    _Use_decl_annotations_
    bool EditorController::SetBackgroundImage(std::wstring const& fileName, BackgroundFit fit, double opacity)
    {
        // Only a bare file name is ever stored, so that a layout from a stranger cannot point
        // this at a file elsewhere on the PC.
        auto const safe = SanitizeFileName(fileName);
        auto const strength = std::isfinite(opacity) ? std::clamp(opacity, 0.0, 1.0) : 1.0;

        if (m_document.BackgroundImage == safe &&
            m_document.BackgroundFitMode == fit &&
            std::abs(m_document.BackgroundOpacity - strength) < 0.001)
        {
            return false;
        }

        m_document.BackgroundImage = safe;
        m_document.BackgroundFitMode = fit;
        m_document.BackgroundOpacity = strength;

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

    _Use_decl_annotations_
    bool EditorController::SetPublishesVirtualDevice(bool publishes)
    {
        if (m_document.PublishesVirtualDevice == publishes)
        {
            return false;
        }

        m_document.PublishesVirtualDevice = publishes;
        Commit(EditNames::LayoutProperties);

        return true;
    }

    _Use_decl_annotations_
    bool EditorController::SetScaleMode(ScaleMode mode, double customPercent)
    {
        auto const percent = std::clamp(customPercent, 10.0, 400.0);

        if (m_document.Scale == mode && std::abs(m_document.CustomScalePercent - percent) < 0.01)
        {
            return false;
        }

        m_document.Scale = mode;
        m_document.CustomScalePercent = percent;
        Commit(EditNames::LayoutProperties);

        return true;
    }

    _Use_decl_annotations_
    bool EditorController::SetFullScreenButtonCorner(ScreenCorner corner)
    {
        if (m_document.FullScreenButtonCorner == corner)
        {
            return false;
        }

        m_document.FullScreenButtonCorner = corner;
        Commit(EditNames::LayoutProperties);

        return true;
    }

    _Use_decl_annotations_
    bool EditorController::SetToolbarWindow(bool toolbar)
    {
        if (m_document.ToolbarWindow == toolbar)
        {
            return false;
        }

        m_document.ToolbarWindow = toolbar;
        Commit(EditNames::LayoutProperties);

        return true;
    }

    _Use_decl_annotations_
    bool EditorController::SetAlwaysOnTop(bool onTop)
    {
        if (m_document.AlwaysOnTop == onTop)
        {
            return false;
        }

        m_document.AlwaysOnTop = onTop;
        Commit(EditNames::LayoutProperties);

        return true;
    }

    _Use_decl_annotations_
    bool EditorController::SetSeeThrough(bool seeThrough)
    {
        if (m_document.SeeThrough == seeThrough)
        {
            return false;
        }

        m_document.SeeThrough = seeThrough;
        Commit(EditNames::LayoutProperties);

        return true;
    }

    _Use_decl_annotations_
    bool EditorController::SetTempoSource(TempoSource const& tempo)
    {
        if (m_document.Tempo.Kind == tempo.Kind &&
            std::abs(m_document.Tempo.BeatsPerMinute - tempo.BeatsPerMinute) < 0.01 &&
            m_document.Tempo.DeviceName == tempo.DeviceName)
        {
            return false;
        }

        m_document.Tempo.Kind = tempo.Kind;
        m_document.Tempo.BeatsPerMinute = std::clamp(tempo.BeatsPerMinute, 1.0, 999.0);
        m_document.Tempo.DeviceName = tempo.DeviceName;

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
    bool EditorController::SetDeviceProtocol(std::wstring const& name, DeviceProtocol protocol)
    {
        auto const found = std::find_if(
            m_document.Devices.begin(),
            m_document.Devices.end(),
            [&name](DeviceEntry const& entry) { return entry.Name == name; });

        if (found == m_document.Devices.end() ||
            (found->Protocol == protocol && found->UnrecognizedProtocol.empty()))
        {
            return false;
        }

        auto const previous = found->Protocol;

        found->Protocol = protocol;
        found->UnrecognizedProtocol.clear();

        for (auto& page : m_document.Pages)
        {
            for (auto& control : page.Controls)
            {
                auto& rows = control.Messages;

                std::vector<uint32_t> functions{};

                for (size_t index = 0; index < rows.size();)
                {
                    auto& row = rows[index];

                    if (row.DeviceName != name)
                    {
                        ++index;
                        continue;
                    }

                    ConvertRow(row, control.Kind, previous, protocol);

                    // A press row and a release row, or a fader and its touch note, were one
                    // function all along.
                    if (row.Kind == MessageKind::MackieControl && row.Number != MackieNoFunction)
                    {
                        if (std::find(functions.begin(), functions.end(), row.Number) != functions.end())
                        {
                            rows.erase(rows.begin() + static_cast<ptrdiff_t>(index));
                            continue;
                        }

                        functions.push_back(row.Number);
                    }

                    ++index;
                }

                SpringForTurns(control);
            }
        }

        // A step in a sequence plays a plain message whatever the device speaks.
        for (auto& sequence : m_document.Sequences)
        {
            for (auto& step : sequence.Steps)
            {
                if (step.Message.DeviceName == name && step.Message.Kind != MessageKind::MackieControl)
                {
                    ShareExactValues(step.Message, previous);
                    step.Message.UseMidi1Protocol = false;
                }
            }
        }

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
    size_t EditorController::CountControlsUsingSequence(std::wstring const& name) const
    {
        size_t count{ 0 };

        for (auto const& page : m_document.Pages)
        {
            for (auto const& control : page.Controls)
            {
                if (std::any_of(control.Messages.begin(), control.Messages.end(),
                    [&name](ControlMessage const& message) { return message.SequenceName == name; }))
                {
                    ++count;
                }
            }
        }

        return count;
    }

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

    // ---------------------------------------------------------------- the clipboard

    std::wstring EditorController::CopySelection() const
    {
        auto const selected = SelectedControls();

        if (selected.empty())
        {
            return {};
        }

        LayoutDocument clip{};

        clip.Name = m_document.Name;
        clip.PageWidth = m_document.PageWidth;
        clip.PageHeight = m_document.PageHeight;
        clip.CanvasWidth = m_document.CanvasWidth;
        clip.CanvasHeight = m_document.CanvasHeight;

        Page page{};
        page.Id = LayoutDocument::NewId();

        for (auto const* const control : selected)
        {
            page.Controls.push_back(*control);
        }

        // The names of the groups the copies are in, so a paste can name its copies after them.
        if (auto const* const current = CurrentPage())
        {
            for (auto const& group : current->Groups)
            {
                if (std::any_of(page.Controls.begin(), page.Controls.end(),
                    [&group](Control const& control) { return control.GroupId == group.Id; }))
                {
                    page.Groups.push_back(group);
                }
            }
        }

        // The devices and sequences the copies name. Everything else about the layout stays
        // behind: a paste adds controls, it does not change somebody's theme or page size.
        auto const namesDevice = [&page](std::wstring const& name)
            {
                for (auto const& control : page.Controls)
                {
                    if (control.Feedback.DeviceName == name)
                    {
                        return true;
                    }

                    for (auto const& message : control.Messages)
                    {
                        if (message.DeviceName == name)
                        {
                            return true;
                        }
                    }
                }

                return false;
            };

        auto const namesSequence = [&page](std::wstring const& name)
            {
                for (auto const& control : page.Controls)
                {
                    for (auto const& message : control.Messages)
                    {
                        if (message.SequenceName == name)
                        {
                            return true;
                        }
                    }
                }

                return false;
            };

        for (auto const& device : m_document.Devices)
        {
            if (namesDevice(device.Name))
            {
                clip.Devices.push_back(device);
            }
        }

        for (auto const& sequence : m_document.Sequences)
        {
            if (namesSequence(sequence.Name))
            {
                clip.Sequences.push_back(sequence);
            }
        }

        clip.Pages.push_back(std::move(page));

        return WriteLayoutToJson(clip);
    }

    std::wstring EditorController::CutSelection()
    {
        auto text = CopySelection();

        if (text.empty())
        {
            return {};
        }

        auto* const page = MutablePage();

        if (page == nullptr)
        {
            return {};
        }

        page->Controls.erase(
            std::remove_if(
                page->Controls.begin(),
                page->Controls.end(),
                [this](Control const& control) { return IsSelected(control.Id); }),
            page->Controls.end());

        PruneControlGroups(*page);

        m_selection.clear();
        Commit(EditNames::Cut);

        return text;
    }

    _Use_decl_annotations_
    bool EditorController::PasteControls(std::wstring const& text)
    {
        auto* const page = MutablePage();

        if (page == nullptr || text.empty())
        {
            return false;
        }

        auto read = ReadLayoutFromJson(text);

        if (!read.Succeeded || read.Document.Pages.empty())
        {
            return false;
        }

        auto copies = std::move(read.Document.Pages.front().Controls);
        auto const copiedGroups = std::move(read.Document.Pages.front().Groups);

        if (copies.empty() || page->Controls.size() + copies.size() > MaximumControlsPerPage)
        {
            return false;
        }

        // New ids, and every reference from one copy to another follows it to its new id. A
        // reference to a control that was not copied still means something if that control is
        // on this layout, and nothing at all if it is not, so it is kept or dropped on that.
        std::vector<std::pair<std::wstring, std::wstring>> renamed{};

        for (auto& copy : copies)
        {
            auto fresh = LayoutDocument::NewId();

            renamed.emplace_back(copy.Id, fresh);
            copy.Id = std::move(fresh);
        }

        auto const follow = [this, &renamed](std::wstring& id)
            {
                if (id.empty())
                {
                    return;
                }

                for (auto const& [from, to] : renamed)
                {
                    if (from == id)
                    {
                        id = to;
                        return;
                    }
                }

                if (m_document.FindControl(id) == nullptr)
                {
                    id.clear();
                }
            };

        for (auto& copy : copies)
        {
            follow(copy.Clock.TempoControlId);
            follow(copy.Feedback.TempoControlId);
        }

        auto const regrouped = RegroupCopies(copies);

        // Devices and sequences this layout does not have yet. One it already has by that name
        // is left alone: it is this layout's idea of what the name means.
        for (auto const& device : read.Document.Devices)
        {
            if (m_document.FindDevice(device.Name) == nullptr)
            {
                m_document.Devices.push_back(device);
            }
        }

        for (auto sequence : read.Document.Sequences)
        {
            if (m_document.FindSequence(sequence.Name) == nullptr)
            {
                for (auto& step : sequence.Steps)
                {
                    follow(step.TargetControlId);
                }

                m_document.Sequences.push_back(std::move(sequence));
            }
        }

        // Pasted over the originals, a step down and across, and another step for each paste
        // after that, so the copies are never hidden exactly under the controls they came from.
        auto const step = m_snap.GridSize > 0.0 ? m_snap.GridSize * 2.0 : 16.0;

        auto const coversSomething = [&copies, page](double offset)
            {
                for (auto const& copy : copies)
                {
                    for (auto const& existing : page->Controls)
                    {
                        if (std::abs(existing.X - (copy.X + offset)) < 0.5 &&
                            std::abs(existing.Y - (copy.Y + offset)) < 0.5 &&
                            std::abs(existing.Width - copy.Width) < 0.5 &&
                            std::abs(existing.Height - copy.Height) < 0.5)
                        {
                            return true;
                        }
                    }
                }

                return false;
            };

        auto offset = 0.0;

        for (int32_t attempt = 0; attempt < 64 && coversSomething(offset); ++attempt)
        {
            offset += step;
        }

        auto order = 0;

        for (auto const& control : page->Controls)
        {
            order = std::max(order, control.KeyboardOrder);
        }

        std::vector<std::wstring> selection{};

        for (auto& copy : copies)
        {
            copy.X += offset;
            copy.Y += offset;
            copy.KeyboardOrder = ++order;

            selection.push_back(copy.Id);
            page->Controls.push_back(std::move(copy));
        }

        NameCopiedGroups(*page, copiedGroups, regrouped);

        m_selection = std::move(selection);
        Commit(EditNames::Paste);

        return true;
    }

    _Use_decl_annotations_
    bool EditorController::PasteText(std::wstring const& text, double centerX, double centerY)
    {
        auto* const page = MutablePage();

        if (page == nullptr || page->Controls.size() >= MaximumControlsPerPage)
        {
            return false;
        }

        // One line. A label is a caption, and text copied out of a document arrives with line
        // breaks and tabs that would only print as nothing.
        std::wstring flattened{};
        flattened.reserve(std::min(text.size(), MaximumStringLength));

        for (auto const character : text)
        {
            if (flattened.size() >= MaximumStringLength)
            {
                break;
            }

            flattened.push_back(
                (character == L'\r' || character == L'\n' || character == L'\t') ? L' ' : character);
        }

        auto label = SanitizeStoredString(std::move(flattened));

        // Runs of spaces left behind by the line breaks.
        label.erase(
            std::unique(label.begin(), label.end(),
                [](wchar_t left, wchar_t right) { return left == L' ' && right == L' '; }),
            label.end());

        if (label.empty())
        {
            return false;
        }

        std::wstring deviceName{};

        if (!m_document.Devices.empty())
        {
            deviceName = m_document.Devices[0].Name;
        }

        auto control = MakeNewControl(
            ControlKind::Label, 0.0, 0.0, m_document.PageWidth, m_document.PageHeight, deviceName, *page);

        control.Label = label;

        // Wide enough for the words at the default text size, up to most of the page. The
        // customer resizes it from there, the same as a text control from the palette.
        auto const settings = EffectiveSnap();
        auto const grid = settings.GridSize > 0.0 ? settings.GridSize : DefaultGridSize;

        auto const wanted = (static_cast<double>(label.size()) * 8.0) + (grid * 2.0);

        control.Width = std::clamp(
            std::ceil(wanted / grid) * grid,
            control.Width,
            std::max(control.Width, std::floor(m_document.PageWidth * 0.8)));

        control.X = centerX - (control.Width / 2.0);
        control.Y = centerY - (control.Height / 2.0);

        if (settings.GridEnabled)
        {
            control.X = SnapToGrid(control.X, settings.GridSize);
            control.Y = SnapToGrid(control.Y, settings.GridSize);
        }

        auto const id = control.Id;

        page->Controls.push_back(std::move(control));

        m_selection = { id };
        Commit(EditNames::Paste);

        return true;
    }
}
