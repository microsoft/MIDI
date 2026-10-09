// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MainWindow.xaml.h"

#include "BackgroundWork.h"
#include "DialogParts.h"
#include "StringResources.h"
#include "ThemeBrushes.h"

using namespace winrt::Microsoft::UI::Xaml;

namespace patchbay = ::midipatchbay;
namespace resources = ::midipatchbay::resources;

namespace winrt::midipatchbay::implementation
{
    namespace
    {
        // A white key per natural, black keys drawn over the joins. Narrow enough that all ten
        // and a half octaves fit the dialog without scrolling.
        constexpr double WhiteKeyWidth = 9.0;
        constexpr double WhiteKeyHeight = 62.0;
        constexpr double BlackKeyWidth = 6.0;
        constexpr double BlackKeyHeight = 38.0;

        constexpr bool IsBlackKey(_In_ uint8_t note) noexcept
        {
            switch (note % 12)
            {
            case 1: case 3: case 6: case 8: case 10: return true;
            default: return false;
            }
        }

        // A keyboard is white and black whatever the app theme is, and the theme text brushes
        // are translucent white in dark mode, which would paint the sharps white.
        winrt::Windows::UI::Color KeyColor(_In_ bool black) noexcept
        {
            winrt::Windows::UI::Color color{};

            color.A = 255;
            color.R = color.G = color.B = black ? 0x22 : 0xF2;

            return color;
        }

        // The same hue as the accent, dark enough to still read as a sharp.
        winrt::Windows::UI::Color DarkenedAccent(_In_ media::Brush const& accent) noexcept
        {
            winrt::Windows::UI::Color color{};

            color.A = 255;
            color.R = 0x1E;
            color.G = 0x4B;
            color.B = 0x60;

            try
            {
                if (auto const solid = accent.try_as<media::SolidColorBrush>())
                {
                    auto const source = solid.Color();

                    color.R = static_cast<uint8_t>(source.R * 0.42);
                    color.G = static_cast<uint8_t>(source.G * 0.42);
                    color.B = static_cast<uint8_t>(source.B * 0.42);
                }
            }
            catch (...)
            {
            }

            return color;
        }

        // How many naturals come before this note, which is its x position in white key widths.
        int WhiteKeysBefore(_In_ uint8_t note) noexcept
        {
            int count{ 0 };

            for (uint8_t i = 0; i < note; i++)
            {
                if (!IsBlackKey(i))
                {
                    count++;
                }
            }

            return count;
        }

        using patchbay::parts::Card;
        using patchbay::parts::Heading;
        using patchbay::parts::HeadingHint;

        controls::NumberBox NumberField(
            _In_ winrt::hstring const& header,
            _In_ double minimum,
            _In_ double maximum,
            _In_ double value)
        {
            auto box = patchbay::parts::NumberBox(header, minimum, maximum, value, 12);

            box.Width(150);

            return box;
        }

        // Two choices side by side, for "let them through" against "keep them out" and the like.
        controls::RadioButtons ChoiceButtons(
            _In_ winrt::hstring const& header,
            _In_ std::initializer_list<winrt::hstring> choices,
            _In_ int32_t selected)
        {
            controls::RadioButtons buttons{};

            buttons.Header(winrt::box_value(header));
            buttons.MaxColumns(static_cast<int32_t>(choices.size()));

            for (auto const& choice : choices)
            {
                buttons.Items().Append(winrt::box_value(choice));
            }

            buttons.SelectedIndex(selected);

            return buttons;
        }

        // Numbers in the mask filter are typed in decimal or hexadecimal. "0x" always means hex.
        std::optional<uint32_t> ParseMaskNumber(_In_ std::wstring_view text, _In_ bool hex) noexcept
        {
            while (!text.empty() && ::iswspace(text.front()))
            {
                text.remove_prefix(1);
            }

            while (!text.empty() && ::iswspace(text.back()))
            {
                text.remove_suffix(1);
            }

            auto base = hex ? 16 : 10;

            if (text.size() > 2 && text[0] == L'0' && (text[1] == L'x' || text[1] == L'X'))
            {
                text.remove_prefix(2);
                base = 16;
            }

            // Eight hex digits or ten decimal ones are the most a 32-bit field can need.
            if (text.empty() || text.size() > 10)
            {
                return std::nullopt;
            }

            uint64_t value{ 0 };

            for (auto const c : text)
            {
                uint32_t digit{ 0 };

                if (c >= L'0' && c <= L'9')
                {
                    digit = static_cast<uint32_t>(c - L'0');
                }
                else if (base == 16 && c >= L'a' && c <= L'f')
                {
                    digit = static_cast<uint32_t>(c - L'a' + 10);
                }
                else if (base == 16 && c >= L'A' && c <= L'F')
                {
                    digit = static_cast<uint32_t>(c - L'A' + 10);
                }
                else
                {
                    return std::nullopt;
                }

                value = value * static_cast<uint64_t>(base) + digit;

                if (value > 0xFFFFFFFFull)
                {
                    return std::nullopt;
                }
            }

            return static_cast<uint32_t>(value);
        }

        std::wstring FormatMaskNumber(_In_ uint32_t value, _In_ bool hex) noexcept
        {
            try
            {
                return hex ? std::format(L"0x{:X}", value) : std::to_wstring(value);
            }
            catch (...)
            {
            }

            return {};
        }

        // The places in a message people most often want to look at, so a mask can be set up
        // without counting bits.
        struct MaskField
        {
            wchar_t const* NameKey;
            uint8_t Word;
            uint8_t HighBit;
            uint8_t LowBit;
        };

        constexpr MaskField MaskFields[]
        {
            { L"MaskFieldMessageType", 0, 31, 28 },
            { L"MaskFieldGroup", 0, 27, 24 },
            { L"MaskFieldStatus", 0, 23, 20 },
            { L"MaskFieldChannel", 0, 19, 16 },
            { L"MaskFieldIndex", 0, 14, 8 },
            { L"MaskFieldMidi1Value", 0, 6, 0 },
            { L"MaskFieldMidi2Velocity", 1, 31, 16 },
            { L"MaskFieldMidi2Value", 1, 31, 0 },
        };
    }

    // The dialog edits a copy. Nothing reaches the step until Apply, so Cancel really does leave
    // a running patch alone.
    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::ShowBlockDialogAsync(std::wstring blockId)
    {
        auto strong = get_strong();

        try
        {
            // One at a time: the dialog's sections are built for the step being edited, so a
            // second request would rebuild them under the first.
            if (!m_editingBlockId.empty())
            {
                co_return;
            }

            auto* patch = CurrentPatch();
            auto const* block = patch == nullptr ? nullptr : patch->FindBlock(blockId);

            if (block == nullptr)
            {
                co_return;
            }

            // Everything an annotation has fits in the inspector.
            if (patchbay::IsAnnotation(block->Kind))
            {
                FocusAnnotationText(blockId);
                co_return;
            }

            if (EditsInInspector(block->Kind))
            {
                FocusStepSettings(blockId);
                co_return;
            }

            m_editingBlockId = blockId;

            auto const editingDone = wil::scope_exit([this]() { m_editingBlockId.clear(); });

            m_editingKind = block->Kind;
            m_editingSettings = block->Settings;
            m_editingFilter = block->Settings.Filter;
            m_editingTransform = block->Settings.Transform;
            m_learnedLow = false;

            // Captured now, so a change behind the dialog cannot move where a test note plays.
            FindTestDestination(blockId);

            PrepareTransformRows();
            BuildBlockDialog();

            BlockDialog().Title(winrt::box_value(patchbay::BlockDisplayName(*block)));
            BlockDialogIntroText().Text(patchbay::BlockKindHint(block->Kind));
            BlockDialog().XamlRoot(Content().XamlRoot());

            FitBlockDialogToWindow();

            auto const result = co_await BlockDialog().ShowAsync();

            StopLearning();

            if (result == controls::ContentDialogResult::None)
            {
                co_return;
            }

            // Looked up again: the step can go while the dialog is open.
            patch = CurrentPatch();
            auto* target = patch == nullptr ? nullptr : patch->FindBlock(blockId);

            if (target == nullptr)
            {
                co_return;
            }

            auto settings = result == controls::ContentDialogResult::Secondary
                ? patchbay::DefaultBlockSettings(target->Kind)
                : EditedSettings();

            // Through the file format and back, so what is kept is exactly what a saved patch
            // reads back as.
            settings = patchbay::BlockSettingsFromJson(target->Kind,
                patchbay::BlockSettingsToJson(target->Kind, settings));

            if (patchbay::BlockSettingsSignature(target->Kind, settings) ==
                patchbay::BlockSettingsSignature(target->Kind, target->Settings))
            {
                co_return;
            }

            target->Settings = std::move(settings);

            CommitChange(true);
        }
        catch (...)
        {
            StopLearning();
            MIDI_PATCHBAY_LOG_GENERAL_EXCEPTION(L"Unable to edit the step.");
        }
    }

    ::midipatchbay::BlockSettings MainWindow::EditedSettings() noexcept
    {
        auto settings = m_editingSettings;

        try
        {
            CommitTransformMaps();

            // Every part a kind uses is always on. Whether it changes anything is a question for
            // the part itself.
            settings.Filter = m_editingFilter;
            settings.Filter.IsActive = true;

            settings.Transform = m_editingTransform;
            settings.Transform.IsActive = true;
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to collect the step's settings.")

        return settings;
    }

    void MainWindow::UpdateBlockSummary() noexcept
    {
        try
        {
            auto const settings = EditedSettings();

            BlockSummaryText().Text(resources::FormatString(L"BlockDialogSummaryFormat",
                patchbay::DescribeBlock(m_editingKind, settings)));

            DrawVelocityCurve();
            DrawShapePreviews();
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to summarize the step.")
    }

    void MainWindow::BuildBlockDialog() noexcept
    {
        try
        {
            BlockDialogContent().Children().Clear();

            // Every handle a section kept goes with it.
            m_valueModeButtons = nullptr;
            m_valueLowBox = nullptr;
            m_valueHighBox = nullptr;
            m_valueOneBox = nullptr;
            m_valueRangePanel = nullptr;
            m_valueOnePanel = nullptr;
            m_valueSetText = nullptr;
            m_learnButton = nullptr;
            m_learnStatusText = nullptr;
            m_valueKeyFills.clear();
            m_valueToggles.clear();
            m_maskConditionsPanel = nullptr;

            m_velocityCurveCanvas = nullptr;
            m_fixedVelocityBox = nullptr;
            m_minimumVelocityBox = nullptr;
            m_maximumVelocityBox = nullptr;
            m_velocityRescaleCheck = nullptr;
            m_aftertouchPreview = nullptr;
            m_controlValuePanel = nullptr;
            m_controlValuePreviews.clear();
            m_shapeRangeBoxes.clear();

            m_lfoRateCaption = nullptr;
            m_lfoIntervalCaption = nullptr;
            m_lfoNumberBox = nullptr;
            m_lfoBankBox = nullptr;
            m_lfoIndexBox = nullptr;
            m_lfoProtocolButtons = nullptr;
            m_parameterRowsPanel = nullptr;
            m_parameterCurveCanvases.clear();
            m_gateTriggerPanels = { nullptr, nullptr };

            switch (m_editingKind)
            {
            case patchbay::BlockKind::MessageTypeFilter:
                BuildMessageTypeSections();
                break;

            case patchbay::BlockKind::NoteFilter:
            case patchbay::BlockKind::ControlChangeFilter:
                BuildValueSetSection();
                break;

            case patchbay::BlockKind::MessageMaskFilter:
                BuildMaskSection();
                break;

            case patchbay::BlockKind::LfoGenerator:
                BuildLfoGeneratorSections();
                break;

            case patchbay::BlockKind::ParameterFilter:
                BuildParameterFilterSection();
                break;

            case patchbay::BlockKind::ParameterTransform:
                BuildParameterTransformSection();
                break;

            case patchbay::BlockKind::Gate:
                BuildGateSections();
                break;

            default:
                BuildTransformSections();
                break;
            }

            UpdateBlockSummary();
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to build the step dialog.")
    }

    void MainWindow::BuildMessageTypeSections() noexcept
    {
        try
        {
            auto weak = get_weak();

            // A check box wired straight at one bool in the working copy.
            auto const addCheck = [weak](
                controls::Panel const& parent,
                winrt::hstring const& text,
                bool* flag)
                {
                    controls::CheckBox check{};

                    check.Content(winrt::box_value(text));
                    check.IsChecked(*flag);
                    check.MinWidth(0);

                    check.Checked([weak, flag](auto&&, auto&&)
                        {
                            if (auto s = weak.get()) { *flag = true; s->UpdateBlockSummary(); }
                        });

                    check.Unchecked([weak, flag](auto&&, auto&&)
                        {
                            if (auto s = weak.get()) { *flag = false; s->UpdateBlockSummary(); }
                        });

                    parent.Children().Append(check);

                    return check;
                };

            auto const makeGrid = []()
                {
                    controls::VariableSizedWrapGrid grid{};

                    grid.Orientation(controls::Orientation::Horizontal);
                    grid.MaximumRowsOrColumns(3);
                    grid.ItemWidth(228);
                    grid.ItemHeight(30);

                    return grid;
                };

            // ------------------------------------------------------ message types
            {
                controls::StackPanel body{};
                body.Spacing(4);

                body.Children().Append(Heading(resources::GetString(L"FilterSectionMessageTypes")));
                body.Children().Append(HeadingHint(resources::GetString(L"FilterSectionMessageTypesHint")));

                auto grid = makeGrid();

                for (size_t i = 0; i < patchbay::MessageTypeCount; i++)
                {
                    // The reserved types have nothing to say to a customer, and listing eight of
                    // them would bury the six that matter.
                    switch (i)
                    {
                    case 0x0: case 0x1: case 0x2: case 0x3:
                    case 0x4: case 0x5: case 0xD: case 0xF:
                        break;
                    default:
                        continue;
                    }

                    addCheck(grid, patchbay::DescribeMessageType(static_cast<uint8_t>(i)),
                        &m_editingFilter.MessageTypes[i]);
                }

                body.Children().Append(grid);

                BlockDialogContent().Children().Append(Card(body));
            }

            // -------------------------------------------- channel voice messages
            {
                controls::StackPanel body{};
                body.Spacing(4);

                body.Children().Append(Heading(resources::GetString(L"FilterSectionChannelVoice")));
                body.Children().Append(HeadingHint(resources::GetString(L"FilterSectionChannelVoiceHint")));

                auto grid = makeGrid();

                // Note off and note on together: nobody wants one without the other, and a stuck
                // note is what you get when they are split by accident.
                {
                    controls::CheckBox notes{};

                    notes.Content(winrt::box_value(resources::GetString(L"VoiceNotes")));
                    notes.IsChecked(m_editingFilter.ChannelVoiceStatuses[0x8] &&
                        m_editingFilter.ChannelVoiceStatuses[0x9]);

                    auto const setNotes = [weak](bool value)
                        {
                            if (auto s = weak.get())
                            {
                                s->m_editingFilter.ChannelVoiceStatuses[0x8] = value;
                                s->m_editingFilter.ChannelVoiceStatuses[0x9] = value;
                                s->UpdateBlockSummary();
                            }
                        };

                    notes.Checked([setNotes](auto&&, auto&&) { setNotes(true); });
                    notes.Unchecked([setNotes](auto&&, auto&&) { setNotes(false); });

                    grid.Children().Append(notes);
                }

                for (uint8_t status = 0; status < patchbay::ChannelVoiceStatusCount; status++)
                {
                    if (status == 0x8 || status == 0x9 || status == 0x7)
                    {
                        continue;
                    }

                    addCheck(grid, patchbay::DescribeChannelVoiceStatus(status),
                        &m_editingFilter.ChannelVoiceStatuses[status]);
                }

                body.Children().Append(grid);

                BlockDialogContent().Children().Append(Card(body));
            }

            // -------------------------------------------------- system messages
            {
                controls::StackPanel body{};
                body.Spacing(4);

                body.Children().Append(Heading(resources::GetString(L"FilterSectionSystem")));
                body.Children().Append(HeadingHint(resources::GetString(L"FilterSectionSystemHint")));

                auto grid = makeGrid();

                for (size_t i = 0; i < patchbay::SystemMessageCount; i++)
                {
                    addCheck(grid, patchbay::DescribeSystemMessage(patchbay::SystemMessageList[i]),
                        &m_editingFilter.SystemMessages[i]);
                }

                body.Children().Append(grid);

                BlockDialogContent().Children().Append(Card(body));
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to build the message type sections.")
    }

    void MainWindow::BuildValueSetSection() noexcept
    {
        try
        {
            auto weak = get_weak();
            auto const isNotes = m_editingKind == patchbay::BlockKind::NoteFilter;
            auto const& values = m_editingSettings.Values;

            controls::StackPanel body{};
            body.Spacing(8);

            body.Children().Append(Heading(resources::GetString(isNotes ? L"FilterSectionNotes" : L"FilterSectionControllers")));
            body.Children().Append(HeadingHint(resources::GetString(isNotes ? L"FilterSectionNotesHint" : L"FilterSectionControllersHint")));

            // ---- what happens to them
            auto action = ChoiceButtons(
                resources::GetString(L"FilterActionHeader"),
                { resources::GetString(L"FilterActionLetThrough"), resources::GetString(L"FilterActionKeepOut") },
                values.Action == patchbay::FilterAction::KeepOut ? 1 : 0);

            action.SelectionChanged([weak](foundation::IInspectable const& sender, auto&&)
                {
                    auto s = weak.get();
                    auto const list = sender.try_as<controls::RadioButtons>();

                    if (s == nullptr || list == nullptr || list.SelectedIndex() < 0 || s->m_updatingValueSet)
                    {
                        return;
                    }

                    s->m_editingSettings.Values.Action = list.SelectedIndex() == 1
                        ? patchbay::FilterAction::KeepOut
                        : patchbay::FilterAction::LetThrough;

                    s->RefreshValueSetUi();
                    s->UpdateBlockSummary();
                });

            body.Children().Append(action);

            // ---- which ones
            m_valueModeButtons = ChoiceButtons(
                resources::GetString(L"FilterValuesHeader"),
                {
                    resources::GetString(L"FilterModeRange"),
                    resources::GetString(isNotes ? L"FilterModeOneNote" : L"FilterModeOneController"),
                    resources::GetString(L"FilterModeList"),
                },
                static_cast<int32_t>(values.Mode));

            m_valueModeButtons.SelectionChanged([weak](foundation::IInspectable const& sender, auto&&)
                {
                    auto s = weak.get();
                    auto const list = sender.try_as<controls::RadioButtons>();

                    if (s == nullptr || list == nullptr || list.SelectedIndex() < 0 || s->m_updatingValueSet)
                    {
                        return;
                    }

                    s->m_editingSettings.Values.Mode = static_cast<patchbay::ValueSetMode>(std::clamp(list.SelectedIndex(), 0, 2));
                    s->m_learnedLow = false;

                    s->RefreshValueSetUi();
                    s->UpdateBlockSummary();
                });

            body.Children().Append(m_valueModeButtons);

            // ---- the numbers, for the modes that use them
            auto const addNumber = [weak](controls::Panel const& parent, winrt::hstring const& header, uint8_t value,
                std::function<void(patchbay::ValueSetFilter&, uint8_t)> apply)
                {
                    auto box = NumberField(header, 0, 127, value);

                    box.ValueChanged([weak, apply](auto&&, controls::NumberBoxValueChangedEventArgs const& args)
                        {
                            auto s = weak.get();

                            if (s == nullptr || s->m_updatingValueSet || std::isnan(args.NewValue()))
                            {
                                return;
                            }

                            apply(s->m_editingSettings.Values, static_cast<uint8_t>(std::clamp(args.NewValue(), 0.0, 127.0)));

                            s->RefreshValueSetUi();
                            s->UpdateBlockSummary();
                        });

                    parent.Children().Append(box);

                    return box;
                };

            m_valueRangePanel = controls::StackPanel{};
            m_valueRangePanel.Orientation(controls::Orientation::Horizontal);
            m_valueRangePanel.Spacing(12);

            m_valueLowBox = addNumber(m_valueRangePanel,
                resources::GetString(isNotes ? L"FilterLowestNote" : L"FilterLowestController"), values.Lowest,
                [](patchbay::ValueSetFilter& set, uint8_t value)
                {
                    set.Lowest = value;
                    set.Highest = (std::max)(set.Highest, value);
                });

            m_valueHighBox = addNumber(m_valueRangePanel,
                resources::GetString(isNotes ? L"FilterHighestNote" : L"FilterHighestController"), values.Highest,
                [](patchbay::ValueSetFilter& set, uint8_t value)
                {
                    set.Highest = value;
                    set.Lowest = (std::min)(set.Lowest, value);
                });

            body.Children().Append(m_valueRangePanel);

            m_valueOnePanel = controls::StackPanel{};
            m_valueOnePanel.Orientation(controls::Orientation::Horizontal);
            m_valueOnePanel.Spacing(12);

            m_valueOneBox = addNumber(m_valueOnePanel,
                resources::GetString(isNotes ? L"FilterOneNote" : L"FilterOneController"), values.One,
                [](patchbay::ValueSetFilter& set, uint8_t value) { set.One = value; });

            body.Children().Append(m_valueOnePanel);

            m_valueSetText = controls::TextBlock{};
            m_valueSetText.FontSize(13);
            m_valueSetText.TextWrapping(xaml::TextWrapping::Wrap);
            body.Children().Append(m_valueSetText);

            // ---- learn from a device
            {
                controls::StackPanel learnRow{};
                learnRow.Orientation(controls::Orientation::Horizontal);
                learnRow.Spacing(12);

                m_learnButton = primitives::ToggleButton{};
                m_learnButton.Content(winrt::box_value(resources::GetString(L"FilterLearn")));

                // Click rather than Checked, so turning it off in code does not loop back here.
                m_learnButton.Click([weak](auto&&, auto&&)
                    {
                        auto s = weak.get();

                        if (s == nullptr || s->m_learnButton == nullptr)
                        {
                            return;
                        }

                        auto const checked = s->m_learnButton.IsChecked();

                        if (checked != nullptr && checked.Value())
                        {
                            s->StartLearningAsync();
                        }
                        else
                        {
                            s->StopLearning();
                        }
                    });

                learnRow.Children().Append(m_learnButton);

                m_learnStatusText = controls::TextBlock{};
                m_learnStatusText.FontSize(12);
                m_learnStatusText.MaxWidth(520);
                m_learnStatusText.TextWrapping(xaml::TextWrapping::Wrap);
                m_learnStatusText.VerticalAlignment(xaml::VerticalAlignment::Center);
                m_learnStatusText.Foreground(patchbay::ThemeBrushes::Current().Get(L"TextFillColorTertiaryBrush"));
                m_learnStatusText.Text(resources::GetString(isNotes ? L"FilterLearnNotesHint" : L"FilterLearnControllersHint"));

                learnRow.Children().Append(m_learnStatusText);

                body.Children().Append(learnRow);
            }

            if (isNotes)
            {
                // ---- the keyboard
                controls::Canvas keyboard{};

                auto const whiteCount = WhiteKeysBefore(patchbay::HighestNote) + 1;

                keyboard.Width(whiteCount * WhiteKeyWidth);
                keyboard.Height(WhiteKeyHeight);

                // Whites first so the blacks sit on top of them.
                for (int pass = 0; pass < 2; pass++)
                {
                    for (uint8_t note = patchbay::LowestNote; note <= patchbay::HighestNote; note++)
                    {
                        auto const black = IsBlackKey(note);

                        if ((pass == 0) == black)
                        {
                            continue;
                        }

                        shapes::Rectangle key{};

                        key.Width(black ? BlackKeyWidth : WhiteKeyWidth - 1);
                        key.Height(black ? BlackKeyHeight : WhiteKeyHeight);
                        key.RadiusX(1.5);
                        key.RadiusY(1.5);
                        key.StrokeThickness(0);

                        auto const left = black
                            ? WhiteKeysBefore(note) * WhiteKeyWidth - BlackKeyWidth / 2
                            : WhiteKeysBefore(note) * WhiteKeyWidth;

                        controls::Canvas::SetLeft(key, left);
                        controls::Canvas::SetTop(key, 0);

                        xaml::Automation::AutomationProperties::SetName(key, patchbay::DescribeNote(note));

                        key.PointerPressed([weak, note](auto&&, input::PointerRoutedEventArgs const& args)
                            {
                                args.Handled(true);

                                auto s = weak.get();

                                if (s == nullptr)
                                {
                                    return;
                                }

                                auto const shift = (args.KeyModifiers() &
                                    winrt::Windows::System::VirtualKeyModifiers::Shift) !=
                                    winrt::Windows::System::VirtualKeyModifiers::None;

                                s->OnValueKeyPressed(note, shift);
                            });

                        keyboard.Children().Append(key);
                        m_valueKeyFills.emplace_back(note, key);
                    }
                }

                controls::ScrollViewer keyboardScroller{};

                keyboardScroller.HorizontalScrollBarVisibility(controls::ScrollBarVisibility::Auto);
                keyboardScroller.VerticalScrollBarVisibility(controls::ScrollBarVisibility::Disabled);
                keyboardScroller.HorizontalScrollMode(controls::ScrollMode::Auto);
                keyboardScroller.Content(keyboard);

                body.Children().Append(keyboardScroller);
                body.Children().Append(HeadingHint(resources::GetString(L"FilterKeyboardHint")));
            }
            else
            {
                // ---- the controller numbers, sixteen to a row
                controls::VariableSizedWrapGrid grid{};

                grid.Orientation(controls::Orientation::Horizontal);
                grid.MaximumRowsOrColumns(16);
                grid.ItemWidth(44);
                grid.ItemHeight(32);

                for (uint8_t number = 0; number < static_cast<uint8_t>(patchbay::SevenBitValueCount); number++)
                {
                    primitives::ToggleButton toggle{};

                    toggle.Content(winrt::box_value(winrt::hstring{ std::to_wstring(number) }));
                    toggle.Width(40);
                    toggle.Height(28);
                    toggle.MinWidth(0);
                    toggle.Padding(xaml::ThicknessHelper::FromUniformLength(0));
                    toggle.FontSize(12);

                    xaml::Automation::AutomationProperties::SetName(toggle,
                        resources::FormatString(L"FilterControllerFormat", static_cast<int>(number)));

                    toggle.Click([weak, number](auto&&, auto&&)
                        {
                            if (auto s = weak.get())
                            {
                                s->OnValueKeyPressed(number, (::GetKeyState(VK_SHIFT) & 0x8000) != 0);
                            }
                        });

                    grid.Children().Append(toggle);
                    m_valueToggles.emplace_back(number, toggle);
                }

                body.Children().Append(grid);
                body.Children().Append(HeadingHint(resources::GetString(L"FilterControllerGridHint")));
            }

            BlockDialogContent().Children().Append(Card(body));

            RefreshValueSetUi();
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to build the value section.")
    }

    _Use_decl_annotations_
    void MainWindow::OnValueKeyPressed(uint8_t value, bool shift) noexcept
    {
        try
        {
            if (value >= patchbay::SevenBitValueCount)
            {
                return;
            }

            auto& values = m_editingSettings.Values;

            switch (values.Mode)
            {
            case patchbay::ValueSetMode::One:
                values.One = value;
                break;

            case patchbay::ValueSetMode::List:
                values.List[value] = !values.List[value];
                break;

            default:
                // A click sets the bottom of the range and Shift and a click the top, which is
                // quicker than aiming at two separate strips of keys.
                if (shift)
                {
                    values.Highest = value;
                    values.Lowest = (std::min)(values.Lowest, value);
                }
                else
                {
                    values.Lowest = value;
                    values.Highest = (std::max)(values.Highest, value);
                }
                break;
            }

            RefreshValueSetUi();
            UpdateBlockSummary();
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to pick a value.")
    }

    void MainWindow::RefreshValueSetUi() noexcept
    {
        try
        {
            m_updatingValueSet = true;
            auto const reset = wil::scope_exit([this]() { m_updatingValueSet = false; });

            auto const& values = m_editingSettings.Values;
            auto const isNotes = m_editingKind == patchbay::BlockKind::NoteFilter;

            if (m_valueModeButtons != nullptr)
            {
                m_valueModeButtons.SelectedIndex(static_cast<int32_t>(values.Mode));
            }

            if (m_valueRangePanel != nullptr)
            {
                m_valueRangePanel.Visibility(values.Mode == patchbay::ValueSetMode::Range
                    ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);
            }

            if (m_valueOnePanel != nullptr)
            {
                m_valueOnePanel.Visibility(values.Mode == patchbay::ValueSetMode::One
                    ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);
            }

            if (m_valueLowBox != nullptr)
            {
                m_valueLowBox.Value(values.Lowest);
            }

            if (m_valueHighBox != nullptr)
            {
                m_valueHighBox.Value(values.Highest);
            }

            if (m_valueOneBox != nullptr)
            {
                m_valueOneBox.Value(values.One);
            }

            if (m_valueSetText != nullptr)
            {
                auto const count = std::count(values.List.begin(), values.List.end(), true);

                winrt::hstring text{};

                switch (values.Mode)
                {
                case patchbay::ValueSetMode::One:
                    text = isNotes
                        ? resources::FormatString(L"FilterOneNoteFormat", patchbay::DescribeNote(values.One), static_cast<int>(values.One))
                        : resources::FormatString(L"FilterControllerFormat", static_cast<int>(values.One));
                    break;

                case patchbay::ValueSetMode::List:
                    text = count == 0
                        ? resources::GetString(isNotes ? L"FilterListEmptyNotes" : L"FilterListEmptyControllers")
                        : resources::FormatString(isNotes ? L"FilterListNotesFormat" : L"FilterListControllersFormat",
                            static_cast<int>(count));
                    break;

                default:
                    text = isNotes
                        ? resources::FormatString(L"FilterNoteRangeFormat",
                            patchbay::DescribeNote(values.Lowest), static_cast<int>(values.Lowest),
                            patchbay::DescribeNote(values.Highest), static_cast<int>(values.Highest))
                        : resources::FormatString(L"FilterControllerRangeFormat",
                            static_cast<int>(values.Lowest), static_cast<int>(values.Highest));
                    break;
                }

                m_valueSetText.Text(text);
            }

            // Picked keys in the accent color when they go through, and in the warning color
            // when they are kept out, so the keyboard reads the same way the filter works.
            auto const keepOut = values.Action == patchbay::FilterAction::KeepOut;
            auto const picked = patchbay::ThemeBrushes::Current().Get(keepOut
                ? L"SystemFillColorCriticalBrush"
                : L"AccentFillColorDefaultBrush");

            media::SolidColorBrush const naturalKey{ KeyColor(false) };
            media::SolidColorBrush const sharpKey{ KeyColor(true) };
            media::SolidColorBrush const sharpPicked{ DarkenedAccent(picked) };

            for (auto const& [note, key] : m_valueKeyFills)
            {
                auto const black = IsBlackKey(note);

                if (!values.Contains(note))
                {
                    key.Fill(black ? sharpKey : naturalKey);
                    continue;
                }

                key.Fill(black ? static_cast<media::Brush>(sharpPicked) : picked);
            }

            for (auto const& [number, toggle] : m_valueToggles)
            {
                toggle.IsChecked(values.Contains(number));
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to refresh the value picker.")
    }

    _Use_decl_annotations_
    std::vector<std::wstring> MainWindow::SourcesFeeding(std::wstring const& blockId) noexcept
    {
        std::vector<std::wstring> sources{};

        try
        {
            auto const* patch = CurrentPatch();

            if (patch == nullptr)
            {
                return sources;
            }

            // Back along the links, through any steps, to the endpoints messages come from.
            std::vector<std::wstring> pending{ blockId };
            std::unordered_set<std::wstring> seen{};

            while (!pending.empty())
            {
                auto const current = pending.back();
                pending.pop_back();

                if (!seen.insert(current).second)
                {
                    continue;
                }

                for (auto const& link : patch->Connections)
                {
                    if (link.DestinationId != current)
                    {
                        continue;
                    }

                    if (auto const* previous = patch->FindBlock(link.SourceId))
                    {
                        // A generator sends its own messages, not what comes into it.
                        if (!patchbay::IsGenerator(previous->Kind))
                        {
                            pending.push_back(link.SourceId);
                        }

                        continue;
                    }

                    if (auto const* endpoint = patch->FindEndpoint(link.SourceId))
                    {
                        if (auto const live = patchbay::ResolveEndpoint(*endpoint))
                        {
                            if (std::find(sources.begin(), sources.end(), live->EndpointDeviceId) == sources.end())
                            {
                                sources.push_back(live->EndpointDeviceId);
                            }
                        }
                    }
                }
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to find what feeds the step.")

        return sources;
    }

    winrt::fire_and_forget MainWindow::StartLearningAsync()
    {
        auto strong = get_strong();

        try
        {
            StopLearning();

            auto const sources = SourcesFeeding(m_editingBlockId);

            if (sources.empty())
            {
                if (m_learnStatusText != nullptr)
                {
                    m_learnStatusText.Text(resources::GetString(L"FilterLearnNoSource"));
                }

                co_return;
            }

            if (m_learnButton != nullptr)
            {
                m_learnButton.IsChecked(true);
            }

            if (m_learnStatusText != nullptr)
            {
                m_learnStatusText.Text(resources::GetString(m_editingKind == patchbay::BlockKind::NoteFilter
                    ? L"FilterLearningNotes"
                    : L"FilterLearningControllers"));
            }

            m_learnedLow = false;

            auto const generation = ++m_learnGeneration;
            auto const isNotes = m_editingKind == patchbay::BlockKind::NoteFilter;
            auto const queue = DispatcherQueue();
            auto weak = get_weak();

            midi2::MidiSession session{ nullptr };

            // Opening connections waits on the service, so it never happens on this thread. The
            // values arrive on the service's thread and are handed back here one at a time.
            co_await patchbay::RunOnBackgroundAsync([&session, sources, isNotes, queue, weak, generation]()
                {
                    try
                    {
                        session = midi2::MidiSession::Create(resources::GetString(L"LearnSessionName"));

                        if (session == nullptr)
                        {
                            return;
                        }

                        for (auto const& endpointDeviceId : sources)
                        {
                            auto connection = session.CreateEndpointConnection(winrt::hstring{ endpointDeviceId });

                            if (connection == nullptr)
                            {
                                continue;
                            }

                            connection.MessageReceived([queue, weak, isNotes, generation](
                                auto&&, midi2::MidiMessageReceivedEventArgs const& args)
                                {
                                    try
                                    {
                                        auto const word0 = args.PeekFirstWord();
                                        auto const type = (word0 >> 28) & 0x0F;

                                        if (type != 0x2 && type != 0x4)
                                        {
                                            return;
                                        }

                                        auto const status = (word0 >> 20) & 0x0F;
                                        auto const index = static_cast<uint8_t>((word0 >> 8) & 0x7F);

                                        if (isNotes)
                                        {
                                            // A MIDI 1.0 note on at velocity zero is a note off.
                                            if (status != 0x9 || (type == 0x2 && (word0 & 0x7F) == 0))
                                            {
                                                return;
                                            }
                                        }
                                        else if (status != 0xB)
                                        {
                                            return;
                                        }

                                        if (queue != nullptr)
                                        {
                                            queue.TryEnqueue([weak, index, generation]()
                                                {
                                                    auto s = weak.get();

                                                    if (s != nullptr && s->m_learnGeneration == generation)
                                                    {
                                                        s->OnLearnedValue(index);
                                                    }
                                                });
                                        }
                                    }
                                    catch (...)
                                    {
                                    }
                                });

                            connection.Open();
                        }
                    }
                    catch (...)
                    {
                    }
                });

            // Stopped, or the dialog closed, while the connections were opening.
            if (generation != m_learnGeneration || m_closing)
            {
                if (session != nullptr)
                {
                    patchbay::RunOnBackgroundAsync([session]()
                        {
                            try
                            {
                                session.Close();
                            }
                            catch (...)
                            {
                            }
                        });
                }

                co_return;
            }

            m_learnSession = session;
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to start learning.")
    }

    void MainWindow::StopLearning() noexcept
    {
        try
        {
            m_learnGeneration++;

            if (m_learnButton != nullptr)
            {
                m_learnButton.IsChecked(false);
            }

            auto session = m_learnSession;
            m_learnSession = nullptr;

            // Closing waits on the service, so it never happens on this thread.
            if (session != nullptr)
            {
                patchbay::RunOnBackgroundAsync([session]()
                    {
                        try
                        {
                            session.Close();
                        }
                        catch (...)
                        {
                        }
                    });
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to stop learning.")
    }

    _Use_decl_annotations_
    void MainWindow::OnLearnedValue(uint8_t value) noexcept
    {
        try
        {
            if (value >= patchbay::SevenBitValueCount)
            {
                return;
            }

            auto& values = m_editingSettings.Values;

            switch (values.Mode)
            {
            case patchbay::ValueSetMode::One:
                values.One = value;
                StopLearning();
                break;

            case patchbay::ValueSetMode::Range:
                // The first value played is the bottom, the second the top.
                if (!m_learnedLow)
                {
                    values.Lowest = value;
                    values.Highest = value;
                    m_learnedLow = true;
                }
                else
                {
                    values.Lowest = (std::min)(values.Lowest, value);
                    values.Highest = (std::max)(values.Highest, value);
                    m_learnedLow = false;
                    StopLearning();
                }
                break;

            default:
                values.List[value] = true;
                break;
            }

            RefreshValueSetUi();
            UpdateBlockSummary();
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to use a learned value.")
    }

    void MainWindow::BuildMaskSection() noexcept
    {
        try
        {
            auto weak = get_weak();
            auto const& mask = m_editingSettings.Mask;

            controls::StackPanel body{};
            body.Spacing(8);

            body.Children().Append(Heading(resources::GetString(L"MaskSectionHeading")));
            body.Children().Append(HeadingHint(resources::GetString(L"MaskSectionHint")));
            body.Children().Append(midiapp::MakeUmpPrimerLink(resources::GetString(L"UmpPrimerLink")));

            auto size = ChoiceButtons(
                resources::GetString(L"MaskSize"),
                {
                    resources::GetString(L"MaskSize1"),
                    resources::GetString(L"MaskSize2"),
                    resources::GetString(L"MaskSize3"),
                    resources::GetString(L"MaskSize4"),
                },
                std::clamp(static_cast<int32_t>(mask.WordCount) - 1, 0, 3));

            size.SelectionChanged([weak](foundation::IInspectable const& sender, auto&&)
                {
                    auto s = weak.get();
                    auto const list = sender.try_as<controls::RadioButtons>();

                    if (s == nullptr || list == nullptr || list.SelectedIndex() < 0)
                    {
                        return;
                    }

                    auto& target = s->m_editingSettings.Mask;
                    auto const words = static_cast<uint8_t>(std::clamp(list.SelectedIndex() + 1, 1,
                        static_cast<int32_t>(patchbay::MaximumUmpWords)));

                    if (target.WordCount == words)
                    {
                        return;
                    }

                    target.WordCount = words;

                    // A word the message doesn't have can't match anything.
                    for (auto& condition : target.Conditions)
                    {
                        condition.Word = (std::min)(condition.Word, static_cast<uint8_t>(words - 1));
                    }

                    s->RebuildMaskConditions();
                    s->UpdateBlockSummary();
                });

            body.Children().Append(size);
            body.Children().Append(HeadingHint(resources::GetString(L"MaskSizeHint")));

            auto action = ChoiceButtons(
                resources::GetString(L"MaskActionHeader"),
                { resources::GetString(L"FilterActionLetThrough"), resources::GetString(L"FilterActionKeepOut") },
                mask.Action == patchbay::FilterAction::KeepOut ? 1 : 0);

            action.SelectionChanged([weak](foundation::IInspectable const& sender, auto&&)
                {
                    auto s = weak.get();
                    auto const list = sender.try_as<controls::RadioButtons>();

                    if (s == nullptr || list == nullptr || list.SelectedIndex() < 0)
                    {
                        return;
                    }

                    s->m_editingSettings.Mask.Action = list.SelectedIndex() == 1
                        ? patchbay::FilterAction::KeepOut
                        : patchbay::FilterAction::LetThrough;

                    s->UpdateBlockSummary();
                });

            body.Children().Append(action);

            controls::CheckBox hex{};

            hex.Content(winrt::box_value(resources::GetString(L"MaskShowHex")));
            hex.IsChecked(mask.ShowHex);

            auto const setHex = [weak](bool value)
                {
                    if (auto s = weak.get())
                    {
                        s->m_editingSettings.Mask.ShowHex = value;
                        s->RebuildMaskConditions();
                        s->UpdateBlockSummary();
                    }
                };

            hex.Checked([setHex](auto&&, auto&&) { setHex(true); });
            hex.Unchecked([setHex](auto&&, auto&&) { setHex(false); });

            body.Children().Append(hex);

            m_maskConditionsPanel = controls::StackPanel{};
            m_maskConditionsPanel.Spacing(10);
            m_maskConditionsPanel.Margin(xaml::ThicknessHelper::FromLengths(0, 6, 0, 6));

            body.Children().Append(m_maskConditionsPanel);

            controls::Button add{};

            add.Content(winrt::box_value(resources::GetString(L"MaskAddCondition")));

            add.Click([weak](auto&&, auto&&)
                {
                    auto s = weak.get();

                    if (s == nullptr)
                    {
                        return;
                    }

                    auto& target = s->m_editingSettings.Mask;

                    if (target.Conditions.size() >= patchbay::MaximumMaskConditions)
                    {
                        return;
                    }

                    // The message type is the field people start from.
                    patchbay::MaskCondition condition{};

                    condition.Word = MaskFields[0].Word;
                    condition.HighBit = MaskFields[0].HighBit;
                    condition.LowBit = MaskFields[0].LowBit;

                    target.Conditions.push_back(condition);

                    s->RebuildMaskConditions();
                    s->UpdateBlockSummary();
                });

            body.Children().Append(add);

            BlockDialogContent().Children().Append(Card(body));

            RebuildMaskConditions();
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to build the mask section.")
    }

    void MainWindow::RebuildMaskConditions() noexcept
    {
        try
        {
            if (m_maskConditionsPanel == nullptr)
            {
                return;
            }

            m_maskConditionsPanel.Children().Clear();

            auto& mask = m_editingSettings.Mask;
            auto const hex = mask.ShowHex;
            auto weak = get_weak();

            if (mask.Conditions.empty())
            {
                auto empty = HeadingHint(resources::GetString(L"MaskNoConditions"));
                empty.Margin(xaml::ThicknessHelper::FromUniformLength(0));
                m_maskConditionsPanel.Children().Append(empty);
                return;
            }

            // In words, under each condition, so the bit numbers can be checked against what
            // they mean.
            auto const describe = [hex](patchbay::MaskCondition const& condition) -> winrt::hstring
                {
                    if (condition.BitCount() == 0)
                    {
                        return resources::GetString(L"MaskBitsInvalid");
                    }

                    return resources::FormatString(L"MaskConditionFormat",
                        static_cast<int>(condition.Word) + 1,
                        static_cast<int>(condition.HighBit),
                        static_cast<int>(condition.LowBit),
                        static_cast<int>(condition.BitCount()),
                        FormatMaskNumber(condition.FieldMaximum(), hex));
                };

            auto const parseOne = [hex](patchbay::MaskCondition const& target, std::wstring const& text) -> std::optional<uint32_t>
                {
                    auto const value = ParseMaskNumber(text, hex);

                    if (!value.has_value() || value.value() > target.FieldMaximum())
                    {
                        return std::nullopt;
                    }

                    return value;
                };

            for (size_t position = 0; position < mask.Conditions.size(); position++)
            {
                auto const& condition = mask.Conditions[position];

                if (position > 0)
                {
                    controls::TextBlock joiner{};

                    joiner.Text(resources::GetString(L"MaskAnd"));
                    joiner.FontSize(12);
                    joiner.FontWeight(winrt::Microsoft::UI::Text::FontWeights::SemiBold());

                    m_maskConditionsPanel.Children().Append(joiner);
                }

                controls::StackPanel conditionPanel{};
                conditionPanel.Spacing(6);

                controls::TextBlock description{};
                description.FontSize(12);
                description.TextWrapping(xaml::TextWrapping::Wrap);
                description.Foreground(patchbay::ThemeBrushes::Current().Get(L"TextFillColorSecondaryBrush"));
                description.Text(describe(condition));

                // After any change to this condition: the words underneath, and the summary.
                auto const changed = [weak, position, description, describe]()
                    {
                        auto s = weak.get();

                        if (s == nullptr || position >= s->m_editingSettings.Mask.Conditions.size())
                        {
                            return;
                        }

                        description.Text(describe(s->m_editingSettings.Mask.Conditions[position]));
                        s->UpdateBlockSummary();
                    };

                // ---- where in the message
                controls::StackPanel where{};
                where.Orientation(controls::Orientation::Horizontal);
                where.Spacing(8);

                controls::ComboBox word{};
                word.Header(winrt::box_value(resources::GetString(L"MaskWord")));
                word.MinWidth(96);

                for (uint8_t i = 0; i < mask.WordCount; i++)
                {
                    word.Items().Append(winrt::box_value(resources::FormatString(L"MaskWordFormat", static_cast<int>(i) + 1)));
                }

                word.SelectedIndex((std::min)(static_cast<int32_t>(condition.Word), static_cast<int32_t>(mask.WordCount) - 1));

                word.SelectionChanged([weak, position, changed](foundation::IInspectable const& sender, auto&&)
                    {
                        auto s = weak.get();
                        auto const combo = sender.try_as<controls::ComboBox>();

                        if (s == nullptr || combo == nullptr || combo.SelectedIndex() < 0 ||
                            position >= s->m_editingSettings.Mask.Conditions.size())
                        {
                            return;
                        }

                        s->m_editingSettings.Mask.Conditions[position].Word = static_cast<uint8_t>(combo.SelectedIndex());
                        changed();
                    });

                where.Children().Append(word);

                auto const highBit = NumberField(resources::GetString(L"MaskHighBit"), 0, 31, condition.HighBit);
                auto const lowBit = NumberField(resources::GetString(L"MaskLowBit"), 0, 31, condition.LowBit);

                highBit.Width(130);
                lowBit.Width(130);

                // The other end follows, so the range never runs backwards. Each box holds the
                // other weakly, or the two would keep each other alive after the dialog closes.
                highBit.ValueChanged([weak, position, changed, other = winrt::make_weak(lowBit)](auto&&, controls::NumberBoxValueChangedEventArgs const& args)
                    {
                        auto s = weak.get();

                        if (s == nullptr || s->m_updatingValueSet || std::isnan(args.NewValue()) ||
                            position >= s->m_editingSettings.Mask.Conditions.size())
                        {
                            return;
                        }

                        auto& target = s->m_editingSettings.Mask.Conditions[position];

                        target.HighBit = static_cast<uint8_t>(std::clamp(args.NewValue(), 0.0, 31.0));

                        if (target.LowBit > target.HighBit)
                        {
                            target.LowBit = target.HighBit;

                            if (auto const box = other.get())
                            {
                                s->m_updatingValueSet = true;
                                box.Value(target.LowBit);
                                s->m_updatingValueSet = false;
                            }
                        }

                        changed();
                    });

                lowBit.ValueChanged([weak, position, changed, other = winrt::make_weak(highBit)](auto&&, controls::NumberBoxValueChangedEventArgs const& args)
                    {
                        auto s = weak.get();

                        if (s == nullptr || s->m_updatingValueSet || std::isnan(args.NewValue()) ||
                            position >= s->m_editingSettings.Mask.Conditions.size())
                        {
                            return;
                        }

                        auto& target = s->m_editingSettings.Mask.Conditions[position];

                        target.LowBit = static_cast<uint8_t>(std::clamp(args.NewValue(), 0.0, 31.0));

                        if (target.HighBit < target.LowBit)
                        {
                            target.HighBit = target.LowBit;

                            if (auto const box = other.get())
                            {
                                s->m_updatingValueSet = true;
                                box.Value(target.HighBit);
                                s->m_updatingValueSet = false;
                            }
                        }

                        changed();
                    });

                where.Children().Append(highBit);
                where.Children().Append(lowBit);

                controls::DropDownButton fields{};
                fields.Content(winrt::box_value(resources::GetString(L"MaskPickField")));
                fields.VerticalAlignment(xaml::VerticalAlignment::Bottom);

                controls::MenuFlyout fieldMenu{};

                for (auto const& field : MaskFields)
                {
                    controls::MenuFlyoutItem item{};

                    item.Text(resources::GetString(field.NameKey));

                    item.Click([weak, position, field](auto&&, auto&&)
                        {
                            auto s = weak.get();

                            if (s == nullptr || position >= s->m_editingSettings.Mask.Conditions.size())
                            {
                                return;
                            }

                            auto& target = s->m_editingSettings.Mask;

                            // A field in the second word needs a message that has one.
                            if (field.Word >= target.WordCount)
                            {
                                target.WordCount = static_cast<uint8_t>(field.Word + 1);
                            }

                            auto& picked = target.Conditions[position];

                            picked.Word = field.Word;
                            picked.HighBit = field.HighBit;
                            picked.LowBit = field.LowBit;

                            // The size can have changed too, so the whole section is drawn again.
                            s->BuildBlockDialog();
                        });

                    fieldMenu.Items().Append(item);
                }

                fields.Flyout(fieldMenu);
                where.Children().Append(fields);

                controls::Button remove{};
                remove.Content(winrt::box_value(resources::GetString(L"TransformRemove")));
                remove.VerticalAlignment(xaml::VerticalAlignment::Bottom);

                remove.Click([weak, position](auto&&, auto&&)
                    {
                        auto s = weak.get();

                        if (s == nullptr || position >= s->m_editingSettings.Mask.Conditions.size())
                        {
                            return;
                        }

                        auto& conditions = s->m_editingSettings.Mask.Conditions;
                        conditions.erase(conditions.begin() + static_cast<ptrdiff_t>(position));

                        s->RebuildMaskConditions();
                        s->UpdateBlockSummary();
                    });

                where.Children().Append(remove);

                conditionPanel.Children().Append(where);

                // ---- what counts as a match
                controls::StackPanel match{};
                match.Orientation(controls::Orientation::Horizontal);
                match.Spacing(8);

                controls::ComboBox matchKind{};
                matchKind.Header(winrt::box_value(resources::GetString(L"MaskMatch")));
                matchKind.MinWidth(150);
                matchKind.Items().Append(winrt::box_value(resources::GetString(L"MaskMatchExactly")));
                matchKind.Items().Append(winrt::box_value(resources::GetString(L"MaskMatchAnyOf")));
                matchKind.Items().Append(winrt::box_value(resources::GetString(L"MaskMatchBetween")));
                matchKind.SelectedIndex(static_cast<int32_t>(condition.Match));

                matchKind.SelectionChanged([weak, position](foundation::IInspectable const& sender, auto&&)
                    {
                        auto s = weak.get();
                        auto const combo = sender.try_as<controls::ComboBox>();

                        if (s == nullptr || combo == nullptr || combo.SelectedIndex() < 0 ||
                            position >= s->m_editingSettings.Mask.Conditions.size())
                        {
                            return;
                        }

                        auto const kind = static_cast<patchbay::MaskMatch>(std::clamp(combo.SelectedIndex(), 0, 2));
                        auto& target = s->m_editingSettings.Mask.Conditions[position];

                        if (target.Match == kind)
                        {
                            return;
                        }

                        target.Match = kind;

                        s->RebuildMaskConditions();
                        s->UpdateBlockSummary();
                    });

                match.Children().Append(matchKind);

                // A value that doesn't fit the field is shown as a problem and not kept.
                auto const makeValueBox = [weak, position, changed, description, hex](
                    winrt::hstring const& header,
                    std::wstring const& text,
                    double width,
                    std::function<bool(patchbay::MaskCondition&, std::wstring const&)> apply)
                    {
                        controls::TextBox box{};

                        box.Header(winrt::box_value(header));
                        box.Text(winrt::hstring{ text });
                        box.Width(width);

                        box.TextChanged([weak, position, changed, description, apply, hex](foundation::IInspectable const& sender, auto&&)
                            {
                                auto s = weak.get();
                                auto const source = sender.try_as<controls::TextBox>();

                                if (s == nullptr || source == nullptr || position >= s->m_editingSettings.Mask.Conditions.size())
                                {
                                    return;
                                }

                                auto& target = s->m_editingSettings.Mask.Conditions[position];

                                if (apply(target, std::wstring{ source.Text() }))
                                {
                                    changed();
                                    return;
                                }

                                description.Text(resources::FormatString(L"MaskValueInvalidFormat",
                                    FormatMaskNumber(target.FieldMaximum(), hex)));
                            });

                        return box;
                    };

                auto const setOne = [parseOne](uint32_t patchbay::MaskCondition::* member)
                    {
                        return [parseOne, member](patchbay::MaskCondition& target, std::wstring const& text)
                            {
                                auto const value = parseOne(target, text);

                                if (value.has_value())
                                {
                                    target.*member = value.value();
                                }

                                return value.has_value();
                            };
                    };

                switch (condition.Match)
                {
                case patchbay::MaskMatch::AnyOf:
                {
                    std::wstring list{};

                    for (auto const value : condition.Values)
                    {
                        if (!list.empty())
                        {
                            list += L", ";
                        }

                        list += FormatMaskNumber(value, hex);
                    }

                    auto box = makeValueBox(resources::GetString(L"MaskValues"), list, 320,
                        [hex](patchbay::MaskCondition& target, std::wstring const& text)
                        {
                            std::vector<uint32_t> parsed{};
                            std::wstring current{};

                            auto const take = [&]() -> bool
                                {
                                    if (current.empty())
                                    {
                                        return true;
                                    }

                                    auto const value = ParseMaskNumber(current, hex);
                                    current.clear();

                                    if (!value.has_value() || value.value() > target.FieldMaximum() ||
                                        parsed.size() >= patchbay::MaximumMaskValues)
                                    {
                                        return false;
                                    }

                                    if (std::find(parsed.begin(), parsed.end(), value.value()) == parsed.end())
                                    {
                                        parsed.push_back(value.value());
                                    }

                                    return true;
                                };

                            for (auto const c : text)
                            {
                                if (c == L',' || c == L';' || ::iswspace(c))
                                {
                                    if (!take())
                                    {
                                        return false;
                                    }

                                    continue;
                                }

                                current += c;
                            }

                            if (!take() || parsed.empty())
                            {
                                return false;
                            }

                            target.Values = std::move(parsed);
                            return true;
                        });

                    box.PlaceholderText(resources::GetString(L"MaskValuesPlaceholder"));
                    match.Children().Append(box);
                    break;
                }

                case patchbay::MaskMatch::Between:
                    match.Children().Append(makeValueBox(resources::GetString(L"MaskLowest"),
                        FormatMaskNumber(condition.Lowest, hex), 150, setOne(&patchbay::MaskCondition::Lowest)));

                    match.Children().Append(makeValueBox(resources::GetString(L"MaskHighest"),
                        FormatMaskNumber(condition.Highest, hex), 150, setOne(&patchbay::MaskCondition::Highest)));
                    break;

                default:
                    match.Children().Append(makeValueBox(resources::GetString(L"MaskValue"),
                        FormatMaskNumber(condition.Value, hex), 150, setOne(&patchbay::MaskCondition::Value)));
                    break;
                }

                conditionPanel.Children().Append(match);
                conditionPanel.Children().Append(description);

                m_maskConditionsPanel.Children().Append(conditionPanel);
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to list the mask conditions.")
    }
}
