// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// The inspector. Nothing here is modal: it always reflects the selection, and an empty
// selection shows the page's own properties rather than going blank.

#include "pch.h"
#include "EditorWindow.xaml.h"

#include "StringResources.h"
#include "ControlFactory.h"
#include "SurfaceColors.h"
#include "HexText.h"

#include <format>

namespace resources = ::midiglass::resources;

namespace winrt::midiglass::implementation
{
    namespace
    {
        namespace shapes = ::winrt::Microsoft::UI::Xaml::Shapes;

        // The order the two combo boxes offer, and what each index means. Written once so the
        // index a combo hands back is never guessed at.
        constexpr glass::ControlKind KindOrder[]
        {
            glass::ControlKind::Knob,
            glass::ControlKind::Encoder,
            glass::ControlKind::Fader,
            glass::ControlKind::Pad,
            glass::ControlKind::Button,
            glass::ControlKind::Toggle,
            glass::ControlKind::XYPad,
            glass::ControlKind::Joystick,
            glass::ControlKind::Ribbon,
            glass::ControlKind::PianoKeyboard,
            glass::ControlKind::BeatClock,
            glass::ControlKind::TimeDisplay,
            glass::ControlKind::Meter,
            glass::ControlKind::Lamp,
            glass::ControlKind::Readout,
            glass::ControlKind::Label,
            glass::ControlKind::Image,
            glass::ControlKind::PageTab,
            glass::ControlKind::Panel,
        };

        constexpr wchar_t const* KindResourceKeys[]
        {
            L"PaletteKnob", L"PaletteEncoder", L"PaletteFader", L"PalettePad", L"PaletteButton",
            L"PaletteToggle", L"PaletteXYPad", L"PaletteJoystick", L"PaletteRibbon",
            L"PaletteKeyboard", L"PaletteBeatClock",
            L"PaletteTimeDisplay",
            L"PaletteMeter", L"PaletteLamp", L"PaletteReadout",
            L"PaletteLabel", L"PaletteImage", L"PalettePageTab", L"PalettePanel",
        };

        static_assert(std::size(KindOrder) == std::size(KindResourceKeys));

        constexpr glass::MessageTrigger TriggerOrder[]
        {
            glass::MessageTrigger::Changes,
            glass::MessageTrigger::TurnsOn,
            glass::MessageTrigger::TurnsOff,
            glass::MessageTrigger::Touched,
            glass::MessageTrigger::Released,
        };

        constexpr wchar_t const* TriggerResourceKeys[]
        {
            L"TriggerChanges", L"TriggerTurnsOn", L"TriggerTurnsOff", L"TriggerTouched", L"TriggerReleased",
        };

        constexpr glass::MessageKind MessageKindOrder[]
        {
            glass::MessageKind::ControlChange,
            glass::MessageKind::Note,
            glass::MessageKind::ProgramChange,
            glass::MessageKind::PitchBend,
            glass::MessageKind::ChannelPressure,
            glass::MessageKind::PerNoteController,
            glass::MessageKind::RegisteredController,
            glass::MessageKind::AssignedController,
            glass::MessageKind::SystemExclusive,
            glass::MessageKind::RawUmp,
            glass::MessageKind::Sequence,
            glass::MessageKind::GoToPage,
        };

        constexpr wchar_t const* MessageKindResourceKeys[]
        {
            L"MessageControlChange", L"MessageNote", L"MessageProgramChange", L"MessagePitchBend",
            L"MessageChannelPressure", L"MessagePerNoteController", L"MessageRegisteredController",
            L"MessageAssignedController", L"MessageSystemExclusive", L"MessageRawUmp",
            L"MessageSequence", L"MessageGoToPage",
        };

        static_assert(std::size(MessageKindOrder) == std::size(MessageKindResourceKeys));

        // The overrides, in the order the comp lists them. "Use the theme" is first, because it
        // is what almost every control stays at.
        constexpr glass::LabelPlacementOverride LabelPlacedOrder[]
        {
            glass::LabelPlacementOverride::UseTheme,
            glass::LabelPlacementOverride::Above,
            glass::LabelPlacementOverride::Below,
            glass::LabelPlacementOverride::InsideTop,
            glass::LabelPlacementOverride::InsideCenter,
            glass::LabelPlacementOverride::InsideBottom,
            glass::LabelPlacementOverride::VerticalLeft,
            glass::LabelPlacementOverride::VerticalRight,
            glass::LabelPlacementOverride::Custom,
            glass::LabelPlacementOverride::None,
        };

        constexpr wchar_t const* LabelPlacedResourceKeys[]
        {
            L"LabelPlacedTheme", L"LabelPlacedAbove", L"LabelPlacedBelow",
            L"LabelPlacedInsideTop", L"LabelPlacedInsideCenter", L"LabelPlacedInsideBottom",
            L"LabelPlacedVerticalLeft", L"LabelPlacedVerticalRight",
            L"LabelPlacedCustom", L"LabelPlacedNone",
        };

        static_assert(std::size(LabelPlacedOrder) == std::size(LabelPlacedResourceKeys));

        constexpr glass::ShowValueOverride ShowValueOrder[]
        {
            glass::ShowValueOverride::UseTheme,
            glass::ShowValueOverride::WhileTouched,
            glass::ShowValueOverride::Always,
            glass::ShowValueOverride::Never,
        };

        constexpr wchar_t const* ShowValueResourceKeys[]
        {
            L"ShowValueTheme", L"ShowValueWhileTouched", L"ShowValueAlways", L"ShowValueNever",
        };

        static_assert(std::size(ShowValueOrder) == std::size(ShowValueResourceKeys));

        constexpr glass::ControlStyleOverride StyleOrder[]
        {
            glass::ControlStyleOverride::Plate,
            glass::ControlStyleOverride::Outline,
            glass::ControlStyleOverride::Solid,
            glass::ControlStyleOverride::Bare,
        };

        template <typename T, size_t N>
        int32_t IndexOf(T const (&values)[N], _In_ T value) noexcept
        {
            for (size_t index = 0; index < N; ++index)
            {
                if (values[index] == value)
                {
                    return static_cast<int32_t>(index);
                }
            }

            return 0;
        }

        // What the Font button has been set to, in one line. Everything left at the theme's
        // value says so rather than being listed, because a caption that always reads the same
        // tells nobody anything.
        winrt::hstring DescribeLabelFont(_In_ glass::LabelStyle const& style)
        {
            std::vector<std::wstring> parts{};

            if (!style.FontFamily.empty()) { parts.push_back(style.FontFamily); }
            if (style.FontSize > 0.0) { parts.push_back(std::to_wstring(static_cast<int32_t>(style.FontSize)) + L" px"); }
            if (style.FontWeight > 0) { parts.push_back(std::to_wstring(style.FontWeight)); }
            if (style.Italic) { parts.push_back(std::wstring{ resources::GetString(L"FontItalic") }); }
            if (style.Underline) { parts.push_back(std::wstring{ resources::GetString(L"FontUnderline") }); }
            if (!style.Color.empty()) { parts.push_back(style.Color); }
            if (!style.Wrap) { parts.push_back(std::wstring{ resources::GetString(L"FontNoWrap") }); }
            if (style.HasBox()) { parts.push_back(std::wstring{ resources::GetString(L"FontCustomBox") }); }

            if (parts.empty())
            {
                return resources::GetString(L"FontAllFromTheme");
            }

            std::wstring text{};

            for (auto const& part : parts)
            {
                if (!text.empty()) { text += L" · "; }
                text += part;
            }

            return winrt::hstring{ text };
        }

        winrt::Windows::UI::Color ToColor(_In_ glass::ThemeColor const& color) noexcept
        {
            return winrt::Windows::UI::ColorHelper::FromArgb(255, color.R, color.G, color.B);
        }

        std::wstring FormatNumber(_In_ double value)
        {
            return std::to_wstring(static_cast<int32_t>(std::lround(value)));
        }

        double ParseNumber(_In_ winrt::hstring const& text, _In_ double fallback) noexcept
        {
            try
            {
                size_t consumed{ 0 };
                auto const parsed = std::stod(std::wstring{ text }, &consumed);

                return consumed == 0 ? fallback : parsed;
            }
            catch (...)
            {
                return fallback;
            }
        }
    }

    void EditorWindow::BuildInspectorChoices()
    {
        try
        {
            BuildControlPropertyChoices();

            for (auto const* const key : KindResourceKeys)
            {
                KindCombo().Items().Append(box_value(resources::GetString(key)));
            }

            for (auto const* const key : LabelPlacedResourceKeys)
            {
                LabelPlacedCombo().Items().Append(box_value(resources::GetString(key)));
            }

            for (auto const* const key : ShowValueResourceKeys)
            {
                ShowValueCombo().Items().Append(box_value(resources::GetString(key)));
            }

            for (auto const* const key : TriggerResourceKeys)
            {
                TriggerCombo().Items().Append(box_value(resources::GetString(key)));
            }

            for (auto const* const key : MessageKindResourceKeys)
            {
                KindMessageCombo().Items().Append(box_value(resources::GetString(key)));
            }

            GroupCombo().Items().Append(box_value(resources::GetString(L"GroupAll")));

            for (int32_t channel = 1; channel <= 16; ++channel)
            {
                ChannelCombo().Items().Append(box_value(
                    resources::FormatString(L"ChannelNumberFormat", std::to_wstring(channel))));
            }

            for (auto const* const key : { L"PickupJump", L"PickupCatch", L"PickupRelative" })
            {
                PickupCombo().Items().Append(box_value(resources::GetString(key)));
            }

            // Six hue slots and nothing else. A control stores a slot rather than a color, which
            // is what makes changing theme a six color operation instead of a redesign.
            //
            // The swatch is the whole tile rather than a small square inside a button, because
            // the comp's row of color is the thing being chosen; a button around it is chrome
            // that pushes six swatches wider than the pane.
            for (int32_t slot = 0; slot < glass::HueSlotCount; ++slot)
            {
                controls::Button swatch{};

                swatch.Width(26);
                swatch.Height(26);
                swatch.MinWidth(0);
                swatch.Padding({ 0, 0, 0, 0 });
                swatch.CornerRadius({ 5, 5, 5, 5 });
                swatch.BorderThickness({ 0, 0, 0, 0 });
                swatch.Background(media::SolidColorBrush(
                    ToColor(m_theme.HueSlots[static_cast<size_t>(slot)])));
                swatch.Tag(box_value(slot));

                xaml::Automation::AutomationProperties::SetName(swatch, resources::FormatString(
                    L"HueSlotAccessibleFormat", std::to_wstring(slot + 1)));

                swatch.Click({ this, &EditorWindow::OnHueSlotClick });

                HueSlotHost().Items().Append(swatch);
            }

            // A color of this control's own, outside the theme. Last, and drawn as a spectrum,
            // because it is the one choice that will not move when the theme changes.
            {
                controls::Button custom{};

                custom.Width(26);
                custom.Height(26);
                custom.MinWidth(0);
                custom.Padding({ 0, 0, 0, 0 });
                custom.CornerRadius({ 5, 5, 5, 5 });
                custom.BorderThickness({ 0, 0, 0, 0 });
                custom.Tag(box_value(glass::LiteralHue));
                custom.IsEnabled(false);

                media::LinearGradientBrush spectrum{};

                spectrum.StartPoint({ 0.0, 0.0 });
                spectrum.EndPoint({ 1.0, 1.0 });

                constexpr uint32_t wheel[]{ 0xFF4D4D, 0xFFE04D, 0x5BE87F, 0x4CE0FF, 0x6B7BFF, 0xFF6BD6 };

                for (size_t i = 0; i < std::size(wheel); ++i)
                {
                    media::GradientStop stop{};

                    stop.Offset(static_cast<double>(i) / (std::size(wheel) - 1));
                    stop.Color(winrt::Windows::UI::ColorHelper::FromArgb(
                        255,
                        static_cast<uint8_t>((wheel[i] >> 16) & 0xFF),
                        static_cast<uint8_t>((wheel[i] >> 8) & 0xFF),
                        static_cast<uint8_t>(wheel[i] & 0xFF)));

                    spectrum.GradientStops().Append(stop);
                }

                custom.Background(spectrum);

                xaml::Automation::AutomationProperties::SetName(
                    custom, resources::GetString(L"HueSlotCustomName"));

                controls::ToolTipService::SetToolTip(
                    custom, box_value(resources::GetString(L"HueSlotCustomName")));

                HueSlotHost().Items().Append(custom);
            }
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to build the inspector choices.")
    }

    glass::Control const* EditorWindow::SingleSelectedControl() const
    {
        auto const selected = m_editor.SelectedControls();

        return selected.size() == 1 ? selected[0] : nullptr;
    }

    // The position and size boxes and nothing else, for the pointer-move path.
    void EditorWindow::RefreshInspectorGeometry()
    {
        try
        {
            auto const* const control = SingleSelectedControl();

            if (control == nullptr)
            {
                return;
            }

            auto const previous = m_updatingInspector;
            m_updatingInspector = true;

            BoundsX().Text(winrt::hstring{ FormatNumber(control->X) });
            BoundsY().Text(winrt::hstring{ FormatNumber(control->Y) });
            BoundsWidth().Text(winrt::hstring{ FormatNumber(control->Width) });
            BoundsHeight().Text(winrt::hstring{ FormatNumber(control->Height) });

            m_updatingInspector = previous;
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to refresh the position and size.")
    }

    void EditorWindow::RefreshInspector()
    {
        try
        {
            auto const previous = m_updatingInspector;
            m_updatingInspector = true;

            // The monitor rail follows the selection, because "is this thing sending what I
            // think it is" is a question about one control.
            RebuildMonitorList();

            auto const* const control = SingleSelectedControl();
            auto const selectedCount = m_editor.Selection().size();

            LookTab().IsEnabled(control != nullptr);
            BehaviorTab().IsEnabled(control != nullptr);
            MidiTab().IsEnabled(control != nullptr);
            MidiInTab().IsEnabled(control != nullptr);

            if (control == nullptr)
            {
                auto const* const page = m_editor.CurrentPage();

                InspectorKindText().Text(resources::GetString(
                    selectedCount > 1 ? L"InspectorManySelected" : L"InspectorPageKind"));

                InspectorTitleText().Text(selectedCount > 1
                    ? resources::FormatString(L"EditorSelectedFormat", std::to_wstring(selectedCount))
                    : winrt::hstring{ page == nullptr || page->Name.empty()
                        ? std::wstring{ m_editor.Document().Name }
                        : page->Name });

                // "11 selected" is a count, not a name. Nothing to rename, so nothing to type in.
                InspectorTitleText().IsReadOnly(selectedCount > 1 || page == nullptr);

                // Emptied and greyed, not left as they were. Leaving the last control's numbers
                // under a heading that says "Page" is what made three disabled tabs look like
                // three broken ones.
                BoundsX().Text(L"");
                BoundsY().Text(L"");
                BoundsWidth().Text(L"");
                BoundsHeight().Text(L"");
                LabelBox().Text(L"");
                LabelFontCaption().Text(L"");

                LookPane().IsEnabled(false);
                MidiPane().IsEnabled(false);
                MidiInPane().IsEnabled(false);
                BehaviorPane().IsEnabled(false);

                m_preview.Teardown();

                m_messageIndex = -1;
                MessageList().Items().Clear();

                m_updatingInspector = previous;
                return;
            }

            LookPane().IsEnabled(true);
            MidiPane().IsEnabled(true);
            MidiInPane().IsEnabled(true);
            BehaviorPane().IsEnabled(true);

            auto const kindIndex = IndexOf(KindOrder, control->Kind);

            InspectorKindText().Text(resources::GetString(KindResourceKeys[kindIndex]));

            InspectorTitleText().Text(winrt::hstring{
                control->Label.empty()
                    ? std::wstring{ resources::GetString(L"InspectorUnnamedControl") }
                    : control->Label });

            InspectorTitleText().IsReadOnly(false);

            BoundsX().Text(winrt::hstring{ FormatNumber(control->X) });
            BoundsY().Text(winrt::hstring{ FormatNumber(control->Y) });
            BoundsWidth().Text(winrt::hstring{ FormatNumber(control->Width) });
            BoundsHeight().Text(winrt::hstring{ FormatNumber(control->Height) });

            AspectLockToggle().IsChecked(control->AspectLocked);

            LabelBox().Text(winrt::hstring{ control->Label });
            KindCombo().SelectedIndex(kindIndex);
            PickupCombo().SelectedIndex(static_cast<int32_t>(control->Pickup));

            LabelPlacedCombo().SelectedIndex(IndexOf(LabelPlacedOrder,
                control->LabelPlaced == glass::LabelPlacementOverride::Inside
                    ? glass::LabelPlacementOverride::InsideBottom
                    : control->LabelPlaced));

            ShowValueCombo().SelectedIndex(IndexOf(ShowValueOrder, control->ShowValue));

            LabelWidthBox().Value(control->LabelLook.WidthPercent);
            LabelFontCaption().Text(DescribeLabelFont(control->LabelLook));

            SyncStyleSegments(control->Style);
            RefreshPreview(*control);

            SendOnStartSwitch().IsOn(control->SendsValueOnStart);
            DefaultValueSlider().Value(control->DefaultValue * 100.0);
            ReturnsToDefaultSwitch().IsOn(control->ReturnsToDefault);
            SendIntervalBox().Value(control->SendIntervalMilliseconds);
            KeyboardOrderBox().Value(control->KeyboardOrder);

            // A control that only displays has nothing to send, so the MIDI tab would be a page
            // of controls that do nothing. It is disabled rather than hidden, because a tab that
            // comes and goes is harder to find than one that is visibly not for this control.
            MidiTab().IsEnabled(glass::SendsAnything(control->Kind));

            // The mirror of that: text, a picture and a frame are not driven by anything, and
            // nothing about them changes over time either.
            auto const passive =
                control->Kind == glass::ControlKind::Label ||
                control->Kind == glass::ControlKind::Image ||
                control->Kind == glass::ControlKind::Panel;

            MidiInTab().IsEnabled(!passive);
            BehaviorTab().IsEnabled(!passive);

            // A disabled tab cannot be left showing, or the pane reads as broken rather than as
            // not applicable.
            if ((m_inspectorTab == 1 && !MidiTab().IsEnabled()) ||
                (m_inspectorTab == 2 && !MidiInTab().IsEnabled()) ||
                (m_inspectorTab == 3 && !BehaviorTab().IsEnabled()))
            {
                SelectInspectorTab(0);
            }

            RefreshKindPanels(*control);
            RefreshFeedbackPanel(*control);

            RefreshMessageList();

            m_updatingInspector = previous;
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to refresh the inspector.")
    }

    void EditorWindow::RefreshMessageList()
    {
        try
        {
            MessageList().Items().Clear();

            auto const* const control = SingleSelectedControl();

            if (control == nullptr)
            {
                m_messageIndex = -1;
                RefreshMessageFields();
                return;
            }

            for (size_t index = 0; index < control->Messages.size(); ++index)
            {
                auto const& message = control->Messages[index];

                auto const text = resources::FormatString(
                    L"MessageRowFormat",
                    resources::GetString(TriggerResourceKeys[IndexOf(TriggerOrder, message.Trigger)]),
                    resources::GetString(MessageKindResourceKeys[IndexOf(MessageKindOrder, message.Kind)]),
                    std::to_wstring(message.Number),
                    message.DeviceName.empty()
                        ? std::wstring{ resources::GetString(L"MessageNoDevice") }
                        : message.DeviceName);

                controls::ListViewItem item{};

                item.Content(box_value(text));
                item.Tag(box_value(static_cast<int32_t>(index)));

                xaml::Automation::AutomationProperties::SetName(item, text);

                MessageList().Items().Append(item);
            }

            if (control->Messages.empty())
            {
                m_messageIndex = -1;
            }
            else
            {
                if (m_messageIndex < 0 || m_messageIndex >= static_cast<int32_t>(control->Messages.size()))
                {
                    m_messageIndex = 0;
                }

                MessageList().SelectedIndex(m_messageIndex);
            }

            RemoveMessageButton().IsEnabled(m_messageIndex >= 0);

            RefreshMessageFields();
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to refresh the message list.")
    }

    void EditorWindow::RefreshMessageFields()
    {
        try
        {
            auto const previous = m_updatingInspector;
            m_updatingInspector = true;

            // The device list is rebuilt here rather than once at startup, because renaming a
            // destination is a one place edit and every row has to follow it.
            DeviceCombo().Items().Clear();

            for (auto const& device : m_editor.Document().Devices)
            {
                DeviceCombo().Items().Append(box_value(winrt::hstring{ device.Name }));
            }

            auto const* const control = SingleSelectedControl();

            auto const valid = control != nullptr &&
                m_messageIndex >= 0 &&
                m_messageIndex < static_cast<int32_t>(control->Messages.size());

            TriggerCombo().IsEnabled(valid);
            KindMessageCombo().IsEnabled(valid);
            DeviceCombo().IsEnabled(valid);
            GroupCombo().IsEnabled(valid);
            ChannelCombo().IsEnabled(valid);
            MessageNumberBox().IsEnabled(valid);

            if (!valid)
            {
                SysExPanel().Visibility(xaml::Visibility::Collapsed);
                RawWordsPanel().Visibility(xaml::Visibility::Collapsed);
                SequencePanel().Visibility(xaml::Visibility::Collapsed);
                TargetPagePanel().Visibility(xaml::Visibility::Collapsed);

                m_updatingInspector = previous;
                return;
            }

            auto const& message = control->Messages[static_cast<size_t>(m_messageIndex)];

            TriggerCombo().SelectedIndex(IndexOf(TriggerOrder, message.Trigger));
            KindMessageCombo().SelectedIndex(IndexOf(MessageKindOrder, message.Kind));

            auto deviceIndex = -1;

            for (size_t index = 0; index < m_editor.Document().Devices.size(); ++index)
            {
                if (m_editor.Document().Devices[index].Name == message.DeviceName)
                {
                    deviceIndex = static_cast<int32_t>(index);
                    break;
                }
            }

            DeviceCombo().SelectedIndex(deviceIndex);
            DeviceResolvedText().Text(DescribeResolvedDevice(message.DeviceName));

            RefreshGroupChoices(message.DeviceName, message.GroupIndex);

            ChannelCombo().SelectedIndex(std::clamp(message.ChannelIndex, 0, 15));
            MessageNumberBox().Value(message.Number);

            RefreshMessageKindFields(message);

            m_updatingInspector = previous;
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to refresh the message fields.")
    }

    // The groups this message could sensibly go to. A device that declares four of them has
    // twelve that go nowhere, and a control pointed at one of those is a control that silently
    // does nothing. The group the message already uses is always offered, so opening a layout
    // built against other hardware never quietly rewrites it.
    _Use_decl_annotations_
    void EditorWindow::RefreshGroupChoices(std::wstring const& deviceName, int32_t selectedGroup)
    {
        auto const previous = m_updatingInspector;
        m_updatingInspector = true;

        std::array<bool, glass::MaximumGroupCount> offered{};
        offered.fill(true);

        auto declared = false;

        if (!m_showAllGroups)
        {
            if (auto const* const device = m_editor.Document().FindDevice(deviceName))
            {
                auto const resolved = midiapp::EndpointCatalog::Current().Resolve(
                    device->Match, device->MatchMode, device->Match.TransportSuppliedEndpointName);

                if (resolved.has_value() && resolved->DestinationGroupCount() > 0)
                {
                    offered = resolved->DestinationGroups;
                    declared = true;
                }
            }
        }

        GroupCombo().Items().Clear();
        m_groupChoices.clear();

        GroupCombo().Items().Append(box_value(resources::GetString(L"GroupAll")));
        m_groupChoices.push_back(glass::AllGroups);

        auto chosen = 0;

        for (int32_t group = 0; group < glass::MaximumGroupCount; ++group)
        {
            if (!offered[static_cast<size_t>(group)] && group != selectedGroup)
            {
                continue;
            }

            GroupCombo().Items().Append(box_value(
                resources::FormatString(L"GroupNumberFormat", std::to_wstring(group + 1))));

            m_groupChoices.push_back(group);

            if (group == selectedGroup)
            {
                chosen = static_cast<int32_t>(m_groupChoices.size()) - 1;
            }
        }

        GroupCombo().SelectedIndex(chosen);

        AllGroupsCheck().IsChecked(m_showAllGroups);

        GroupSourceText().Text(declared
            ? resources::FormatString(
                L"GroupsFromDeviceFormat",
                std::to_wstring(static_cast<int32_t>(m_groupChoices.size()) - 1))
            : winrt::hstring{});

        m_updatingInspector = previous;
    }

    _Use_decl_annotations_
    void EditorWindow::OnShowAllGroupsChanged(
        foundation::IInspectable const& sender,
        xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        if (m_updatingInspector || !m_loaded)
        {
            return;
        }

        m_showAllGroups = AllGroupsCheck().IsChecked().GetBoolean();

        RefreshMessageFields();
    }

    // Which of the message rows make sense for this kind. A dump has no controller number and a
    // control change has no bytes, so showing a field that cannot be answered is worse than
    // hiding it.
    _Use_decl_annotations_
    void EditorWindow::RefreshMessageKindFields(glass::Control const& control)
    {
        if (m_messageIndex < 0 || m_messageIndex >= static_cast<int32_t>(control.Messages.size()))
        {
            return;
        }

        RefreshMessageKindFields(control.Messages[static_cast<size_t>(m_messageIndex)]);

        // A clock generates its own stream. Its row is there so it knows where to send, and
        // asking somebody to type the words it will send would be a question with no answer.
        if (control.Kind == glass::ControlKind::BeatClock)
        {
            RawWordsPanel().Visibility(xaml::Visibility::Collapsed);
            DetentPanel().Visibility(xaml::Visibility::Collapsed);
            MessageChannelLabel().Visibility(xaml::Visibility::Collapsed);
            ChannelCombo().Visibility(xaml::Visibility::Collapsed);
            MessageNumberLabel().Visibility(xaml::Visibility::Collapsed);
            MessageNumberBox().Visibility(xaml::Visibility::Collapsed);
        }

        // Same for a keyboard: the key decides the note, so the number on the row is not a
        // question anybody can answer.
        if (control.Kind == glass::ControlKind::PianoKeyboard)
        {
            MessageNumberLabel().Visibility(xaml::Visibility::Collapsed);
            MessageNumberBox().Visibility(xaml::Visibility::Collapsed);
        }
    }

    _Use_decl_annotations_
    void EditorWindow::RefreshMessageKindFields(glass::ControlMessage const& message)
    {
        auto const show = [](xaml::UIElement const& element, bool visible)
            {
                element.Visibility(visible ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);
            };

        auto const kind = message.Kind;

        auto const isChannelVoice =
            kind != glass::MessageKind::SystemExclusive &&
            kind != glass::MessageKind::RawUmp &&
            kind != glass::MessageKind::Sequence &&
            kind != glass::MessageKind::GoToPage;

        auto const usesDevice = kind != glass::MessageKind::Sequence && kind != glass::MessageKind::GoToPage;
        auto const usesGroup = usesDevice;

        show(MessageDeviceLabel(), usesDevice);
        show(DevicePanel(), usesDevice);
        show(MessageGroupLabel(), usesGroup);
        show(GroupPanel(), usesGroup);
        show(MessageChannelLabel(), isChannelVoice);
        show(ChannelCombo(), isChannelVoice);
        show(MessageNumberLabel(), isChannelVoice);
        show(MessageNumberBox(), isChannelVoice);

        show(SysExPanel(), kind == glass::MessageKind::SystemExclusive);
        show(RawWordsPanel(), kind == glass::MessageKind::RawUmp);
        show(SequencePanel(), kind == glass::MessageKind::Sequence);
        show(TargetPagePanel(), kind == glass::MessageKind::GoToPage);

        // Stops only mean something on a message that carries a position. There is nothing to
        // step through on a page change or a dump.
        auto const stepping =
            kind == glass::MessageKind::ControlChange ||
            kind == glass::MessageKind::PitchBend ||
            kind == glass::MessageKind::ChannelPressure ||
            kind == glass::MessageKind::PerNoteController ||
            kind == glass::MessageKind::RegisteredController ||
            kind == glass::MessageKind::AssignedController;

        show(DetentPanel(), stepping);

        if (stepping)
        {
            DetentModeCombo().SelectedIndex(static_cast<int32_t>(message.Detents.Mode));

            show(DetentStepBox(), message.Detents.Mode == glass::DetentMode::EvenSteps);
            show(DetentStopsPanel(), message.Detents.Mode == glass::DetentMode::ExplicitValues);

            DetentStepBox().Value(message.Detents.Step);

            if (message.Detents.Mode == glass::DetentMode::ExplicitValues)
            {
                RebuildDetentStopRows();
            }

            DetentCaption().Text(
                message.Detents.Mode == glass::DetentMode::Continuous
                    ? resources::GetString(L"DetentContinuousCaption")
                    : message.Detents.Mode == glass::DetentMode::EvenSteps
                        ? resources::GetString(L"DetentStepCaption")
                        : resources::GetString(L"DetentListCaption"));
        }

        if (kind == glass::MessageKind::SystemExclusive)
        {
            SysExBox().Text(winrt::hstring{ glass::FormatHexBytes(message.SystemExclusive) });

            SysExCaption().Text(message.SystemExclusive.empty()
                ? resources::GetString(L"SysExEmpty")
                : resources::FormatString(
                    L"SysExByteCountFormat", static_cast<int32_t>(message.SystemExclusive.size())));
        }
        else if (kind == glass::MessageKind::RawUmp)
        {
            RawWordsBox().Text(winrt::hstring{ glass::FormatHexWords(message.RawWords) });
        }
        else if (kind == glass::MessageKind::Sequence)
        {
            RefreshSequenceChoices(message.SequenceName);
        }
        else if (kind == glass::MessageKind::GoToPage)
        {
            TargetPageCombo().Items().Clear();

            auto selected = -1;

            for (size_t index = 0; index < m_editor.Document().Pages.size(); ++index)
            {
                auto const& page = m_editor.Document().Pages[index];

                TargetPageCombo().Items().Append(box_value(winrt::hstring{ page.Name }));

                if (page.Id == message.TargetPageId)
                {
                    selected = static_cast<int32_t>(index);
                }
            }

            TargetPageCombo().SelectedIndex(selected);
        }
    }

    _Use_decl_annotations_
    void EditorWindow::RefreshSequenceChoices(std::wstring const& selectedName)
    {
        SequenceCombo().Items().Clear();

        auto selected = -1;

        for (size_t index = 0; index < m_editor.Document().Sequences.size(); ++index)
        {
            auto const& sequence = m_editor.Document().Sequences[index];

            SequenceCombo().Items().Append(box_value(winrt::hstring{ sequence.Name }));

            if (sequence.Name == selectedName)
            {
                selected = static_cast<int32_t>(index);
            }
        }

        SequenceCombo().SelectedIndex(selected);

        auto const* const sequence = m_editor.Document().FindSequence(selectedName);

        EditSequenceButton().IsEnabled(sequence != nullptr);

        SequenceCaption().Text(sequence == nullptr
            ? resources::GetString(L"SequenceNoneChosen")
            : resources::FormatString(
                L"SequenceStepCountFormat", static_cast<int32_t>(sequence->Steps.size())));
    }

    // ---------------------------------------------------------------- look

    _Use_decl_annotations_
    void EditorWindow::OnBoundsChanged(foundation::IInspectable const& sender, controls::TextChangedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        if (m_updatingInspector)
        {
            return;
        }

        try
        {
            auto const* const control = SingleSelectedControl();

            if (control == nullptr)
            {
                return;
            }

            auto const id = control->Id;

            // Typed in, so it is exact and never snapped. The grid is for dragging.
            auto const x = ParseNumber(BoundsX().Text(), control->X);
            auto const y = ParseNumber(BoundsY().Text(), control->Y);
            auto const width = ParseNumber(BoundsWidth().Text(), control->Width);
            auto const height = ParseNumber(BoundsHeight().Text(), control->Height);

            // The inspector is deliberately not refreshed here. Rewriting the box somebody is
            // typing in would move their caret, and a half-typed number would be rounded under
            // their hands.
            if (m_editor.SetControlBounds(id, x, y, width, height))
            {
                RebuildSurface();
                UpdateStatusBar();
                MarkChanged();
            }
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to set the control bounds.")
    }

    _Use_decl_annotations_
    void EditorWindow::OnAspectLockToggled(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        if (m_updatingInspector)
        {
            return;
        }

        auto const* const control = SingleSelectedControl();

        if (control != nullptr &&
            m_editor.SetControlAspectLocked(control->Id, AspectLockToggle().IsChecked().GetBoolean()))
        {
            MarkChanged();
        }
    }

    _Use_decl_annotations_
    void EditorWindow::OnLabelChanged(foundation::IInspectable const& sender, controls::TextChangedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(args);

        if (m_updatingInspector)
        {
            return;
        }

        // The name is edited in two places — the heading and the Label row — and they have to
        // stay in step without setting each other off.
        auto const fromTitle = sender.try_as<controls::TextBox>() == InspectorTitleText();

        auto const* const control = SingleSelectedControl();

        if (control == nullptr)
        {
            // With nothing selected the heading is the PAGE's name, so typing in it renames the
            // page. Doing nothing here is what made a rename look as though it had taken and
            // then vanish the moment anything refreshed the box.
            if (fromTitle && m_editor.Selection().empty())
            {
                if (m_editor.RenamePage(m_editor.PageIndex(), std::wstring{ InspectorTitleText().Text() }))
                {
                    RebuildPageRail();
                    MarkChanged();
                }
            }

            return;
        }

        auto const text = std::wstring{ fromTitle ? InspectorTitleText().Text() : LabelBox().Text() };

        auto const id = control->Id;

        if (m_editor.SetControlLabel(id, text))
        {
            RebuildSurface();
            RebuildOutline();

            auto const previous = m_updatingInspector;
            m_updatingInspector = true;

            if (fromTitle)
            {
                LabelBox().Text(winrt::hstring{ text });
            }
            else
            {
                InspectorTitleText().Text(winrt::hstring{ text });
            }

            m_updatingInspector = previous;

            MarkChanged();
        }
    }

    _Use_decl_annotations_
    void EditorWindow::OnLabelWidthChanged(
        controls::NumberBox const& sender,
        controls::NumberBoxValueChangedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        if (m_updatingInspector)
        {
            return;
        }

        auto const* const control = SingleSelectedControl();
        auto const value = LabelWidthBox().Value();

        if (control == nullptr || !std::isfinite(value))
        {
            return;
        }

        auto style = control->LabelLook;
        style.WidthPercent = value;

        if (m_editor.SetControlLabelStyle(control->Id, style))
        {
            RebuildSurface();
            MarkChanged();
        }
    }

    _Use_decl_annotations_
    void EditorWindow::OnLabelFontClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        if (auto const* const control = SingleSelectedControl())
        {
            ShowLabelFontDialog(control->Id);
        }
    }

    _Use_decl_annotations_
    void EditorWindow::OnKindChanged(foundation::IInspectable const& sender, controls::SelectionChangedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        if (m_updatingInspector)
        {
            return;
        }

        auto const* const control = SingleSelectedControl();
        auto const index = KindCombo().SelectedIndex();

        if (control == nullptr || index < 0 || index >= static_cast<int32_t>(std::size(KindOrder)))
        {
            return;
        }

        if (m_editor.SetControlKind(control->Id, KindOrder[index]))
        {
            RebuildSurface();
            RebuildOutline();
            RefreshInspector();
            MarkChanged();
        }
    }

    // ---------------------------------------------------------------- theme overrides

    _Use_decl_annotations_
    void EditorWindow::SyncStyleSegments(glass::ControlStyleOverride style)
    {
        StyleThemeSegment().IsChecked(style == glass::ControlStyleOverride::UseTheme);
        StylePlateSegment().IsChecked(style == glass::ControlStyleOverride::Plate);
        StyleOutlineSegment().IsChecked(style == glass::ControlStyleOverride::Outline);
        StyleSolidSegment().IsChecked(style == glass::ControlStyleOverride::Solid);
        StyleBareSegment().IsChecked(style == glass::ControlStyleOverride::Bare);
    }

    _Use_decl_annotations_
    void EditorWindow::OnStyleChecked(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(args);

        if (m_updatingInspector || !m_loaded)
        {
            return;
        }

        try
        {
            auto const toggle = sender.try_as<controls::Primitives::ToggleButton>();
            auto const* const control = SingleSelectedControl();

            if (toggle == nullptr || control == nullptr)
            {
                return;
            }

            auto const tag = std::wstring{ winrt::unbox_value_or<winrt::hstring>(toggle.Tag(), L"") };

            auto const style =
                tag == L"theme" ? glass::ControlStyleOverride::UseTheme :
                tag == L"plate" ? glass::ControlStyleOverride::Plate :
                tag == L"outline" ? glass::ControlStyleOverride::Outline :
                tag == L"solid" ? glass::ControlStyleOverride::Solid :
                glass::ControlStyleOverride::Bare;

            auto const id = control->Id;

            // One group, so picking a segment turns the others off. Nothing else does that for
            // a row of ToggleButtons.
            auto const previous = m_updatingInspector;
            m_updatingInspector = true;
            SyncStyleSegments(style);
            m_updatingInspector = previous;

            if (m_editor.SetControlStyle(id, style))
            {
                RebuildSurface();
                RefreshInspector();
                MarkChanged();
            }
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to change the control style.")
    }

    _Use_decl_annotations_
    void EditorWindow::OnStyleUnchecked(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(args);

        if (m_updatingInspector || !m_loaded)
        {
            return;
        }

        try
        {
            auto const toggle = sender.try_as<controls::Primitives::ToggleButton>();
            auto const* const control = SingleSelectedControl();

            if (toggle == nullptr || control == nullptr)
            {
                return;
            }

            // Clicking the segment that is already on leaves it on. Turning the last one off
            // would show a style group that claims the control is drawn no way at all.
            auto const tag = std::wstring{ winrt::unbox_value_or<winrt::hstring>(toggle.Tag(), L"") };

            auto const style =
                tag == L"theme" ? glass::ControlStyleOverride::UseTheme :
                tag == L"plate" ? glass::ControlStyleOverride::Plate :
                tag == L"outline" ? glass::ControlStyleOverride::Outline :
                tag == L"solid" ? glass::ControlStyleOverride::Solid :
                glass::ControlStyleOverride::Bare;

            if (style == control->Style)
            {
                auto const previous = m_updatingInspector;
                m_updatingInspector = true;
                toggle.IsChecked(true);
                m_updatingInspector = previous;
            }
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to return the control style to the theme.")
    }

    _Use_decl_annotations_
    void EditorWindow::OnLabelPlacedChanged(
        foundation::IInspectable const& sender,
        controls::SelectionChangedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        if (m_updatingInspector)
        {
            return;
        }

        auto const* const control = SingleSelectedControl();
        auto const index = LabelPlacedCombo().SelectedIndex();

        if (control == nullptr || index < 0 || index >= static_cast<int32_t>(std::size(LabelPlacedOrder)))
        {
            return;
        }

        auto const placement = LabelPlacedOrder[index];

        auto const id = control->Id;

        // Choosing a placement from the list is choosing a rule, so it throws away the box that
        // was dragged by hand. Custom itself is not choosable from here: it only means "wherever
        // the handles were dragged to", and with no box there is nothing for it to mean.
        auto changed = placement == glass::LabelPlacementOverride::Custom
            ? false
            : m_editor.ClearControlLabelBox(id);

        if (placement != glass::LabelPlacementOverride::Custom)
        {
            changed = m_editor.SetControlLabelPlacement(id, placement) || changed;
        }

        if (changed)
        {
            RebuildSurface();
            RefreshInspector();
            MarkChanged();
        }
        else
        {
            // Picking Custom with nothing dragged leaves the list saying something the control
            // is not doing, so it snaps back to what is true.
            RefreshInspector();
        }
    }

    _Use_decl_annotations_
    void EditorWindow::OnShowValueChanged(
        foundation::IInspectable const& sender,
        controls::SelectionChangedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        if (m_updatingInspector)
        {
            return;
        }

        auto const* const control = SingleSelectedControl();
        auto const index = ShowValueCombo().SelectedIndex();

        if (control == nullptr || index < 0 || index >= static_cast<int32_t>(std::size(ShowValueOrder)))
        {
            return;
        }

        if (m_editor.SetControlShowValue(control->Id, ShowValueOrder[index]))
        {
            MarkChanged();
        }
    }

    // ---------------------------------------------------------------- the tabs

    namespace
    {
        // A Tag set in XAML is a string, whatever it looks like. Unboxing one as an int gives
        // the fallback silently, which turned the tab handlers into a pair that called each
        // other until the stack ran out.
        int32_t TagIndex(_In_ foundation::IInspectable const& sender) noexcept
        {
            try
            {
                auto const element = sender.try_as<xaml::FrameworkElement>();

                if (element == nullptr)
                {
                    return 0;
                }

                auto const tag = std::wstring{ winrt::unbox_value_or<winrt::hstring>(element.Tag(), L"0") };

                return tag.empty() ? 0 : std::stoi(tag);
            }
            catch (...)
            {
                return 0;
            }
        }
    }

    // Underline tabs behave like radio buttons: exactly one is on, and unpicking the one that is
    // on puts it straight back rather than leaving the strip showing none.
    _Use_decl_annotations_
    void EditorWindow::SelectInspectorTab(int32_t index)
    {
        try
        {
            m_inspectorTab = index;

            LookTab().IsChecked(index == 0);
            MidiTab().IsChecked(index == 1);
            MidiInTab().IsChecked(index == 2);
            BehaviorTab().IsChecked(index == 3);

            auto const show = [](xaml::UIElement const& element, bool visible)
                {
                    element.Visibility(visible ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);
                };

            show(LookTabLine(), index == 0);
            show(MidiTabLine(), index == 1);
            show(MidiInTabLine(), index == 2);
            show(BehaviorTabLine(), index == 3);

            show(LookPane(), index == 0);
            show(MidiPane(), index == 1);
            show(MidiInPane(), index == 2);
            show(BehaviorPane(), index == 3);
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to change the inspector tab.")
    }

    _Use_decl_annotations_
    void EditorWindow::OnInspectorTabChecked(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(args);

        if (!m_loaded)
        {
            return;
        }

        if (auto const toggle = sender.try_as<controls::Primitives::ToggleButton>())
        {
            SelectInspectorTab(TagIndex(sender));
        }
    }

    _Use_decl_annotations_
    void EditorWindow::OnInspectorTabUnchecked(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(args);

        if (!m_loaded)
        {
            return;
        }

        if (auto const toggle = sender.try_as<controls::Primitives::ToggleButton>())
        {
            if (TagIndex(sender) == m_inspectorTab)
            {
                toggle.IsChecked(true);
            }
        }
    }

    _Use_decl_annotations_
    void EditorWindow::SelectLeftTab(int32_t index)
    {
        try
        {
            m_leftTab = index;

            PaletteTab().IsChecked(index == 0);
            OutlineTab().IsChecked(index == 1);

            auto const show = [](xaml::UIElement const& element, bool visible)
                {
                    element.Visibility(visible ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);
                };

            show(PaletteTabLine(), index == 0);
            show(OutlineTabLine(), index == 1);

            show(PalettePane(), index == 0);
            show(OutlinePane(), index == 1);
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to change the left pane tab.")
    }

    _Use_decl_annotations_
    void EditorWindow::OnLeftTabChecked(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(args);

        if (!m_loaded)
        {
            return;
        }

        if (auto const toggle = sender.try_as<controls::Primitives::ToggleButton>())
        {
            SelectLeftTab(TagIndex(sender));
        }
    }

    _Use_decl_annotations_
    void EditorWindow::OnLeftTabUnchecked(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(args);

        if (!m_loaded)
        {
            return;
        }

        if (auto const toggle = sender.try_as<controls::Primitives::ToggleButton>())
        {
            if (TagIndex(sender) == m_leftTab)
            {
                toggle.IsChecked(true);
            }
        }
    }

    // ---------------------------------------------------------------- the header menu

    _Use_decl_annotations_
    void EditorWindow::OnDuplicateSelectionClick(
        foundation::IInspectable const& sender,
        xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        if (m_editor.DuplicateSelection())
        {
            RebuildSurface();
            RebuildOutline();
            RefreshInspector();
            UpdateStatusBar();
            MarkChanged();
        }
    }

    _Use_decl_annotations_
    void EditorWindow::OnDeleteSelectionClick(
        foundation::IInspectable const& sender,
        xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        if (m_editor.DeleteSelection())
        {
            RebuildSurface();
            RebuildOutline();
            RefreshInspector();
            UpdateStatusBar();
            MarkChanged();
        }
    }

    // The control drawn the way it will actually look, so a style or a color is judged rather
    // than imagined. One control on its own deck, at whatever size fits the box.
    _Use_decl_annotations_
    void EditorWindow::RefreshPreview(glass::Control const& control)
    {
        try
        {
            PreviewDeck().Fill(glass::MakeDeckBrush(m_theme.Deck));

            glass::LayoutDocument single{};

            single.PageWidth = 244;
            single.PageHeight = 92;

            glass::Page page{};

            auto copy = control;

            // Centered at its own size, unless it is bigger than the box.
            auto const scale = std::min(1.0,
                std::min(200.0 / std::max(1.0, control.Width), 64.0 / std::max(1.0, control.Height)));

            copy.Width = std::max(8.0, control.Width * scale);
            copy.Height = std::max(8.0, control.Height * scale);
            copy.X = (single.PageWidth - copy.Width) / 2.0;
            copy.Y = (single.PageHeight - copy.Height) / 2.0 - 6.0;

            page.Controls.push_back(copy);
            single.Pages.push_back(std::move(page));

            PreviewCanvas().Width(single.PageWidth);
            PreviewCanvas().Height(single.PageHeight);

            m_preview.Teardown();
            m_preview.Build(PreviewCanvas(), single, m_theme, 0);
            m_preview.SetValue(0, control.DefaultValue > 0.0 ? control.DefaultValue : 0.62);

            if (auto const element = m_preview.ElementAt(0))
            {
                element.IsHitTestVisible(false);

                // The inspector above already names the control; a screen reader reading the
                // preview too would say everything twice.
                xaml::Automation::AutomationProperties::SetAccessibilityView(
                    element, xaml::Automation::Peers::AccessibilityView::Raw);
            }
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to draw the preview.")
    }

    _Use_decl_annotations_
    void EditorWindow::OnHueSlotClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(args);

        auto const* const control = SingleSelectedControl();
        auto const button = sender.try_as<controls::Button>();

        if (control == nullptr || button == nullptr)
        {
            return;
        }

        auto const slot = winrt::unbox_value_or<int32_t>(button.Tag(), 0);

        if (m_editor.SetControlHueSlot(control->Id, slot))
        {
            RebuildSurface();
            MarkChanged();
        }
    }

    // ---------------------------------------------------------------- what it sends

    _Use_decl_annotations_
    void EditorWindow::OnMessageSelectionChanged(
        foundation::IInspectable const& sender,
        controls::SelectionChangedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        if (m_updatingInspector)
        {
            return;
        }

        m_messageIndex = MessageList().SelectedIndex();

        RemoveMessageButton().IsEnabled(m_messageIndex >= 0);

        RefreshMessageFields();
    }

    _Use_decl_annotations_
    void EditorWindow::OnAddMessageClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        auto const* const control = SingleSelectedControl();

        if (control != nullptr && m_editor.AddMessage(control->Id))
        {
            m_messageIndex = static_cast<int32_t>(SingleSelectedControl()->Messages.size()) - 1;

            RefreshMessageList();
            MarkChanged();
        }
    }

    _Use_decl_annotations_
    void EditorWindow::OnRemoveMessageClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        auto const* const control = SingleSelectedControl();

        if (control != nullptr && m_messageIndex >= 0 &&
            m_editor.RemoveMessage(control->Id, static_cast<size_t>(m_messageIndex)))
        {
            m_messageIndex = -1;

            RefreshMessageList();
            MarkChanged();
        }
    }

    _Use_decl_annotations_
    void EditorWindow::OnMessageFieldChanged(
        foundation::IInspectable const& sender,
        controls::SelectionChangedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        if (m_updatingInspector)
        {
            return;
        }

        try
        {
            auto const* const control = SingleSelectedControl();

            if (control == nullptr ||
                m_messageIndex < 0 ||
                m_messageIndex >= static_cast<int32_t>(control->Messages.size()))
            {
                return;
            }

            auto message = control->Messages[static_cast<size_t>(m_messageIndex)];

            auto const triggerIndex = TriggerCombo().SelectedIndex();
            auto const kindIndex = KindMessageCombo().SelectedIndex();
            auto const deviceIndex = DeviceCombo().SelectedIndex();
            auto const groupIndex = GroupCombo().SelectedIndex();
            auto const channelIndex = ChannelCombo().SelectedIndex();

            if (triggerIndex >= 0 && triggerIndex < static_cast<int32_t>(std::size(TriggerOrder)))
            {
                message.Trigger = TriggerOrder[triggerIndex];
            }

            if (kindIndex >= 0 && kindIndex < static_cast<int32_t>(std::size(MessageKindOrder)))
            {
                message.Kind = MessageKindOrder[kindIndex];
            }

            if (deviceIndex >= 0 && deviceIndex < static_cast<int32_t>(m_editor.Document().Devices.size()))
            {
                message.DeviceName = m_editor.Document().Devices[static_cast<size_t>(deviceIndex)].Name;
            }

            // The list only holds the groups the device declares, so its index is not the group
            // number. m_groupChoices is what turns one into the other.
            if (groupIndex >= 0 && groupIndex < static_cast<int32_t>(m_groupChoices.size()))
            {
                message.GroupIndex = m_groupChoices[static_cast<size_t>(groupIndex)];
            }

            if (channelIndex >= 0)
            {
                message.ChannelIndex = channelIndex;
            }

            auto const id = control->Id;
            auto const index = static_cast<size_t>(m_messageIndex);

            if (m_editor.SetMessage(id, index, message))
            {
                RefreshMessageList();
                MarkChanged();
            }
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to change the message.")
    }

    _Use_decl_annotations_
    void EditorWindow::OnMessageNumberChanged(
        controls::NumberBox const& sender,
        controls::NumberBoxValueChangedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        if (m_updatingInspector)
        {
            return;
        }

        auto const* const control = SingleSelectedControl();

        if (control == nullptr ||
            m_messageIndex < 0 ||
            m_messageIndex >= static_cast<int32_t>(control->Messages.size()))
        {
            return;
        }

        auto const value = MessageNumberBox().Value();

        if (!std::isfinite(value))
        {
            return;
        }

        auto message = control->Messages[static_cast<size_t>(m_messageIndex)];

        message.Number = static_cast<uint32_t>(std::clamp(std::lround(value), 0L, 127L));

        auto const id = control->Id;
        auto const index = static_cast<size_t>(m_messageIndex);

        if (m_editor.SetMessage(id, index, message))
        {
            RefreshMessageList();
            MarkChanged();
        }
    }

    // ---------------------------------------------------------------- behavior

    _Use_decl_annotations_
    void EditorWindow::OnSendOnStartToggled(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        if (m_updatingInspector)
        {
            return;
        }

        auto const* const control = SingleSelectedControl();

        if (control != nullptr &&
            m_editor.SetControlSendsValueOnStart(control->Id, SendOnStartSwitch().IsOn()))
        {
            MarkChanged();
        }
    }

    _Use_decl_annotations_
    void EditorWindow::OnReturnsToDefaultToggled(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        if (m_updatingInspector)
        {
            return;
        }

        auto const* const control = SingleSelectedControl();

        if (control != nullptr &&
            m_editor.SetControlReturnsToDefault(control->Id, ReturnsToDefaultSwitch().IsOn()))
        {
            // The surface reads this when it attaches the pointer handlers, so it has to be
            // rebuilt for the change to be felt rather than only saved.
            RebuildSurface();
            MarkChanged();
        }
    }

    _Use_decl_annotations_
    void EditorWindow::OnDefaultValueChanged(
        foundation::IInspectable const& sender,
        controls::Primitives::RangeBaseValueChangedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        if (m_updatingInspector)
        {
            return;
        }

        auto const* const control = SingleSelectedControl();

        if (control != nullptr &&
            m_editor.SetControlDefaultValue(control->Id, DefaultValueSlider().Value() / 100.0))
        {
            MarkChanged();
        }
    }

    _Use_decl_annotations_
    void EditorWindow::OnSendIntervalChanged(
        controls::NumberBox const& sender,
        controls::NumberBoxValueChangedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        if (m_updatingInspector)
        {
            return;
        }

        auto const value = SendIntervalBox().Value();
        auto const* const control = SingleSelectedControl();

        if (control != nullptr && std::isfinite(value) &&
            m_editor.SetControlSendInterval(control->Id, static_cast<int32_t>(std::lround(value))))
        {
            MarkChanged();
        }
    }

    _Use_decl_annotations_
    void EditorWindow::OnPickupChanged(
        foundation::IInspectable const& sender,
        controls::SelectionChangedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        if (m_updatingInspector)
        {
            return;
        }

        auto const index = PickupCombo().SelectedIndex();
        auto const* const control = SingleSelectedControl();

        if (control != nullptr && index >= 0 && index <= 2 &&
            m_editor.SetControlPickup(control->Id, static_cast<glass::PickupMode>(index)))
        {
            MarkChanged();
        }
    }

    // ---------------------------------------------------------------- the message payloads

    bool EditorWindow::TryEditSelectedMessage(_In_ std::function<bool(glass::ControlMessage&)> const& edit)
    {
        auto const* const control = SingleSelectedControl();

        if (control == nullptr ||
            m_messageIndex < 0 ||
            m_messageIndex >= static_cast<int32_t>(control->Messages.size()))
        {
            return false;
        }

        auto message = control->Messages[static_cast<size_t>(m_messageIndex)];

        if (!edit(message))
        {
            return false;
        }

        auto const id = control->Id;
        auto const index = static_cast<size_t>(m_messageIndex);

        if (!m_editor.SetMessage(id, index, message))
        {
            return false;
        }

        RefreshMessageList();
        MarkChanged();

        return true;
    }

    _Use_decl_annotations_
    void EditorWindow::OnSysExChanged(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        if (m_updatingInspector)
        {
            return;
        }

        try
        {
            auto const text = std::wstring{ SysExBox().Text() };

            auto const bytes = glass::ParseHexBytes(text, glass::MaximumSystemExclusiveBytes);

            // Nothing readable means nothing is written. Half a dump is worse than none, and
            // saying so is more use than quietly keeping the last good value.
            if (bytes.empty() && !text.empty())
            {
                SysExCaption().Text(resources::GetString(L"SysExNotReadable"));
                return;
            }

            TryEditSelectedMessage([&bytes](glass::ControlMessage& message)
                {
                    if (message.SystemExclusive == bytes)
                    {
                        return false;
                    }

                    message.SystemExclusive = bytes;
                    return true;
                });

            SysExCaption().Text(bytes.empty()
                ? resources::GetString(L"SysExEmpty")
                : resources::FormatString(L"SysExByteCountFormat", static_cast<int32_t>(bytes.size())));
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to read the system exclusive bytes.")
    }

    _Use_decl_annotations_
    void EditorWindow::OnRawWordsChanged(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        if (m_updatingInspector)
        {
            return;
        }

        try
        {
            auto const words = glass::ParseHexWords(std::wstring{ RawWordsBox().Text() }, 4);

            TryEditSelectedMessage([&words](glass::ControlMessage& message)
                {
                    if (message.RawWords == words)
                    {
                        return false;
                    }

                    message.RawWords = words;
                    return true;
                });
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to read the raw words.")
    }

    _Use_decl_annotations_
    void EditorWindow::OnSequenceChanged(
        foundation::IInspectable const& sender,
        controls::SelectionChangedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        if (m_updatingInspector)
        {
            return;
        }

        try
        {
            auto const index = SequenceCombo().SelectedIndex();

            if (index < 0 || index >= static_cast<int32_t>(m_editor.Document().Sequences.size()))
            {
                return;
            }

            auto const name = m_editor.Document().Sequences[static_cast<size_t>(index)].Name;

            if (TryEditSelectedMessage([&name](glass::ControlMessage& message)
                {
                    if (message.SequenceName == name)
                    {
                        return false;
                    }

                    message.SequenceName = name;
                    return true;
                }))
            {
                RefreshSequenceChoices(name);
            }
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to choose the sequence.")
    }

    _Use_decl_annotations_
    void EditorWindow::OnTargetPageChanged(
        foundation::IInspectable const& sender,
        controls::SelectionChangedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        if (m_updatingInspector)
        {
            return;
        }

        try
        {
            auto const index = TargetPageCombo().SelectedIndex();

            if (index < 0 || index >= static_cast<int32_t>(m_editor.Document().Pages.size()))
            {
                return;
            }

            auto const id = m_editor.Document().Pages[static_cast<size_t>(index)].Id;

            TryEditSelectedMessage([&id](glass::ControlMessage& message)
                {
                    if (message.TargetPageId == id)
                    {
                        return false;
                    }

                    message.TargetPageId = id;
                    return true;
                });
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to choose the page.")
    }

    _Use_decl_annotations_
    void EditorWindow::OnSysExFromFileClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        try
        {
            std::vector<uint8_t> bytes{};

            if (!TryReadSystemExclusiveFile(bytes))
            {
                return;
            }

            TryEditSelectedMessage([&bytes](glass::ControlMessage& message)
                {
                    message.SystemExclusive = bytes;
                    return true;
                });

            RefreshMessageFields();
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to read the system exclusive file.")
    }

    _Use_decl_annotations_
    void EditorWindow::OnNewSequenceClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        try
        {
            auto const name = m_editor.AddSequence(std::wstring{ resources::GetString(L"SequenceDefaultName") });

            if (name.empty())
            {
                return;
            }

            // The new sequence is what the row plays, so the next thing somebody does is add
            // steps to it rather than hunting for it in a list.
            TryEditSelectedMessage([&name](glass::ControlMessage& message)
                {
                    message.SequenceName = name;
                    return true;
                });

            RefreshSequenceChoices(name);
            MarkChanged();

            ShowSequenceDialog(name);
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to add a sequence.")
    }

    _Use_decl_annotations_
    void EditorWindow::OnEditSequenceClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        auto const* const control = SingleSelectedControl();

        if (control == nullptr ||
            m_messageIndex < 0 ||
            m_messageIndex >= static_cast<int32_t>(control->Messages.size()))
        {
            return;
        }

        auto const name = control->Messages[static_cast<size_t>(m_messageIndex)].SequenceName;

        if (!name.empty())
        {
            ShowSequenceDialog(name);
        }
    }

    _Use_decl_annotations_
    void EditorWindow::OnKeyboardOrderChanged(
        controls::NumberBox const& sender,
        controls::NumberBoxValueChangedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        if (m_updatingInspector)
        {
            return;
        }

        auto const value = KeyboardOrderBox().Value();
        auto const* const control = SingleSelectedControl();

        if (control != nullptr && std::isfinite(value) &&
            m_editor.SetKeyboardOrder(control->Id, static_cast<int32_t>(std::lround(value))))
        {
            RebuildOutline();
            UpdateOverlay();
            MarkChanged();
        }
    }
}
