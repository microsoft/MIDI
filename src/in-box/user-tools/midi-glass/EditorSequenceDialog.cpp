// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// The sequence editor, screen 7 of the design.
//
// A button that does more than one thing. Steps run in order, a wait is a first-class step, and
// a block can repeat. This is deliberately a list rather than a scripting language: a layout is
// untrusted input from a stranger, and putting a script engine behind a downloaded file would
// turn a sequencer into a way to deliver code.

#include "pch.h"
#include "EditorWindow.xaml.h"

#include "StringResources.h"
#include "HexText.h"

#include <shobjidl.h>

namespace resources = ::midiglass::resources;

namespace winrt::midiglass::implementation
{
    namespace
    {
        media::Brush SequenceBrushNamed(_In_ wchar_t const* key)
        {
            return xaml::Application::Current().Resources().Lookup(box_value(key)).as<media::Brush>();
        }

        // The step kinds the design's "add a step" row offers, in its order.
        struct StepChoice
        {
            glass::SequenceStepKind Kind{ glass::SequenceStepKind::SendMidiMessage };
            glass::MessageKind Message{ glass::MessageKind::Note };
            wchar_t const* NameKey{ nullptr };
            wchar_t Glyph{ 0 };
        };

        constexpr StepChoice StepChoices[]
        {
            { glass::SequenceStepKind::SendMidiMessage, glass::MessageKind::Note,
                L"StepAddNote", L'\uEC4F' },
            { glass::SequenceStepKind::SendMidiMessage, glass::MessageKind::ControlChange,
                L"StepAddMessage", L'\uE9E9' },
            { glass::SequenceStepKind::SendSystemExclusive, glass::MessageKind::SystemExclusive,
                L"StepAddSystemExclusive", L'\uE8A5' },
            { glass::SequenceStepKind::Wait, glass::MessageKind::ControlChange,
                L"StepAddWait", L'\uE916' },
            { glass::SequenceStepKind::RepeatBlockStart, glass::MessageKind::ControlChange,
                L"StepAddRepeat", L'\uE72C' },
            { glass::SequenceStepKind::SetControlValue, glass::MessageKind::ControlChange,
                L"StepAddSetControl", L'\uE8AB' },
            { glass::SequenceStepKind::GoToPage, glass::MessageKind::GoToPage,
                L"StepAddGoToPage", L'\uE80A' },
            { glass::SequenceStepKind::HoldLayer, glass::MessageKind::ControlChange,
                L"StepAddHoldLayer", L'\uE72E' },
        };

        wchar_t const* GlyphKeyForStep(_In_ glass::SequenceStep const& step) noexcept
        {
            switch (step.Kind)
            {
            case glass::SequenceStepKind::Wait:             return L"StepAddWait";
            case glass::SequenceStepKind::SendSystemExclusive: return L"StepAddSystemExclusive";
            case glass::SequenceStepKind::SetControlValue:  return L"StepAddSetControl";
            case glass::SequenceStepKind::GoToPage:         return L"StepAddGoToPage";
            case glass::SequenceStepKind::HoldLayer:        return L"StepAddHoldLayer";
            case glass::SequenceStepKind::RepeatBlockStart: return L"StepAddRepeat";
            case glass::SequenceStepKind::RepeatBlockEnd:   return L"StepAddRepeat";
            default:
                return step.Message.Kind == glass::MessageKind::Note
                    ? L"StepAddNote"
                    : L"StepAddMessage";
            }
        }

        // What one row reads, in plain words. A step list somebody cannot read at a glance is a
        // step list they will not trust with a synthesizer.
        std::wstring DescribeStep(
            _In_ glass::SequenceStep const& step,
            _In_ glass::LayoutDocument const& document)
        {
            switch (step.Kind)
            {
            case glass::SequenceStepKind::Wait:
                return std::wstring{ resources::FormatString(
                    L"StepWaitFormat", static_cast<int32_t>(step.WaitMilliseconds)) };

            case glass::SequenceStepKind::RepeatBlockStart:
                return std::wstring{ resources::FormatString(
                    L"StepRepeatFormat", static_cast<int32_t>(step.RepeatCount)) };

            case glass::SequenceStepKind::RepeatBlockEnd:
                return std::wstring{ resources::GetString(L"StepRepeatEnd") };

            case glass::SequenceStepKind::HoldLayer:
                return std::wstring{ resources::GetString(L"StepHoldLayerText") };

            case glass::SequenceStepKind::SetControlValue:
            {
                auto const* const control = document.FindControl(step.TargetControlId);

                return std::wstring{ resources::FormatString(
                    L"StepSetControlFormat",
                    control == nullptr
                        ? std::wstring{ resources::GetString(L"StepNoControl") }
                        : (control->Label.empty() ? control->Id : control->Label),
                    std::to_wstring(static_cast<int32_t>(step.TargetValue * 100.0 + 0.5))) };
            }

            case glass::SequenceStepKind::GoToPage:
            {
                auto const* const page = document.FindPage(step.Message.TargetPageId);

                return std::wstring{ resources::FormatString(
                    L"StepGoToPageFormat",
                    page == nullptr
                        ? std::wstring{ resources::GetString(L"StepNoPage") }
                        : page->Name) };
            }

            case glass::SequenceStepKind::SendSystemExclusive:
                return std::wstring{ resources::FormatString(
                    L"StepSystemExclusiveFormat",
                    static_cast<int32_t>(step.Message.SystemExclusive.size()),
                    step.Message.DeviceName.empty()
                        ? std::wstring{ resources::GetString(L"MessageNoDevice") }
                        : step.Message.DeviceName) };

            default:
            {
                auto const number = std::to_wstring(step.Message.Number);

                auto const what = step.Message.Kind == glass::MessageKind::Note
                    ? resources::FormatString(L"StepNoteFormat", number, std::to_wstring(step.DurationMilliseconds))
                    : resources::FormatString(L"StepMessageFormat", number);

                return std::wstring{ resources::FormatString(
                    L"StepDestinationFormat",
                    what,
                    step.Message.DeviceName.empty()
                        ? std::wstring{ resources::GetString(L"MessageNoDevice") }
                        : step.Message.DeviceName,
                    std::to_wstring(step.Message.ChannelIndex + 1)) };
            }
            }
        }

        // How long the whole thing takes, counting only the waits. It is an honest floor rather
        // than a promise: what a device does with a dump is its own business.
        uint64_t TotalWaitMilliseconds(_In_ std::vector<glass::SequenceStep> const& steps)
        {
            uint64_t total{ 0 };

            uint32_t repeat{ 1 };
            uint64_t inBlock{ 0 };
            bool insideBlock{ false };

            for (auto const& step : steps)
            {
                switch (step.Kind)
                {
                case glass::SequenceStepKind::RepeatBlockStart:
                    insideBlock = true;
                    repeat = std::clamp<uint32_t>(step.RepeatCount, 1, 256);
                    inBlock = 0;
                    break;

                case glass::SequenceStepKind::RepeatBlockEnd:
                    total += inBlock * repeat;
                    insideBlock = false;
                    inBlock = 0;
                    break;

                case glass::SequenceStepKind::Wait:
                    (insideBlock ? inBlock : total) += step.WaitMilliseconds;
                    break;

                default:
                    break;
                }
            }

            // An unclosed block still counts once, so the number does not jump when the end is
            // added.
            total += inBlock;

            return total;
        }
    }

    _Use_decl_annotations_
    winrt::fire_and_forget EditorWindow::ShowSequenceDialog(std::wstring sequenceName)
    {
        auto lifetime = get_strong();

        try
        {
            auto const* const existing = m_editor.Document().FindSequence(sequenceName);

            if (existing == nullptr)
            {
                co_return;
            }

            // Everything is edited on a copy. A canceled dialog has to leave nothing behind,
            // and half a sequence written into the document is exactly what that would be.
            auto working = std::make_shared<glass::Sequence>(*existing);

            auto const document = m_editor.Document();

            controls::ContentDialog dialog{};

            dialog.XamlRoot(RootGrid().XamlRoot());
            dialog.Title(box_value(resources::FormatString(L"SequenceDialogTitleFormat", sequenceName)));
            dialog.PrimaryButtonText(resources::GetString(L"DialogDone"));
            dialog.CloseButtonText(resources::GetString(L"DialogCancel"));
            dialog.DefaultButton(controls::ContentDialogButton::Primary);

            controls::StackPanel root{};
            root.Spacing(10);
            root.MinWidth(620);

            // ---- add a step ----

            controls::TextBlock addHeading{};
            addHeading.Text(resources::GetString(L"SequenceAddHeading"));
            addHeading.FontSize(11);
            addHeading.VerticalAlignment(xaml::VerticalAlignment::Center);
            addHeading.Foreground(SequenceBrushNamed(L"TextFillColorTertiaryBrush"));

            // The design wraps these onto a second line rather than scrolling them sideways.
            // A chip nobody can see is a step kind nobody knows exists.
            controls::StackPanel addChips{};
            addChips.Orientation(controls::Orientation::Horizontal);
            addChips.Spacing(6);

            controls::ScrollViewer addScroll{};
            addScroll.HorizontalScrollBarVisibility(controls::ScrollBarVisibility::Auto);
            addScroll.VerticalScrollMode(controls::ScrollMode::Disabled);
            addScroll.VerticalScrollBarVisibility(controls::ScrollBarVisibility::Disabled);
            addScroll.Content(addChips);

            // ---- the steps ----

            controls::ListView list{};
            list.SelectionMode(controls::ListViewSelectionMode::Single);
            list.Height(230);

            controls::TextBlock summary{};
            summary.FontSize(11);
            summary.Foreground(SequenceBrushNamed(L"TextFillColorTertiaryBrush"));

            // ---- what pressing the button does ----

            // Three named choices rather than a drop-down, because the difference between them
            // is the difference between a one-shot and something that keeps going, and that is
            // worth reading without opening anything.
            controls::RadioButtons modeChoices{};
            modeChoices.Header(box_value(resources::GetString(L"SequenceModeHeader")));
            modeChoices.MaxColumns(3);
            modeChoices.Items().Append(box_value(resources::GetString(L"SequenceModeOnce")));
            modeChoices.Items().Append(box_value(resources::GetString(L"SequenceModeWhileHeld")));
            modeChoices.Items().Append(box_value(resources::GetString(L"SequenceModeToggle")));
            modeChoices.SelectedIndex(static_cast<int32_t>(working->Mode));

            // A wait is a scheduled state machine on the MIDI clock, not a sleeping thread, so
            // twenty buttons can be mid-sequence while a fader still moves at full rate. Saying
            // so here is what stops somebody avoiding waits out of caution.
            controls::Border waitNote{};
            waitNote.CornerRadius(xaml::CornerRadiusHelper::FromUniformRadius(9.0));
            waitNote.Padding(xaml::ThicknessHelper::FromLengths(9.0, 2.0, 10.0, 3.0));
            waitNote.VerticalAlignment(xaml::VerticalAlignment::Bottom);
            waitNote.Background(SequenceBrushNamed(L"SystemFillColorCautionBackgroundBrush"));

            {
                controls::StackPanel noteRow{};
                noteRow.Orientation(controls::Orientation::Horizontal);
                noteRow.Spacing(6.0);

                controls::FontIcon noteGlyph{};
                noteGlyph.Glyph(L"\uE7BA");
                noteGlyph.FontSize(11.0);
                noteGlyph.Foreground(SequenceBrushNamed(L"SystemFillColorCautionBrush"));

                controls::TextBlock noteText{};
                noteText.Text(resources::GetString(L"SequenceWaitsDoNotBlock"));
                noteText.FontSize(11.0);
                noteText.Foreground(SequenceBrushNamed(L"SystemFillColorCautionBrush"));

                noteRow.Children().Append(noteGlyph);
                noteRow.Children().Append(noteText);
                waitNote.Child(noteRow);
            }

            controls::Grid modeRow{};
            modeRow.ColumnSpacing(10.0);

            {
                controls::ColumnDefinition stretch{};
                stretch.Width(xaml::GridLengthHelper::FromValueAndType(1.0, xaml::GridUnitType::Star));

                controls::ColumnDefinition fit{};
                fit.Width(xaml::GridLengthHelper::FromValueAndType(0.0, xaml::GridUnitType::Auto));

                modeRow.ColumnDefinitions().Append(stretch);
                modeRow.ColumnDefinitions().Append(fit);

                controls::Grid::SetColumn(modeChoices, 0);
                controls::Grid::SetColumn(waitNote, 1);

                modeRow.Children().Append(modeChoices);
                modeRow.Children().Append(waitNote);
            }

            // ---- the fields for the selected step ----

            controls::StackPanel fields{};
            fields.Spacing(6);

            controls::NumberBox numberField{};
            numberField.Header(box_value(resources::GetString(L"StepNumberHeader")));
            numberField.Minimum(0);
            numberField.Maximum(127);
            numberField.SpinButtonPlacementMode(controls::NumberBoxSpinButtonPlacementMode::Compact);

            controls::NumberBox waitField{};
            waitField.Header(box_value(resources::GetString(L"StepWaitHeader")));
            waitField.Minimum(0);
            waitField.Maximum(60000);
            waitField.SpinButtonPlacementMode(controls::NumberBoxSpinButtonPlacementMode::Compact);

            controls::NumberBox durationField{};
            durationField.Header(box_value(resources::GetString(L"StepDurationHeader")));
            durationField.Minimum(0);
            durationField.Maximum(60000);
            durationField.SpinButtonPlacementMode(controls::NumberBoxSpinButtonPlacementMode::Compact);

            controls::NumberBox repeatField{};
            repeatField.Header(box_value(resources::GetString(L"StepRepeatHeader")));
            repeatField.Minimum(1);
            repeatField.Maximum(256);
            repeatField.SpinButtonPlacementMode(controls::NumberBoxSpinButtonPlacementMode::Compact);

            controls::ComboBox deviceField{};
            deviceField.Header(box_value(resources::GetString(L"StepDeviceHeader")));
            deviceField.HorizontalAlignment(xaml::HorizontalAlignment::Stretch);

            for (auto const& device : document.Devices)
            {
                deviceField.Items().Append(box_value(winrt::hstring{ device.Name }));
            }

            controls::ComboBox channelField{};
            channelField.Header(box_value(resources::GetString(L"StepChannelHeader")));
            channelField.HorizontalAlignment(xaml::HorizontalAlignment::Stretch);

            for (int32_t channel = 1; channel <= 16; ++channel)
            {
                channelField.Items().Append(box_value(winrt::to_hstring(channel)));
            }

            controls::ComboBox targetControlField{};
            targetControlField.Header(box_value(resources::GetString(L"StepControlHeader")));
            targetControlField.HorizontalAlignment(xaml::HorizontalAlignment::Stretch);

            std::vector<std::wstring> controlIds{};

            for (auto const& page : document.Pages)
            {
                for (auto const& control : page.Controls)
                {
                    controlIds.push_back(control.Id);

                    targetControlField.Items().Append(box_value(winrt::hstring{
                        control.Label.empty() ? control.Id : control.Label }));
                }
            }

            controls::Slider targetValueField{};
            targetValueField.Header(box_value(resources::GetString(L"StepValueHeader")));
            targetValueField.Minimum(0);
            targetValueField.Maximum(100);
            targetValueField.StepFrequency(1);

            controls::ComboBox targetPageField{};
            targetPageField.Header(box_value(resources::GetString(L"StepPageHeader")));
            targetPageField.HorizontalAlignment(xaml::HorizontalAlignment::Stretch);

            for (auto const& page : document.Pages)
            {
                targetPageField.Items().Append(box_value(winrt::hstring{ page.Name }));
            }

            controls::TextBox bytesField{};
            bytesField.Header(box_value(resources::GetString(L"StepBytesHeader")));
            bytesField.AcceptsReturn(true);
            bytesField.TextWrapping(xaml::TextWrapping::Wrap);
            bytesField.MinHeight(64);
            bytesField.MaxHeight(110);
            bytesField.FontFamily(media::FontFamily{ L"Cascadia Mono, Consolas" });
            bytesField.FontSize(11);

            controls::Button fromFileButton{};
            fromFileButton.Content(box_value(resources::GetString(L"SysExFromFileCommand")));
            fromFileButton.FontSize(11);

            fields.Children().Append(numberField);
            fields.Children().Append(waitField);
            fields.Children().Append(durationField);
            fields.Children().Append(repeatField);
            fields.Children().Append(deviceField);
            fields.Children().Append(channelField);
            fields.Children().Append(targetControlField);
            fields.Children().Append(targetValueField);
            fields.Children().Append(targetPageField);
            fields.Children().Append(bytesField);
            fields.Children().Append(fromFileButton);

            // ---- move and delete ----

            controls::StackPanel tools{};
            tools.Orientation(controls::Orientation::Horizontal);
            tools.Spacing(6);

            auto const iconButton = [](wchar_t glyph, winrt::hstring const& name)
                {
                    controls::Button button{};

                    controls::FontIcon icon{};
                    icon.Glyph(winrt::hstring{ std::wstring(1, glyph) });
                    icon.FontSize(12);

                    button.Content(icon);
                    button.Width(36);
                    button.Padding({ 0, 0, 0, 0 });

                    xaml::Automation::AutomationProperties::SetName(button, name);
                    controls::ToolTipService::SetToolTip(button, box_value(name));

                    return button;
                };

            auto upButton = iconButton(L'\uE74A', resources::GetString(L"StepMoveUp"));
            auto downButton = iconButton(L'\uE74B', resources::GetString(L"StepMoveDown"));
            auto deleteButton = iconButton(L'\uE74D', resources::GetString(L"StepDelete"));

            controls::Button duplicateButton{};
            {
                controls::StackPanel duplicateRow{};
                duplicateRow.Orientation(controls::Orientation::Horizontal);
                duplicateRow.Spacing(7.0);

                controls::FontIcon duplicateGlyph{};
                duplicateGlyph.Glyph(L"\uE8C8");
                duplicateGlyph.FontSize(12.0);

                controls::TextBlock duplicateText{};
                duplicateText.Text(resources::GetString(L"StepDuplicate"));
                duplicateText.FontSize(12.0);

                duplicateRow.Children().Append(duplicateGlyph);
                duplicateRow.Children().Append(duplicateText);

                duplicateButton.Content(duplicateRow);

                xaml::Automation::AutomationProperties::SetName(
                    duplicateButton, resources::GetString(L"StepDuplicate"));
            }

            // The comp puts these above the list, because they act on the row that is selected
            // in it and a toolbar below the thing it edits reads as belonging to what follows.
            tools.Children().Append(duplicateButton);
            tools.Children().Append(upButton);
            tools.Children().Append(downButton);
            tools.Children().Append(deleteButton);

            controls::Grid toolbar{};
            toolbar.ColumnSpacing(10.0);

            controls::Button testButton{};
            {
                controls::StackPanel testRow{};
                testRow.Orientation(controls::Orientation::Horizontal);
                testRow.Spacing(7.0);

                controls::FontIcon testGlyph{};
                testGlyph.Glyph(L"\uE768");
                testGlyph.FontSize(12.0);

                controls::TextBlock testText{};
                testText.Text(resources::GetString(L"SequenceTestIt"));
                testText.FontSize(12.0);

                testRow.Children().Append(testGlyph);
                testRow.Children().Append(testText);

                testButton.Content(testRow);

                xaml::Automation::AutomationProperties::SetName(
                    testButton, resources::GetString(L"SequenceTestIt"));
            }

            {
                controls::ColumnDefinition left{};
                left.Width(xaml::GridLengthHelper::FromValueAndType(0.0, xaml::GridUnitType::Auto));

                controls::ColumnDefinition gap{};
                gap.Width(xaml::GridLengthHelper::FromValueAndType(1.0, xaml::GridUnitType::Star));

                controls::ColumnDefinition middle{};
                middle.Width(xaml::GridLengthHelper::FromValueAndType(0.0, xaml::GridUnitType::Auto));

                controls::ColumnDefinition right{};
                right.Width(xaml::GridLengthHelper::FromValueAndType(0.0, xaml::GridUnitType::Auto));

                toolbar.ColumnDefinitions().Append(left);
                toolbar.ColumnDefinitions().Append(gap);
                toolbar.ColumnDefinitions().Append(middle);
                toolbar.ColumnDefinitions().Append(right);

                summary.VerticalAlignment(xaml::VerticalAlignment::Center);

                controls::Grid::SetColumn(tools, 0);
                controls::Grid::SetColumn(summary, 2);
                controls::Grid::SetColumn(testButton, 3);

                toolbar.Children().Append(tools);
                toolbar.Children().Append(summary);
                toolbar.Children().Append(testButton);
            }

            controls::Grid addRow{};
            addRow.ColumnSpacing(9.0);

            {
                controls::ColumnDefinition left{};
                left.Width(xaml::GridLengthHelper::FromValueAndType(0.0, xaml::GridUnitType::Auto));

                controls::ColumnDefinition rest{};
                rest.Width(xaml::GridLengthHelper::FromValueAndType(1.0, xaml::GridUnitType::Star));

                addRow.ColumnDefinitions().Append(left);
                addRow.ColumnDefinitions().Append(rest);

                controls::Grid::SetColumn(addHeading, 0);
                controls::Grid::SetColumn(addScroll, 1);

                addRow.Children().Append(addHeading);
                addRow.Children().Append(addScroll);
            }

            root.Children().Append(toolbar);
            root.Children().Append(addRow);
            root.Children().Append(list);
            root.Children().Append(fields);
            root.Children().Append(modeRow);

            dialog.Content(root);

            // ---- keeping the list and the fields in step ----

            auto updating = std::make_shared<bool>(false);
            auto selected = std::make_shared<int32_t>(-1);

            auto const show = [](xaml::UIElement const& element, bool visible)
                {
                    element.Visibility(visible ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);
                };

            auto refreshFields = [=]()
                {
                    auto const index = *selected;

                    auto const valid = index >= 0 && index < static_cast<int32_t>(working->Steps.size());

                    upButton.IsEnabled(valid && index > 0);
                    downButton.IsEnabled(valid && index + 1 < static_cast<int32_t>(working->Steps.size()));
                    deleteButton.IsEnabled(valid);

                    if (!valid)
                    {
                        show(numberField, false);
                        show(waitField, false);
                        show(durationField, false);
                        show(repeatField, false);
                        show(deviceField, false);
                        show(channelField, false);
                        show(targetControlField, false);
                        show(targetValueField, false);
                        show(targetPageField, false);
                        show(bytesField, false);
                        show(fromFileButton, false);

                        return;
                    }

                    auto const& step = working->Steps[static_cast<size_t>(index)];

                    auto const isMessage = step.Kind == glass::SequenceStepKind::SendMidiMessage;
                    auto const isSysEx = step.Kind == glass::SequenceStepKind::SendSystemExclusive;

                    show(numberField, isMessage);
                    show(waitField, step.Kind == glass::SequenceStepKind::Wait);
                    show(durationField, isMessage && step.Message.Kind == glass::MessageKind::Note);
                    show(repeatField, step.Kind == glass::SequenceStepKind::RepeatBlockStart);
                    show(deviceField, isMessage || isSysEx);
                    show(channelField, isMessage);
                    show(targetControlField, step.Kind == glass::SequenceStepKind::SetControlValue);
                    show(targetValueField, step.Kind == glass::SequenceStepKind::SetControlValue);
                    show(targetPageField, step.Kind == glass::SequenceStepKind::GoToPage);
                    show(bytesField, isSysEx);
                    show(fromFileButton, isSysEx);

                    auto const previous = *updating;
                    *updating = true;

                    numberField.Value(step.Message.Number);
                    waitField.Value(step.WaitMilliseconds);
                    durationField.Value(step.DurationMilliseconds);
                    repeatField.Value(step.RepeatCount);
                    channelField.SelectedIndex(std::clamp(step.Message.ChannelIndex, 0, 15));
                    targetValueField.Value(step.TargetValue * 100.0);
                    bytesField.Text(winrt::hstring{ glass::FormatHexBytes(step.Message.SystemExclusive) });

                    auto deviceIndex = -1;

                    for (size_t i = 0; i < document.Devices.size(); ++i)
                    {
                        if (document.Devices[i].Name == step.Message.DeviceName)
                        {
                            deviceIndex = static_cast<int32_t>(i);
                            break;
                        }
                    }

                    deviceField.SelectedIndex(deviceIndex);

                    auto controlIndex = -1;

                    for (size_t i = 0; i < controlIds.size(); ++i)
                    {
                        if (controlIds[i] == step.TargetControlId)
                        {
                            controlIndex = static_cast<int32_t>(i);
                            break;
                        }
                    }

                    targetControlField.SelectedIndex(controlIndex);

                    auto pageIndex = -1;

                    for (size_t i = 0; i < document.Pages.size(); ++i)
                    {
                        if (document.Pages[i].Id == step.Message.TargetPageId)
                        {
                            pageIndex = static_cast<int32_t>(i);
                            break;
                        }
                    }

                    targetPageField.SelectedIndex(pageIndex);

                    *updating = previous;
                };

            auto refreshList = [=]()
                {
                    auto const previous = *updating;
                    *updating = true;

                    list.Items().Clear();

                    int32_t depth{ 0 };

                    for (size_t index = 0; index < working->Steps.size(); ++index)
                    {
                        auto const& step = working->Steps[index];

                        if (step.Kind == glass::SequenceStepKind::RepeatBlockEnd && depth > 0)
                        {
                            --depth;
                        }

                        controls::StackPanel row{};
                        row.Orientation(controls::Orientation::Horizontal);
                        row.Spacing(8);
                        row.Margin({ depth * 18.0, 0, 0, 0 });

                        controls::FontIcon icon{};
                        icon.Glyph(winrt::hstring{ std::wstring(1, StepChoices[0].Glyph) });

                        for (auto const& choice : StepChoices)
                        {
                            if (std::wcscmp(choice.NameKey, GlyphKeyForStep(step)) == 0)
                            {
                                icon.Glyph(winrt::hstring{ std::wstring(1, choice.Glyph) });
                                break;
                            }
                        }

                        icon.FontSize(12);
                        icon.Foreground(SequenceBrushNamed(L"AccentTextFillColorPrimaryBrush"));

                        controls::TextBlock text{};
                        text.Text(winrt::hstring{ DescribeStep(step, document) });
                        text.FontSize(12);

                        row.Children().Append(icon);
                        row.Children().Append(text);

                        controls::ListViewItem item{};
                        item.Content(row);
                        item.Tag(box_value(static_cast<int32_t>(index)));

                        xaml::Automation::AutomationProperties::SetName(item, text.Text());

                        list.Items().Append(item);

                        if (step.Kind == glass::SequenceStepKind::RepeatBlockStart)
                        {
                            ++depth;
                        }
                    }

                    if (*selected >= static_cast<int32_t>(working->Steps.size()))
                    {
                        *selected = static_cast<int32_t>(working->Steps.size()) - 1;
                    }

                    list.SelectedIndex(*selected);

                    summary.Text(resources::FormatString(
                        L"SequenceSummaryFormat",
                        static_cast<int32_t>(working->Steps.size()),
                        std::to_wstring(TotalWaitMilliseconds(working->Steps) / 1000.0).substr(0, 4)));

                    *updating = previous;

                    refreshFields();
                };

            list.SelectionChanged([=](auto&&, auto&&)
                {
                    if (*updating)
                    {
                        return;
                    }

                    *selected = list.SelectedIndex();

                    refreshFields();
                });

            // ---- the add chips ----

            for (auto const& choice : StepChoices)
            {
                controls::Button chip{};

                controls::StackPanel content{};
                content.Orientation(controls::Orientation::Horizontal);
                content.Spacing(6);

                controls::FontIcon icon{};
                icon.Glyph(winrt::hstring{ std::wstring(1, choice.Glyph) });
                icon.FontSize(11);

                controls::TextBlock text{};
                text.Text(resources::GetString(choice.NameKey));
                text.FontSize(11);

                content.Children().Append(icon);
                content.Children().Append(text);

                chip.Content(content);
                chip.FontSize(11);
                chip.CornerRadius({ 13, 13, 13, 13 });
                chip.Padding({ 11, 4, 11, 4 });

                xaml::Automation::AutomationProperties::SetName(chip, text.Text());

                auto const kind = choice.Kind;
                auto const messageKind = choice.Message;

                chip.Click([=](auto&&, auto&&)
                    {
                        if (working->Steps.size() >= glass::MaximumStepsPerSequence)
                        {
                            return;
                        }

                        glass::SequenceStep step{};

                        step.Kind = kind;
                        step.Message.Kind = messageKind;
                        step.Message.Trigger = glass::MessageTrigger::Changes;
                        step.WaitMilliseconds = kind == glass::SequenceStepKind::Wait ? 250 : 0;
                        step.RepeatCount = 2;

                        if (!document.Devices.empty())
                        {
                            step.Message.DeviceName = document.Devices[0].Name;
                        }

                        if (messageKind == glass::MessageKind::Note)
                        {
                            step.Message.Number = 60;
                        }

                        // A note sent from a sequence with no end would hang. The pair goes in
                        // together, with a gap, so a step list is never a stuck note.
                        auto const insertAt = *selected < 0
                            ? working->Steps.size()
                            : static_cast<size_t>(*selected) + 1;

                        working->Steps.insert(
                            working->Steps.begin() + static_cast<ptrdiff_t>(insertAt), step);

                        auto added = size_t{ 1 };

                        if (kind == glass::SequenceStepKind::RepeatBlockStart)
                        {
                            glass::SequenceStep end{};
                            end.Kind = glass::SequenceStepKind::RepeatBlockEnd;

                            working->Steps.insert(
                                working->Steps.begin() + static_cast<ptrdiff_t>(insertAt + 1), end);

                            added = 2;
                        }

                        *selected = static_cast<int32_t>(insertAt);

                        UNREFERENCED_PARAMETER(added);

                        refreshList();
                    });

                addChips.Children().Append(chip);
            }

            // ---- editing the selected step ----

            auto const withSelected = [=](std::function<void(glass::SequenceStep&)> const& edit)
                {
                    if (*updating)
                    {
                        return;
                    }

                    auto const index = *selected;

                    if (index < 0 || index >= static_cast<int32_t>(working->Steps.size()))
                    {
                        return;
                    }

                    edit(working->Steps[static_cast<size_t>(index)]);

                    refreshList();
                };

            numberField.ValueChanged([=](auto&&, auto&&)
                {
                    auto const value = numberField.Value();

                    if (std::isfinite(value))
                    {
                        withSelected([value](glass::SequenceStep& step)
                            { step.Message.Number = static_cast<uint32_t>(value); });
                    }
                });

            waitField.ValueChanged([=](auto&&, auto&&)
                {
                    auto const value = waitField.Value();

                    if (std::isfinite(value))
                    {
                        withSelected([value](glass::SequenceStep& step)
                            { step.WaitMilliseconds = static_cast<uint32_t>(value); });
                    }
                });

            repeatField.ValueChanged([=](auto&&, auto&&)
                {
                    auto const value = repeatField.Value();

                    if (std::isfinite(value))
                    {
                        withSelected([value](glass::SequenceStep& step)
                            { step.RepeatCount = static_cast<uint32_t>(value); });
                    }
                });

            durationField.ValueChanged([=](auto&&, auto&&)
                {
                    auto const value = durationField.Value();

                    if (std::isfinite(value))
                    {
                        withSelected([value](glass::SequenceStep& step)
                            { step.DurationMilliseconds = static_cast<uint32_t>(value); });
                    }
                });

            deviceField.SelectionChanged([=](auto&&, auto&&)
                {
                    auto const index = deviceField.SelectedIndex();

                    if (index >= 0 && index < static_cast<int32_t>(document.Devices.size()))
                    {
                        auto const name = document.Devices[static_cast<size_t>(index)].Name;

                        withSelected([&name](glass::SequenceStep& step)
                            { step.Message.DeviceName = name; });
                    }
                });

            channelField.SelectionChanged([=](auto&&, auto&&)
                {
                    auto const index = channelField.SelectedIndex();

                    if (index >= 0)
                    {
                        withSelected([index](glass::SequenceStep& step)
                            { step.Message.ChannelIndex = index; });
                    }
                });

            targetControlField.SelectionChanged([=](auto&&, auto&&)
                {
                    auto const index = targetControlField.SelectedIndex();

                    if (index >= 0 && index < static_cast<int32_t>(controlIds.size()))
                    {
                        auto const id = controlIds[static_cast<size_t>(index)];

                        withSelected([&id](glass::SequenceStep& step)
                            { step.TargetControlId = id; });
                    }
                });

            targetValueField.ValueChanged([=](auto&&, auto&&)
                {
                    auto const value = targetValueField.Value() / 100.0;

                    withSelected([value](glass::SequenceStep& step)
                        { step.TargetValue = value; });
                });

            targetPageField.SelectionChanged([=](auto&&, auto&&)
                {
                    auto const index = targetPageField.SelectedIndex();

                    if (index >= 0 && index < static_cast<int32_t>(document.Pages.size()))
                    {
                        auto const id = document.Pages[static_cast<size_t>(index)].Id;

                        withSelected([&id](glass::SequenceStep& step)
                            { step.Message.TargetPageId = id; });
                    }
                });

            bytesField.LostFocus([=](auto&&, auto&&)
                {
                    auto const bytes = glass::ParseHexBytes(
                        std::wstring{ bytesField.Text() }, glass::MaximumSystemExclusiveBytes);

                    if (bytes.empty() && !bytesField.Text().empty())
                    {
                        return;
                    }

                    withSelected([&bytes](glass::SequenceStep& step)
                        { step.Message.SystemExclusive = bytes; });
                });

            fromFileButton.Click([=](auto&&, auto&&)
                {
                    std::vector<uint8_t> bytes{};

                    if (TryReadSystemExclusiveFile(bytes))
                    {
                        withSelected([&bytes](glass::SequenceStep& step)
                            { step.Message.SystemExclusive = bytes; });
                    }
                });

            upButton.Click([=](auto&&, auto&&)
                {
                    auto const index = *selected;

                    if (index > 0 && index < static_cast<int32_t>(working->Steps.size()))
                    {
                        std::swap(
                            working->Steps[static_cast<size_t>(index)],
                            working->Steps[static_cast<size_t>(index) - 1]);

                        *selected = index - 1;

                        refreshList();
                    }
                });

            downButton.Click([=](auto&&, auto&&)
                {
                    auto const index = *selected;

                    if (index >= 0 && index + 1 < static_cast<int32_t>(working->Steps.size()))
                    {
                        std::swap(
                            working->Steps[static_cast<size_t>(index)],
                            working->Steps[static_cast<size_t>(index) + 1]);

                        *selected = index + 1;

                        refreshList();
                    }
                });

            deleteButton.Click([=](auto&&, auto&&)
                {
                    auto const index = *selected;

                    if (index >= 0 && index < static_cast<int32_t>(working->Steps.size()))
                    {
                        working->Steps.erase(
                            working->Steps.begin() + static_cast<ptrdiff_t>(index));

                        refreshList();
                    }
                });

            duplicateButton.Click([=](auto&&, auto&&)
                {
                    auto const index = *selected;

                    if (index < 0 ||
                        index >= static_cast<int32_t>(working->Steps.size()) ||
                        working->Steps.size() >= glass::MaximumStepsPerSequence)
                    {
                        return;
                    }

                    // The copy lands directly under the original, which is what makes this the
                    // fast way to build a run of similar steps.
                    auto const copy = working->Steps[static_cast<size_t>(index)];

                    working->Steps.insert(
                        working->Steps.begin() + static_cast<ptrdiff_t>(index) + 1, copy);

                    *selected = index + 1;

                    refreshList();
                });

            modeChoices.SelectionChanged([=](auto&&, auto&&)
                {
                    auto const index = modeChoices.SelectedIndex();

                    if (index >= 0 && index <= 2)
                    {
                        working->Mode = static_cast<glass::SequenceRunMode>(index);
                    }
                });

            // Test it plays the copy on screen, not the one in the document, so what runs is the
            // edit being judged. It borrows the editor's player and hands output back when the
            // dialog closes, so leaving the dialog cannot leave the gate open.
            auto const monitoredIndex = [this]() -> uint32_t
                {
                    if (auto const* const control = SingleSelectedControl())
                    {
                        if (auto const index = m_editor.ControlIndexOf(control->Id); index >= 0)
                        {
                            return static_cast<uint32_t>(index);
                        }
                    }

                    return 0;
                }();

            auto testing = std::make_shared<bool>(false);

            testButton.Click([=](auto&&, auto&&)
                {
                    StartPlayerForLearning();

                    if (m_player == nullptr)
                    {
                        return;
                    }

                    if (*testing)
                    {
                        m_player->StopSequenceNow(monitoredIndex);
                        m_player->SetOutputEnabled(m_tryMode);
                        *testing = false;
                        return;
                    }

                    m_player->SetOutputEnabled(true);

                    if (m_player->RunSequenceNow(*working, monitoredIndex))
                    {
                        *testing = true;
                    }
                    else
                    {
                        m_player->SetOutputEnabled(m_tryMode);
                    }
                });

            refreshList();

            auto const result = co_await dialog.ShowAsync();

            // Whatever the dialog is closed with, nothing is left running and the output gate
            // goes back to whatever the mode says it should be.
            if (m_player != nullptr)
            {
                m_player->StopSequenceNow(monitoredIndex);
                m_player->SetOutputEnabled(m_tryMode);
            }

            if (result != controls::ContentDialogResult::Primary)
            {
                co_return;
            }

            if (m_editor.SetSequence(sequenceName, *working))
            {
                RefreshInspector();
                MarkChanged();
            }
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to edit the sequence.")
    }

    // The Win32 common item dialog, not Windows.Storage.Pickers: this is a desktop app and the
    // picker needs a window handle it can be modal to.
    _Use_decl_annotations_
    bool EditorWindow::TryReadSystemExclusiveFile(std::vector<uint8_t>& bytes)
    {
        bytes.clear();

        try
        {
            auto dialog = winrt::create_instance<IFileOpenDialog>(CLSID_FileOpenDialog);

            if (dialog == nullptr)
            {
                return false;
            }

            COMDLG_FILTERSPEC const filters[]
            {
                { L"System exclusive (*.syx)", L"*.syx" },
                { L"All files", L"*.*" },
            };

            dialog->SetFileTypes(static_cast<UINT>(std::size(filters)), filters);
            dialog->SetTitle(resources::GetString(L"SysExOpenTitle").c_str());

            if (FAILED(dialog->Show(m_chrome.WindowHandle())))
            {
                return false;
            }

            winrt::com_ptr<IShellItem> item{};

            if (FAILED(dialog->GetResult(item.put())) || item == nullptr)
            {
                return false;
            }

            wil::unique_cotaskmem_string path{};

            if (FAILED(item->GetDisplayName(SIGDN_FILESYSPATH, path.put())))
            {
                return false;
            }

            wil::unique_hfile file{ ::CreateFileW(
                path.get(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                FILE_ATTRIBUTE_NORMAL, nullptr) };

            if (!file)
            {
                return false;
            }

            LARGE_INTEGER size{};

            if (!::GetFileSizeEx(file.get(), &size) ||
                size.QuadPart <= 0 ||
                static_cast<uint64_t>(size.QuadPart) > glass::MaximumSystemExclusiveBytes)
            {
                return false;
            }

            bytes.resize(static_cast<size_t>(size.QuadPart));

            DWORD read{ 0 };

            if (!::ReadFile(file.get(), bytes.data(), static_cast<DWORD>(bytes.size()), &read, nullptr) ||
                read != bytes.size())
            {
                bytes.clear();
                return false;
            }

            return true;
        }
        catch (...)
        {
            bytes.clear();
            return false;
        }
    }
}
