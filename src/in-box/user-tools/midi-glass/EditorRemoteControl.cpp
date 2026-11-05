// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// MIDI remote control in the editor.
//
// The rules live in layout settings, one card each: when this message arrives from this device,
// do this. Learn fills in the "when" from a tap on the controller, and a card lights up when its
// message arrives, so somebody can see which switch is which without opening a monitor.
//
// Try mode draws the controls a rule turned off or hid, the same way the runtime window does.
// Edit mode always draws everything, because a control nobody can see is one nobody can edit.

#include "pch.h"
#include "EditorWindow.xaml.h"

#include "StringResources.h"
#include "GlassControl.h"
#include "PadGrid.h"
#include "Fretboard.h"
#include "SpinButtons.h"

namespace resources = ::midiglass::resources;

namespace winrt::midiglass::implementation
{
    namespace
    {
        namespace automation = ::winrt::Microsoft::UI::Xaml::Automation;

        // How long a card stays lit after its message arrives.
        constexpr uint64_t RemoteFlashMilliseconds = 350;

        media::Brush RemoteBrush(_In_ wchar_t const* key)
        {
            return xaml::Application::Current().Resources().Lookup(box_value(key)).as<media::Brush>();
        }

        controls::TextBlock RemoteText(_In_ winrt::hstring const& text, _In_ double size, _In_ wchar_t const* brushKey)
        {
            controls::TextBlock block{};

            block.Text(text);
            block.FontSize(size);
            block.TextWrapping(xaml::TextWrapping::Wrap);
            block.Foreground(RemoteBrush(brushKey));

            return block;
        }

        // The actions, in the order the list offers them.
        struct ActionChoice
        {
            glass::RemoteActionKind Action{};
            wchar_t const* NameKey{ nullptr };
        };

        constexpr ActionChoice ActionChoices[]
        {
            { glass::RemoteActionKind::GoToPage, L"RemoteActionGoToPage" },
            { glass::RemoteActionKind::NextPage, L"RemoteActionNextPage" },
            { glass::RemoteActionKind::PreviousPage, L"RemoteActionPreviousPage" },
            { glass::RemoteActionKind::PageFromMessage, L"RemoteActionPageFromMessage" },
            { glass::RemoteActionKind::PressControl, L"RemoteActionPressControl" },
            { glass::RemoteActionKind::SetControlValue, L"RemoteActionSetControlValue" },
            { glass::RemoteActionKind::EnableControl, L"RemoteActionEnableControl" },
            { glass::RemoteActionKind::DisableControl, L"RemoteActionDisableControl" },
            { glass::RemoteActionKind::ToggleEnabled, L"RemoteActionToggleEnabled" },
            { glass::RemoteActionKind::ShowControl, L"RemoteActionShowControl" },
            { glass::RemoteActionKind::HideControl, L"RemoteActionHideControl" },
            { glass::RemoteActionKind::ToggleShown, L"RemoteActionToggleShown" },
            { glass::RemoteActionKind::ShowOnlyThis, L"RemoteActionShowOnlyThis" },
            { glass::RemoteActionKind::PadGrid, L"RemoteActionPadGrid" },
            { glass::RemoteActionKind::SetKeyRoot, L"RemoteActionSetKeyRoot" },
            { glass::RemoteActionKind::SetScale, L"RemoteActionSetScale" },
            { glass::RemoteActionKind::RunSequence, L"RemoteActionRunSequence" },
            { glass::RemoteActionKind::Panic, L"RemoteActionPanic" },
        };

        constexpr glass::MessageKind KindChoices[]
        {
            glass::MessageKind::ProgramChange,
            glass::MessageKind::ControlChange,
            glass::MessageKind::Note,
        };

        constexpr wchar_t const* KindKeys[]
        {
            L"RemoteKindProgramChange",
            L"RemoteKindControlChange",
            L"RemoteKindNote",
        };

        constexpr wchar_t const* PadFunctionKeys[glass::PadGridFunctionCount]
        {
            L"PadFunctionOctaveUp", L"PadFunctionOctaveDown", L"PadFunctionRowUp", L"PadFunctionRowDown",
            L"PadFunctionShiftLeft", L"PadFunctionShiftRight", L"PadFunctionSemitoneUp", L"PadFunctionSemitoneDown",
            L"PadFunctionInKeyOnlyOn", L"PadFunctionInKeyOnlyOff", L"PadFunctionInKeyOnlyToggle",
            L"PadFunctionNextRoot", L"PadFunctionPreviousRoot", L"PadFunctionNextScale", L"PadFunctionPreviousScale",
            L"PadFunctionNextRowOffset", L"PadFunctionReset", L"PadFunctionCapoUp", L"PadFunctionCapoDown",
            L"PadFunctionUpTheNeck", L"PadFunctionDownTheNeck", L"PadFunctionMuteStrings",
        };

        int32_t KindPosition(_In_ glass::MessageKind kind) noexcept
        {
            for (int32_t at = 0; at < static_cast<int32_t>(std::size(KindChoices)); ++at)
            {
                if (KindChoices[at] == kind)
                {
                    return at;
                }
            }

            return 0;
        }

        bool UsesTarget(_In_ glass::RemoteActionKind action) noexcept
        {
            switch (action)
            {
            case glass::RemoteActionKind::PressControl:
            case glass::RemoteActionKind::SetControlValue:
            case glass::RemoteActionKind::EnableControl:
            case glass::RemoteActionKind::DisableControl:
            case glass::RemoteActionKind::ToggleEnabled:
            case glass::RemoteActionKind::ShowControl:
            case glass::RemoteActionKind::HideControl:
            case glass::RemoteActionKind::ToggleShown:
            case glass::RemoteActionKind::ShowOnlyThis:
                return true;

            default:
                return false;
            }
        }

        // One thing a rule can aim at: a control, or a group of them.
        struct TargetChoice
        {
            bool IsGroup{ false };
            std::wstring Id{};
            std::wstring Name{};
        };

        std::vector<TargetChoice> TargetChoices(_In_ glass::LayoutDocument const& document, _In_ bool controlsOnly)
        {
            std::vector<TargetChoice> choices{};

            for (auto const& page : document.Pages)
            {
                std::vector<std::wstring> groups{};

                for (auto const& control : page.Controls)
                {
                    if (control.Id.empty())
                    {
                        continue;
                    }

                    TargetChoice choice{};
                    choice.Id = control.Id;
                    choice.Name = resources::FormatString(
                        L"RemoteControlOnPageFormat",
                        control.Label.empty() ? control.Id : control.Label,
                        page.Name);
                    choices.push_back(std::move(choice));

                    if (!control.GroupId.empty() && std::find(groups.begin(), groups.end(), control.GroupId) == groups.end())
                    {
                        groups.push_back(control.GroupId);
                    }
                }

                if (controlsOnly)
                {
                    continue;
                }

                for (size_t at = 0; at < groups.size(); ++at)
                {
                    auto const* const named = page.FindGroup(groups[at]);

                    TargetChoice choice{};
                    choice.IsGroup = true;
                    choice.Id = groups[at];
                    choice.Name = resources::FormatString(
                        L"RemoteGroupFormat",
                        named != nullptr && !named->Name.empty()
                            ? named->Name
                            : std::wstring{ resources::FormatString(L"OutlineGroupFormat", std::to_wstring(at + 1)) },
                        page.Name);
                    choices.push_back(std::move(choice));
                }
            }

            return choices;
        }

        // A combo box with a header, filled and selected, whose changes are read back only when
        // a person made them.
        controls::ComboBox MakeCombo(
            _In_ wchar_t const* headerKey,
            _In_ std::vector<winrt::hstring> const& items,
            _In_ int32_t selected,
            _In_ double width)
        {
            controls::ComboBox combo{};

            combo.Header(box_value(resources::GetString(headerKey)));
            combo.MinWidth(width);
            combo.MaxWidth(width + 140.0);

            for (auto const& item : items)
            {
                combo.Items().Append(box_value(item));
            }

            combo.SelectedIndex(selected);

            return combo;
        }

        controls::NumberBox MakeNumber(
            _In_ wchar_t const* headerKey,
            _In_ double value,
            _In_ double lowest,
            _In_ double highest)
        {
            controls::NumberBox box{};

            box.Header(box_value(resources::GetString(headerKey)));
            box.Minimum(lowest);
            box.Maximum(highest);
            box.Value(value);
            box.MinWidth(96.0);
            box.SmallChange(1.0);
            midiapp::UseTouchSpinButtons(box);

            return box;
        }

        std::vector<winrt::hstring> ChannelItems(_In_ bool withAny)
        {
            std::vector<winrt::hstring> items{};

            if (withAny)
            {
                items.push_back(resources::GetString(L"RemoteAnyChannel"));
            }

            for (int32_t channel = 1; channel <= 16; ++channel)
            {
                items.push_back(winrt::to_hstring(channel));
            }

            return items;
        }

        // Unlike a check box's IsChecked, never null.
        bool IsTicked(_In_ controls::Primitives::ToggleButton const& toggle) noexcept
        {
            try
            {
                auto const checked = toggle.IsChecked();

                return checked != nullptr && checked.Value();
            }
            catch (...)
            {
                return false;
            }
        }
    }

    // ---------------------------------------------------------------- the pane

    void EditorWindow::RefreshRemoteControlPane()
    {
        try
        {
            auto panel = RemoteControlPanel();

            auto const previous = m_updatingSettings;
            m_updatingSettings = true;

            panel.Children().Clear();
            m_remoteRuleCards.clear();
            m_remoteFlashUntil.clear();

            auto const& document = m_editor.Document();

            auto heading = RemoteText(resources::GetString(L"RemoteHeading"), 14.0, L"TextFillColorPrimaryBrush");
            heading.FontWeight(winrt::Windows::UI::Text::FontWeights::SemiBold());
            automation::AutomationProperties::SetHeadingLevel(heading, automation::Peers::AutomationHeadingLevel::Level2);
            panel.Children().Append(heading);

            auto caption = RemoteText(resources::GetString(L"RemoteCaption"), 12.0, L"TextFillColorSecondaryBrush");
            caption.Margin(xaml::ThicknessHelper::FromLengths(0.0, 4.0, 0.0, 10.0));
            panel.Children().Append(caption);

            if (document.Devices.empty())
            {
                auto none = RemoteText(resources::GetString(L"RemoteNoDevices"), 12.0, L"SystemFillColorCautionBrush");
                none.Margin(xaml::ThicknessHelper::FromLengths(0.0, 0.0, 0.0, 10.0));
                panel.Children().Append(none);
            }

            AddRemoteRuleCards();
            AddTellPageSection();

            auto note = RemoteText(resources::GetString(L"RemoteLoopNote"), 11.0, L"TextFillColorTertiaryBrush");
            note.Margin(xaml::ThicknessHelper::FromLengths(0.0, 16.0, 0.0, 0.0));
            panel.Children().Append(note);

            m_updatingSettings = previous;

            // The player listens while this pane is open, so a card can light when its message
            // arrives and Learn has something to hear. Its output stays shut outside Try mode.
            if (m_player == nullptr)
            {
                StartPlayerForLearning();
            }
            else
            {
                m_player->UpdateRemoteRules(document);
            }

            if (m_remoteBringIntoView >= 0 && static_cast<size_t>(m_remoteBringIntoView) < m_remoteRuleCards.size())
            {
                m_remoteRuleCards[static_cast<size_t>(m_remoteBringIntoView)].StartBringIntoView();
            }

            m_remoteBringIntoView = -1;
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to show the MIDI remote control rules.")
    }

    void EditorWindow::AddRemoteRuleCards()
    {
        auto panel = RemoteControlPanel();
        auto const& rules = m_editor.Document().Remote.Rules;

        auto rulesHeading = RemoteText(resources::GetString(L"RemoteRulesHeading"), 13.0, L"TextFillColorPrimaryBrush");
        rulesHeading.FontWeight(winrt::Windows::UI::Text::FontWeights::SemiBold());
        rulesHeading.Margin(xaml::ThicknessHelper::FromLengths(0.0, 4.0, 0.0, 6.0));
        automation::AutomationProperties::SetHeadingLevel(rulesHeading, automation::Peers::AutomationHeadingLevel::Level3);
        panel.Children().Append(rulesHeading);

        if (rules.empty())
        {
            auto empty = RemoteText(resources::GetString(L"RemoteNoRules"), 12.0, L"TextFillColorTertiaryBrush");
            empty.Margin(xaml::ThicknessHelper::FromLengths(0.0, 0.0, 0.0, 8.0));
            panel.Children().Append(empty);
        }

        for (size_t index = 0; index < rules.size(); ++index)
        {
            auto card = BuildRemoteRuleCard(index);

            m_remoteRuleCards.push_back(card);
            m_remoteFlashUntil.push_back(0);
            panel.Children().Append(card);
        }

        controls::Button add{};
        add.Content(box_value(resources::GetString(L"RemoteAddRule")));
        add.Margin(xaml::ThicknessHelper::FromLengths(0.0, 4.0, 0.0, 0.0));
        add.IsEnabled(rules.size() < glass::MaximumRemoteRules);

        add.Click([weak = get_weak()](auto&&, auto&&)
            {
                auto strong = weak.get();

                if (strong == nullptr)
                {
                    return;
                }

                auto const& document = strong->m_editor.Document();

                strong->m_remoteBringIntoView = static_cast<int32_t>(document.Remote.Rules.size());

                // A new rule listens to the layout's first device, which on a layout built for a
                // foot controller is the foot controller.
                std::wstring device{};

                if (!document.Devices.empty())
                {
                    device = document.Devices.front().Name;
                }

                strong->EditRemoteControl([device](glass::RemoteControl& remote)
                    {
                        glass::RemoteRule rule{};
                        rule.When.DeviceName = device;
                        rule.When.Number = static_cast<int32_t>(remote.Rules.size());
                        remote.Rules.push_back(std::move(rule));
                    }, true);
            });

        panel.Children().Append(add);
    }

    _Use_decl_annotations_
    controls::Border EditorWindow::BuildRemoteRuleCard(size_t ruleIndex)
    {
        auto const& document = m_editor.Document();
        auto const rule = document.Remote.Rules[ruleIndex];
        auto weak = get_weak();
        auto const number = static_cast<int32_t>(ruleIndex + 1);

        // Every edit on this card goes through here: a copy of the rule changed, and handed
        // back. Rebuild when the change alters which fields the card shows.
        auto const change = [weak, ruleIndex](std::function<void(glass::RemoteRule&)> edit, bool rebuild)
            {
                if (auto strong = weak.get(); strong != nullptr && !strong->m_updatingSettings)
                {
                    strong->EditRemoteControl([ruleIndex, edit](glass::RemoteControl& remote)
                        {
                            if (ruleIndex < remote.Rules.size())
                            {
                                edit(remote.Rules[ruleIndex]);
                            }
                        }, rebuild);
                }
            };

        controls::Border card{};
        card.CornerRadius(xaml::CornerRadiusHelper::FromUniformRadius(6.0));
        card.Background(RemoteBrush(L"SubtleFillColorSecondaryBrush"));
        card.Padding(xaml::ThicknessHelper::FromLengths(12.0, 8.0, 8.0, 10.0));
        card.Margin(xaml::ThicknessHelper::FromLengths(0.0, 0.0, 0.0, 8.0));

        controls::StackPanel body{};
        body.Spacing(6.0);

        // ---- the top line: on, its name, learn, and its menu ----

        controls::Grid top{};
        top.ColumnSpacing(8.0);

        for (auto const width : { xaml::GridLengthHelper::FromValueAndType(0.0, xaml::GridUnitType::Auto),
                                  xaml::GridLengthHelper::FromValueAndType(1.0, xaml::GridUnitType::Star),
                                  xaml::GridLengthHelper::FromValueAndType(0.0, xaml::GridUnitType::Auto),
                                  xaml::GridLengthHelper::FromValueAndType(0.0, xaml::GridUnitType::Auto) })
        {
            controls::ColumnDefinition column{};
            column.Width(width);
            top.ColumnDefinitions().Append(column);
        }

        controls::CheckBox on{};
        on.Content(box_value(resources::FormatString(L"RemoteRuleNameFormat", std::to_wstring(number))));
        on.IsChecked(rule.Enabled);
        on.MinWidth(0.0);
        controls::ToolTipService::SetToolTip(on, box_value(resources::GetString(L"RemoteRuleOnTip")));
        on.Click([change](auto&& sender, auto&&)
            {
                auto const ticked = IsTicked(sender.template as<controls::Primitives::ToggleButton>());
                change([ticked](glass::RemoteRule& edited) { edited.Enabled = ticked; }, false);
            });
        controls::Grid::SetColumn(on, 0);

        auto learning = m_remoteLearnRule == static_cast<int32_t>(ruleIndex);

        auto status = RemoteText(
            learning ? resources::GetString(L"RemoteLearnWaiting") : winrt::hstring{},
            11.0,
            L"AccentTextFillColorPrimaryBrush");
        status.VerticalAlignment(xaml::VerticalAlignment::Center);
        controls::Grid::SetColumn(status, 1);

        controls::Primitives::ToggleButton learn{};
        learn.Content(box_value(resources::GetString(L"RemoteLearn")));
        learn.IsChecked(learning);
        automation::AutomationProperties::SetName(
            learn, resources::FormatString(L"RemoteLearnNameFormat", std::to_wstring(number)));
        learn.Click([weak, ruleIndex](auto&& sender, auto&&)
            {
                auto const ticked = IsTicked(sender.template as<controls::Primitives::ToggleButton>());

                if (auto strong = weak.get())
                {
                    strong->SetRemoteLearnRule(ticked ? static_cast<int32_t>(ruleIndex) : -1);
                }
            });
        controls::Grid::SetColumn(learn, 2);

        controls::Button menuButton{};
        {
            controls::FontIcon glyph{};
            glyph.Glyph(L"\uE712");
            glyph.FontSize(13.0);

            menuButton.Content(glyph);
            menuButton.Width(30.0);
            menuButton.Height(28.0);
            menuButton.Padding(xaml::ThicknessHelper::FromUniformLength(0.0));
            menuButton.Background(nullptr);
            menuButton.BorderThickness(xaml::ThicknessHelper::FromUniformLength(0.0));
            automation::AutomationProperties::SetName(
                menuButton, resources::FormatString(L"RemoteRuleMenuNameFormat", std::to_wstring(number)));
        }
        controls::Grid::SetColumn(menuButton, 3);

        {
            controls::MenuFlyout menu{};
            auto const count = document.Remote.Rules.size();

            auto const item = [&menu](wchar_t const* key, bool enabled, std::function<void(glass::RemoteControl&)> edit, auto owner)
                {
                    controls::MenuFlyoutItem entry{};
                    entry.Text(resources::GetString(key));
                    entry.IsEnabled(enabled);
                    entry.Click([owner, edit](auto&&, auto&&)
                        {
                            if (auto strong = owner.get())
                            {
                                strong->EditRemoteControl(edit, true);
                            }
                        });
                    menu.Items().Append(entry);
                };

            item(L"RemoteMenuMoveUp", ruleIndex > 0, [ruleIndex](glass::RemoteControl& remote)
                {
                    if (ruleIndex > 0 && ruleIndex < remote.Rules.size())
                    {
                        std::swap(remote.Rules[ruleIndex], remote.Rules[ruleIndex - 1]);
                    }
                }, weak);

            item(L"RemoteMenuMoveDown", ruleIndex + 1 < count, [ruleIndex](glass::RemoteControl& remote)
                {
                    if (ruleIndex + 1 < remote.Rules.size())
                    {
                        std::swap(remote.Rules[ruleIndex], remote.Rules[ruleIndex + 1]);
                    }
                }, weak);

            item(L"RemoteMenuDuplicate", count < glass::MaximumRemoteRules, [ruleIndex](glass::RemoteControl& remote)
                {
                    if (ruleIndex < remote.Rules.size() && remote.Rules.size() < glass::MaximumRemoteRules)
                    {
                        auto copy = remote.Rules[ruleIndex];
                        remote.Rules.insert(remote.Rules.begin() + static_cast<ptrdiff_t>(ruleIndex) + 1, std::move(copy));
                    }
                }, weak);

            menu.Items().Append(controls::MenuFlyoutSeparator{});

            item(L"RemoteMenuDelete", true, [ruleIndex](glass::RemoteControl& remote)
                {
                    if (ruleIndex < remote.Rules.size())
                    {
                        remote.Rules.erase(remote.Rules.begin() + static_cast<ptrdiff_t>(ruleIndex));
                    }
                }, weak);

            menuButton.Flyout(menu);
        }

        top.Children().Append(on);
        top.Children().Append(status);
        top.Children().Append(learn);
        top.Children().Append(menuButton);
        body.Children().Append(top);

        // ---- when ----

        auto whenHeading = RemoteText(resources::GetString(L"RemoteWhenHeading"), 11.0, L"TextFillColorTertiaryBrush");
        body.Children().Append(whenHeading);

        controls::StackPanel when{};
        when.Orientation(controls::Orientation::Horizontal);
        when.Spacing(8.0);

        {
            std::vector<winrt::hstring> devices{ resources::GetString(L"RemoteAnyDevice") };
            int32_t selected{ 0 };

            for (size_t at = 0; at < document.Devices.size(); ++at)
            {
                devices.push_back(winrt::hstring{ document.Devices[at].Name });

                if (document.Devices[at].Name == rule.When.DeviceName)
                {
                    selected = static_cast<int32_t>(at + 1);
                }
            }

            // A name that is not in the table any more is shown as itself, so a renamed device
            // is visible rather than quietly read as "any".
            if (selected == 0 && !rule.When.DeviceName.empty())
            {
                devices.push_back(winrt::hstring{ rule.When.DeviceName });
                selected = static_cast<int32_t>(devices.size() - 1);
            }

            auto combo = MakeCombo(L"RemoteDeviceHeader", devices, selected, 180.0);
            std::vector<std::wstring> names{ std::wstring{} };

            for (auto const& device : document.Devices)
            {
                names.push_back(device.Name);
            }

            names.push_back(rule.When.DeviceName);

            combo.SelectionChanged([change, names](auto&& sender, auto&&)
                {
                    auto const at = sender.template as<controls::ComboBox>().SelectedIndex();

                    if (at >= 0 && static_cast<size_t>(at) < names.size())
                    {
                        auto const name = names[static_cast<size_t>(at)];
                        change([name](glass::RemoteRule& edited) { edited.When.DeviceName = name; }, false);
                    }
                });

            when.Children().Append(combo);
        }

        {
            std::vector<winrt::hstring> kinds{};

            for (auto const* const key : KindKeys)
            {
                kinds.push_back(resources::GetString(key));
            }

            auto combo = MakeCombo(L"RemoteKindHeader", kinds, KindPosition(rule.When.Kind), 150.0);

            combo.SelectionChanged([change](auto&& sender, auto&&)
                {
                    auto const at = sender.template as<controls::ComboBox>().SelectedIndex();

                    if (at >= 0 && at < static_cast<int32_t>(std::size(KindChoices)))
                    {
                        auto const kind = KindChoices[at];
                        change([kind](glass::RemoteRule& edited) { edited.When.Kind = kind; }, true);
                    }
                });

            when.Children().Append(combo);
        }

        {
            auto combo = MakeCombo(
                L"RemoteChannelHeader",
                ChannelItems(true),
                rule.When.ChannelIndex == glass::AnyRemoteChannel ? 0 : rule.When.ChannelIndex + 1,
                90.0);

            combo.SelectionChanged([change](auto&& sender, auto&&)
                {
                    auto const at = sender.template as<controls::ComboBox>().SelectedIndex();

                    if (at >= 0)
                    {
                        auto const channel = at == 0 ? glass::AnyRemoteChannel : at - 1;
                        change([channel](glass::RemoteRule& edited) { edited.When.ChannelIndex = channel; }, false);
                    }
                });

            when.Children().Append(combo);
        }

        {
            auto const numberKey = rule.When.Kind == glass::MessageKind::Note
                ? L"RemoteNumberNote"
                : (rule.When.Kind == glass::MessageKind::ControlChange ? L"RemoteNumberController" : L"RemoteNumberProgram");

            auto const any = rule.When.Number == glass::AnyRemoteNumber;
            auto box = MakeNumber(numberKey, any ? 0.0 : rule.When.Number, 0.0, 127.0);
            box.IsEnabled(!any);

            box.ValueChanged([change](controls::NumberBox const&, controls::NumberBoxValueChangedEventArgs const& args)
                {
                    auto const value = args.NewValue();

                    if (std::isfinite(value))
                    {
                        auto const number = std::clamp(static_cast<int32_t>(std::lround(value)), 0, 127);
                        change([number](glass::RemoteRule& edited) { edited.When.Number = number; }, false);
                    }
                });

            controls::CheckBox anyBox{};
            anyBox.Content(box_value(resources::GetString(L"RemoteAnyNumber")));
            anyBox.IsChecked(any);
            anyBox.VerticalAlignment(xaml::VerticalAlignment::Bottom);
            anyBox.MinWidth(0.0);
            automation::AutomationProperties::SetName(anyBox, resources::GetString(L"RemoteAnyNumberName"));
            anyBox.Click([change](auto&& sender, auto&&)
                {
                    auto const ticked = IsTicked(sender.template as<controls::Primitives::ToggleButton>());
                    change([ticked](glass::RemoteRule& edited)
                        {
                            edited.When.Number = ticked ? glass::AnyRemoteNumber : 0;
                        }, true);
                });

            when.Children().Append(box);
            when.Children().Append(anyBox);
        }

        body.Children().Append(when);

        if (rule.When.Kind == glass::MessageKind::ControlChange && !glass::TakesFromMessage(rule))
        {
            body.Children().Append(RemoteText(resources::GetString(L"RemoteControlChangeNote"), 11.0, L"TextFillColorTertiaryBrush"));
        }

        // ---- do ----

        body.Children().Append(RemoteText(resources::GetString(L"RemoteDoHeading"), 11.0, L"TextFillColorTertiaryBrush"));

        controls::StackPanel what{};
        what.Orientation(controls::Orientation::Horizontal);
        what.Spacing(8.0);

        if (rule.Action == glass::RemoteActionKind::Unrecognized)
        {
            body.Children().Append(RemoteText(resources::GetString(L"RemoteActionUnrecognized"), 12.0, L"SystemFillColorCautionBrush"));
            card.Child(body);
            return card;
        }

        {
            std::vector<winrt::hstring> actions{};
            int32_t selected{ 0 };

            for (int32_t at = 0; at < static_cast<int32_t>(std::size(ActionChoices)); ++at)
            {
                actions.push_back(resources::GetString(ActionChoices[at].NameKey));

                if (ActionChoices[at].Action == rule.Action)
                {
                    selected = at;
                }
            }

            auto combo = MakeCombo(L"RemoteActionHeader", actions, selected, 230.0);

            combo.SelectionChanged([change](auto&& sender, auto&&)
                {
                    auto const at = sender.template as<controls::ComboBox>().SelectedIndex();

                    if (at >= 0 && at < static_cast<int32_t>(std::size(ActionChoices)))
                    {
                        auto const action = ActionChoices[at].Action;
                        change([action](glass::RemoteRule& edited) { edited.Action = action; }, true);
                    }
                });

            what.Children().Append(combo);
        }

        wchar_t const* captionKey{ nullptr };

        switch (rule.Action)
        {
        case glass::RemoteActionKind::GoToPage:
        {
            std::vector<winrt::hstring> pages{};
            int32_t selected{ -1 };

            for (size_t at = 0; at < document.Pages.size(); ++at)
            {
                pages.push_back(winrt::hstring{ document.Pages[at].Name });

                if (document.Pages[at].Id == rule.TargetPageId)
                {
                    selected = static_cast<int32_t>(at);
                }
            }

            auto combo = MakeCombo(L"RemotePageHeader", pages, selected, 180.0);
            std::vector<std::wstring> ids{};

            for (auto const& page : document.Pages)
            {
                ids.push_back(page.Id);
            }

            combo.SelectionChanged([change, ids](auto&& sender, auto&&)
                {
                    auto const at = sender.template as<controls::ComboBox>().SelectedIndex();

                    if (at >= 0 && static_cast<size_t>(at) < ids.size())
                    {
                        auto const id = ids[static_cast<size_t>(at)];
                        change([id](glass::RemoteRule& edited) { edited.TargetPageId = id; }, false);
                    }
                });

            what.Children().Append(combo);
            break;
        }

        case glass::RemoteActionKind::NextPage:
            captionKey = L"RemoteCaptionNextPage";
            break;

        case glass::RemoteActionKind::PreviousPage:
            captionKey = L"RemoteCaptionPreviousPage";
            break;

        case glass::RemoteActionKind::PageFromMessage:
            captionKey = L"RemoteCaptionPageFromMessage";
            break;

        case glass::RemoteActionKind::PadGrid:
        {
            std::vector<winrt::hstring> functions{};

            for (auto const* const key : PadFunctionKeys)
            {
                functions.push_back(resources::GetString(key));
            }

            auto combo = MakeCombo(L"RemoteFunctionHeader", functions, static_cast<int32_t>(rule.PadFunction), 180.0);

            combo.SelectionChanged([change](auto&& sender, auto&&)
                {
                    auto const at = sender.template as<controls::ComboBox>().SelectedIndex();

                    if (at >= 0 && at < glass::PadGridFunctionCount)
                    {
                        auto const function = static_cast<glass::PadGridFunction>(at);
                        change([function](glass::RemoteRule& edited) { edited.PadFunction = function; }, false);
                    }
                });

            what.Children().Append(combo);
            break;
        }

        case glass::RemoteActionKind::SetKeyRoot:
        {
            std::vector<winrt::hstring> roots{};

            for (int32_t root = 0; root < 12; ++root)
            {
                roots.push_back(winrt::hstring{ glass::RootPickerName(root) });
            }

            auto combo = MakeCombo(L"RemoteRootHeader", roots, rule.KeyRoot, 90.0);
            combo.IsEnabled(!rule.FromMessage);

            combo.SelectionChanged([change](auto&& sender, auto&&)
                {
                    auto const at = sender.template as<controls::ComboBox>().SelectedIndex();

                    if (at >= 0 && at < 12)
                    {
                        change([at](glass::RemoteRule& edited) { edited.KeyRoot = at; }, false);
                    }
                });

            what.Children().Append(combo);

            if (rule.FromMessage)
            {
                captionKey = L"RemoteCaptionRootFromMessage";
            }

            break;
        }

        case glass::RemoteActionKind::SetScale:
        {
            std::vector<winrt::hstring> scales{};

            for (int32_t scale = 0; scale <= static_cast<int32_t>(glass::MusicalScale::WholeTone); ++scale)
            {
                scales.push_back(resources::GetString(glass::ScaleNameResourceKey(static_cast<glass::MusicalScale>(scale))));
            }

            auto combo = MakeCombo(L"RemoteScaleHeader", scales, static_cast<int32_t>(rule.Scale), 160.0);
            combo.IsEnabled(!rule.FromMessage);

            combo.SelectionChanged([change](auto&& sender, auto&&)
                {
                    auto const at = sender.template as<controls::ComboBox>().SelectedIndex();

                    if (at >= 0 && at <= static_cast<int32_t>(glass::MusicalScale::WholeTone))
                    {
                        auto const scale = static_cast<glass::MusicalScale>(at);
                        change([scale](glass::RemoteRule& edited) { edited.Scale = scale; }, false);
                    }
                });

            what.Children().Append(combo);

            if (rule.FromMessage)
            {
                captionKey = L"RemoteCaptionScaleFromMessage";
            }

            break;
        }

        case glass::RemoteActionKind::RunSequence:
        {
            std::vector<winrt::hstring> sequences{};
            std::vector<std::wstring> names{};
            int32_t selected{ -1 };

            for (auto const& sequence : document.Sequences)
            {
                if (sequence.Name == rule.SequenceName)
                {
                    selected = static_cast<int32_t>(names.size());
                }

                sequences.push_back(winrt::hstring{ sequence.Name });
                names.push_back(sequence.Name);
            }

            auto combo = MakeCombo(L"RemoteSequenceHeader", sequences, selected, 180.0);

            combo.SelectionChanged([change, names](auto&& sender, auto&&)
                {
                    auto const at = sender.template as<controls::ComboBox>().SelectedIndex();

                    if (at >= 0 && static_cast<size_t>(at) < names.size())
                    {
                        auto const name = names[static_cast<size_t>(at)];
                        change([name](glass::RemoteRule& edited) { edited.SequenceName = name; }, false);
                    }
                });

            what.Children().Append(combo);

            if (document.Sequences.empty())
            {
                captionKey = L"RemoteCaptionNoSequences";
            }

            break;
        }

        case glass::RemoteActionKind::Panic:
            captionKey = L"RemoteCaptionPanic";
            break;

        case glass::RemoteActionKind::PressControl:
            captionKey = L"RemoteCaptionPress";
            break;

        case glass::RemoteActionKind::ShowOnlyThis:
            captionKey = L"RemoteCaptionShowOnly";
            break;

        default:
            break;
        }

        if (UsesTarget(rule.Action))
        {
            auto const choices = TargetChoices(document, rule.Action == glass::RemoteActionKind::ShowOnlyThis);
            std::vector<winrt::hstring> names{};
            int32_t selected{ -1 };

            for (size_t at = 0; at < choices.size(); ++at)
            {
                names.push_back(winrt::hstring{ choices[at].Name });

                auto const matches = choices[at].IsGroup
                    ? (!rule.TargetGroupId.empty() && choices[at].Id == rule.TargetGroupId)
                    : (rule.TargetGroupId.empty() && choices[at].Id == rule.TargetControlId);

                if (matches)
                {
                    selected = static_cast<int32_t>(at);
                }
            }

            auto combo = MakeCombo(L"RemoteTargetHeader", names, selected, 220.0);

            combo.SelectionChanged([change, choices](auto&& sender, auto&&)
                {
                    auto const at = sender.template as<controls::ComboBox>().SelectedIndex();

                    if (at >= 0 && static_cast<size_t>(at) < choices.size())
                    {
                        auto const choice = choices[static_cast<size_t>(at)];

                        change([choice](glass::RemoteRule& edited)
                            {
                                edited.TargetControlId = choice.IsGroup ? std::wstring{} : choice.Id;
                                edited.TargetGroupId = choice.IsGroup ? choice.Id : std::wstring{};
                            }, false);
                    }
                });

            what.Children().Append(combo);
        }

        if (rule.Action == glass::RemoteActionKind::SetControlValue)
        {
            controls::Slider value{};
            value.Header(box_value(resources::GetString(L"RemoteValueHeader")));
            value.Minimum(0.0);
            value.Maximum(100.0);
            value.StepFrequency(1.0);
            value.Width(150.0);
            value.Value(rule.Value * 100.0);
            value.IsEnabled(!rule.FromMessage);

            value.ValueChanged([change](auto&&, controls::Primitives::RangeBaseValueChangedEventArgs const& args)
                {
                    auto const share = std::clamp(args.NewValue() / 100.0, 0.0, 1.0);
                    change([share](glass::RemoteRule& edited) { edited.Value = share; }, false);
                });

            what.Children().Append(value);
        }

        // Pad grid changes, roots and scales can be aimed at one grid or all of them.
        if (rule.Action == glass::RemoteActionKind::PadGrid ||
            rule.Action == glass::RemoteActionKind::SetKeyRoot ||
            rule.Action == glass::RemoteActionKind::SetScale)
        {
            std::vector<winrt::hstring> grids{ resources::GetString(L"RemoteAllGrids") };
            std::vector<std::wstring> ids{ std::wstring{} };
            int32_t selected{ rule.PadTargets.AllGrids ? 0 : -1 };

            for (auto const& page : document.Pages)
            {
                for (auto const& control : page.Controls)
                {
                    if ((!glass::IsPadGrid(control.Kind) && !glass::IsFretboard(control.Kind)) || control.Id.empty())
                    {
                        continue;
                    }

                    if (!rule.PadTargets.AllGrids && rule.PadTargets.ControlIds.size() == 1 &&
                        rule.PadTargets.ControlIds.front() == control.Id)
                    {
                        selected = static_cast<int32_t>(ids.size());
                    }

                    grids.push_back(resources::FormatString(
                        L"RemoteControlOnPageFormat",
                        control.Label.empty() ? control.Id : control.Label,
                        page.Name));
                    ids.push_back(control.Id);
                }
            }

            auto combo = MakeCombo(L"RemoteGridsHeader", grids, selected, 180.0);

            combo.SelectionChanged([change, ids](auto&& sender, auto&&)
                {
                    auto const at = sender.template as<controls::ComboBox>().SelectedIndex();

                    if (at < 0 || static_cast<size_t>(at) >= ids.size())
                    {
                        return;
                    }

                    auto const id = ids[static_cast<size_t>(at)];

                    change([id](glass::RemoteRule& edited)
                        {
                            edited.PadTargets.AllGrids = id.empty();
                            edited.PadTargets.ControlIds.clear();

                            if (!id.empty())
                            {
                                edited.PadTargets.ControlIds.push_back(id);
                            }
                        }, false);
                });

            what.Children().Append(combo);
        }

        body.Children().Append(what);

        if (rule.Action == glass::RemoteActionKind::SetControlValue ||
            rule.Action == glass::RemoteActionKind::SetKeyRoot ||
            rule.Action == glass::RemoteActionKind::SetScale)
        {
            controls::CheckBox fromMessage{};
            fromMessage.Content(box_value(resources::GetString(L"RemoteFromMessage")));
            fromMessage.IsChecked(rule.FromMessage);
            fromMessage.Click([change](auto&& sender, auto&&)
                {
                    auto const ticked = IsTicked(sender.template as<controls::Primitives::ToggleButton>());
                    change([ticked](glass::RemoteRule& edited) { edited.FromMessage = ticked; }, true);
                });

            body.Children().Append(fromMessage);
        }

        if (captionKey != nullptr)
        {
            body.Children().Append(RemoteText(resources::GetString(captionKey), 11.0, L"TextFillColorTertiaryBrush"));
        }

        card.Child(body);

        return card;
    }

    void EditorWindow::AddTellPageSection()
    {
        auto panel = RemoteControlPanel();
        auto const& document = m_editor.Document();
        auto const tell = document.Remote.TellPage;
        auto weak = get_weak();

        auto const change = [weak](std::function<void(glass::PageFeedback&)> edit, bool rebuild)
            {
                if (auto strong = weak.get(); strong != nullptr && !strong->m_updatingSettings)
                {
                    strong->EditRemoteControl([edit](glass::RemoteControl& remote) { edit(remote.TellPage); }, rebuild);
                }
            };

        auto heading = RemoteText(resources::GetString(L"RemoteTellHeading"), 13.0, L"TextFillColorPrimaryBrush");
        heading.FontWeight(winrt::Windows::UI::Text::FontWeights::SemiBold());
        heading.Margin(xaml::ThicknessHelper::FromLengths(0.0, 18.0, 0.0, 4.0));
        automation::AutomationProperties::SetHeadingLevel(heading, automation::Peers::AutomationHeadingLevel::Level3);
        panel.Children().Append(heading);

        panel.Children().Append(RemoteText(resources::GetString(L"RemoteTellCaption"), 12.0, L"TextFillColorSecondaryBrush"));

        controls::ToggleSwitch enabled{};
        enabled.Header(box_value(resources::GetString(L"RemoteTellSwitch")));
        enabled.IsOn(tell.Enabled);
        enabled.Margin(xaml::ThicknessHelper::FromLengths(0.0, 6.0, 0.0, 0.0));
        enabled.Toggled([change](auto&& sender, auto&&)
            {
                auto const on = sender.template as<controls::ToggleSwitch>().IsOn();
                change([on](glass::PageFeedback& edited) { edited.Enabled = on; }, true);
            });
        panel.Children().Append(enabled);

        controls::StackPanel fields{};
        fields.Orientation(controls::Orientation::Horizontal);
        fields.Spacing(8.0);
        fields.IsHitTestVisible(tell.Enabled);
        fields.Opacity(tell.Enabled ? 1.0 : 0.5);

        {
            std::vector<winrt::hstring> devices{};
            std::vector<std::wstring> names{};
            int32_t selected{ -1 };

            for (auto const& device : document.Devices)
            {
                if (device.Name == tell.DeviceName)
                {
                    selected = static_cast<int32_t>(names.size());
                }

                devices.push_back(winrt::hstring{ device.Name });
                names.push_back(device.Name);
            }

            auto combo = MakeCombo(L"RemoteTellDeviceHeader", devices, selected, 180.0);
            combo.IsEnabled(tell.Enabled);

            combo.SelectionChanged([change, names](auto&& sender, auto&&)
                {
                    auto const at = sender.template as<controls::ComboBox>().SelectedIndex();

                    if (at >= 0 && static_cast<size_t>(at) < names.size())
                    {
                        auto const name = names[static_cast<size_t>(at)];
                        change([name](glass::PageFeedback& edited) { edited.DeviceName = name; }, false);
                    }
                });

            fields.Children().Append(combo);
        }

        {
            auto combo = MakeCombo(
                L"RemoteTellKindHeader",
                { resources::GetString(L"RemoteKindProgramChange"), resources::GetString(L"RemoteKindControlChange") },
                tell.Kind == glass::MessageKind::ControlChange ? 1 : 0,
                150.0);
            combo.IsEnabled(tell.Enabled);

            combo.SelectionChanged([change](auto&& sender, auto&&)
                {
                    auto const at = sender.template as<controls::ComboBox>().SelectedIndex();

                    if (at >= 0)
                    {
                        auto const kind = at == 1 ? glass::MessageKind::ControlChange : glass::MessageKind::ProgramChange;
                        change([kind](glass::PageFeedback& edited) { edited.Kind = kind; }, true);
                    }
                });

            fields.Children().Append(combo);
        }

        {
            auto combo = MakeCombo(L"RemoteChannelHeader", ChannelItems(false), tell.ChannelIndex, 90.0);
            combo.IsEnabled(tell.Enabled);

            combo.SelectionChanged([change](auto&& sender, auto&&)
                {
                    auto const at = sender.template as<controls::ComboBox>().SelectedIndex();

                    if (at >= 0 && at < 16)
                    {
                        change([at](glass::PageFeedback& edited) { edited.ChannelIndex = at; }, false);
                    }
                });

            fields.Children().Append(combo);
        }

        if (tell.Kind == glass::MessageKind::ControlChange)
        {
            auto box = MakeNumber(L"RemoteNumberController", tell.Number, 0.0, 127.0);
            box.IsEnabled(tell.Enabled);

            box.ValueChanged([change](controls::NumberBox const&, controls::NumberBoxValueChangedEventArgs const& args)
                {
                    if (std::isfinite(args.NewValue()))
                    {
                        auto const number = std::clamp(static_cast<int32_t>(std::lround(args.NewValue())), 0, 127);
                        change([number](glass::PageFeedback& edited) { edited.Number = number; }, false);
                    }
                });

            fields.Children().Append(box);
        }

        {
            auto box = MakeNumber(L"RemoteTellFirstHeader", tell.FirstValue, 0.0, 127.0);
            box.IsEnabled(tell.Enabled);

            box.ValueChanged([change](controls::NumberBox const&, controls::NumberBoxValueChangedEventArgs const& args)
                {
                    if (std::isfinite(args.NewValue()))
                    {
                        auto const first = std::clamp(static_cast<int32_t>(std::lround(args.NewValue())), 0, 127);
                        change([first](glass::PageFeedback& edited) { edited.FirstValue = first; }, false);
                    }
                });

            fields.Children().Append(box);
        }

        fields.Margin(xaml::ThicknessHelper::FromLengths(0.0, 4.0, 0.0, 0.0));
        panel.Children().Append(fields);
    }

    _Use_decl_annotations_
    void EditorWindow::EditRemoteControl(std::function<void(glass::RemoteControl&)> const& change, bool rebuild)
    {
        try
        {
            auto remote = m_editor.Document().Remote;

            change(remote);

            if (!m_editor.SetRemoteControl(remote))
            {
                return;
            }

            MarkChanged();

            // Outside Try mode the player still listens for the rules pane and for Learn.
            if (!m_tryMode && m_player != nullptr)
            {
                m_player->UpdateRemoteRules(m_editor.Document());
            }

            // A rule that moved or went away leaves Learn pointing at the wrong one.
            if (rebuild && m_remoteLearnRule >= 0)
            {
                SetRemoteLearnRule(-1);
            }

            // The card that asked is inside the list being rebuilt, so the rebuild waits for its
            // handler to return.
            if (rebuild && m_dispatcher != nullptr)
            {
                m_dispatcher.TryEnqueue([weak = get_weak()]()
                    {
                        if (auto strong = weak.get())
                        {
                            strong->RefreshRemoteControlPane();
                        }
                    });
            }
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to change a MIDI remote control rule.")
    }

    // ---------------------------------------------------------------- hearing and learning

    _Use_decl_annotations_
    void EditorWindow::OnRemoteRuleHeard(uint32_t ruleIndex)
    {
        try
        {
            if (ruleIndex >= m_remoteRuleCards.size() || ruleIndex >= m_remoteFlashUntil.size() ||
                RemoteControlPane().Visibility() != xaml::Visibility::Visible ||
                LayoutSettingsPanel().Visibility() != xaml::Visibility::Visible)
            {
                return;
            }

            m_remoteRuleCards[ruleIndex].Background(RemoteBrush(L"AccentFillColorTertiaryBrush"));
            m_remoteFlashUntil[ruleIndex] = static_cast<uint64_t>(::GetTickCount64()) + RemoteFlashMilliseconds;

            if (m_remoteFlashTimer == nullptr && m_dispatcher != nullptr)
            {
                m_remoteFlashTimer = m_dispatcher.CreateTimer();
                m_remoteFlashTimer.Interval(std::chrono::milliseconds{ 60 });
                m_remoteFlashTimer.Tick([weak = get_weak()](auto&&, auto&&)
                    {
                        if (auto strong = weak.get())
                        {
                            strong->SweepRemoteFlashes();
                        }
                    });
            }

            if (m_remoteFlashTimer != nullptr && !m_remoteFlashTimer.IsRunning())
            {
                m_remoteFlashTimer.Start();
            }
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to light a MIDI remote control rule.")
    }

    void EditorWindow::SweepRemoteFlashes()
    {
        try
        {
            auto const now = static_cast<uint64_t>(::GetTickCount64());
            auto lit = false;

            for (size_t at = 0; at < m_remoteFlashUntil.size() && at < m_remoteRuleCards.size(); ++at)
            {
                if (m_remoteFlashUntil[at] == 0)
                {
                    continue;
                }

                if (now >= m_remoteFlashUntil[at])
                {
                    m_remoteFlashUntil[at] = 0;
                    m_remoteRuleCards[at].Background(RemoteBrush(L"SubtleFillColorSecondaryBrush"));
                }
                else
                {
                    lit = true;
                }
            }

            if (!lit && m_remoteFlashTimer != nullptr)
            {
                m_remoteFlashTimer.Stop();
            }
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to put a MIDI remote control rule out.")
    }

    _Use_decl_annotations_
    void EditorWindow::SetRemoteLearnRule(int32_t ruleIndex)
    {
        try
        {
            if (m_remoteLearnRule == ruleIndex)
            {
                return;
            }

            m_remoteLearnRule = ruleIndex;

            if (ruleIndex >= 0)
            {
                if (m_player == nullptr)
                {
                    StartPlayerForLearning();
                }

                if (m_player != nullptr)
                {
                    m_player->SetLearning(true);
                }
            }
            else if (m_player != nullptr && m_learnMode == LearnMode::Off)
            {
                m_player->SetLearning(false);
            }

            // Only the waiting line and the toggles change, but they are on every card.
            if (m_dispatcher != nullptr)
            {
                m_dispatcher.TryEnqueue([weak = get_weak()]()
                    {
                        if (auto strong = weak.get())
                        {
                            strong->RefreshRemoteControlPane();
                        }
                    });
            }
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to start learning a MIDI remote control rule.")
    }

    _Use_decl_annotations_
    bool EditorWindow::TryApplyRemoteLearn(glass::LearnedBinding const& learned)
    {
        if (m_remoteLearnRule < 0)
        {
            return false;
        }

        try
        {
            // Only what a rule can listen for. Anything else keeps it waiting.
            if (learned.Kind != glass::MessageKind::ProgramChange &&
                learned.Kind != glass::MessageKind::ControlChange &&
                learned.Kind != glass::MessageKind::Note)
            {
                return true;
            }

            auto const ruleIndex = static_cast<size_t>(m_remoteLearnRule);

            EditRemoteControl([ruleIndex, learned](glass::RemoteControl& remote)
                {
                    if (ruleIndex >= remote.Rules.size())
                    {
                        return;
                    }

                    auto& when = remote.Rules[ruleIndex].When;

                    // A device not in the layout's table can't be named, and an empty name would
                    // quietly mean any device. It stays what it was.
                    if (!learned.DeviceName.empty())
                    {
                        when.DeviceName = learned.DeviceName;
                    }

                    when.Kind = learned.Kind;
                    when.GroupIndex = learned.GroupIndex;
                    when.ChannelIndex = learned.ChannelIndex;
                    when.Number = static_cast<int32_t>(std::min<uint32_t>(learned.Number, 127));
                }, false);

            SetRemoteLearnRule(-1);
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to use what arrived for a MIDI remote control rule.")

        return true;
    }

    // ---------------------------------------------------------------- the surface in Try mode

    void EditorWindow::SyncRemoteSurface()
    {
        try
        {
            if (m_player != nullptr)
            {
                m_player->PageShown(m_editor.PageIndex());
            }

            if (!m_tryMode || m_player == nullptr)
            {
                // Edit mode draws everything, because a control nobody can see is a control
                // nobody can edit.
                if (m_surfaceStatesShown)
                {
                    for (size_t item = 0; item < m_renderer.ItemCount(); ++item)
                    {
                        m_renderer.SetControlState(item, true, true);
                    }

                    m_surfaceStatesShown = false;
                }

                return;
            }

            auto const& states = m_player->ControlStates();

            for (size_t item = 0; item < m_renderer.ItemCount(); ++item)
            {
                auto const controlIndex = m_renderer.ControlIndexOf(item);

                if (states.TakesInput(controlIndex))
                {
                    continue;
                }

                m_renderer.SetControlState(item, states.IsEnabled(controlIndex), states.IsShown(controlIndex));

                if (auto const element = m_renderer.ElementAt(item))
                {
                    element.IsHitTestVisible(false);
                }

                m_surfaceStatesShown = true;
            }
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to show which controls are turned off.")
    }

    _Use_decl_annotations_
    void EditorWindow::OnTryControlStatesChanged(std::vector<uint32_t> const& controlIndexes)
    {
        if (!m_tryMode || m_player == nullptr)
        {
            return;
        }

        try
        {
            auto const& states = m_player->ControlStates();

            for (auto const controlIndex : controlIndexes)
            {
                size_t item{ 0 };

                if (!m_renderer.TryFindItem(controlIndex, item))
                {
                    continue;
                }

                auto const takes = states.TakesInput(controlIndex);

                // Let go first, while it still takes input, so a held note ends.
                if (!takes)
                {
                    m_input.ReleaseItem(item);
                }

                m_renderer.SetControlState(item, states.IsEnabled(controlIndex), states.IsShown(controlIndex));

                if (auto const element = m_renderer.ElementAt(item))
                {
                    element.IsHitTestVisible(takes);
                }

                m_surfaceStatesShown = true;
            }
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to show a control a rule turned off.")
    }

    void EditorWindow::ForgetTryControlStates()
    {
        if (m_player != nullptr)
        {
            m_player->ForgetControlStates();
        }
    }

    // ---------------------------------------------------------------- start states and shortcuts

    _Use_decl_annotations_
    void EditorWindow::OnStartsDisabledToggled(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        if (m_updatingInspector)
        {
            return;
        }

        auto const* const control = SingleSelectedControl();

        if (control != nullptr && m_editor.SetControlStartsDisabled(control->Id, StartsDisabledSwitch().IsOn()))
        {
            MarkChanged();
        }
    }

    _Use_decl_annotations_
    void EditorWindow::OnStartsHiddenToggled(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        if (m_updatingInspector)
        {
            return;
        }

        auto const* const control = SingleSelectedControl();

        if (control != nullptr && m_editor.SetControlStartsHidden(control->Id, StartsHiddenSwitch().IsOn()))
        {
            MarkChanged();
        }
    }

    _Use_decl_annotations_
    void EditorWindow::OnListenRemoteRuleClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        try
        {
            auto const* const control = SingleSelectedControl();

            if (control == nullptr)
            {
                return;
            }

            auto const index = m_editor.AddRemoteRuleFor(control->Id);

            if (index < 0)
            {
                return;
            }

            MarkChanged();

            // Straight to the new rule, which still needs its message.
            m_remoteBringIntoView = index;
            m_settingsPane = L"remote";
            ShowLayoutSettings(true);
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to add a MIDI remote control rule for this control.")
    }

    _Use_decl_annotations_
    void EditorWindow::AppendPageOpenMenu(controls::MenuFlyout const& menu, size_t pageIndex)
    {
        auto const& document = m_editor.Document();

        if (pageIndex >= document.Pages.size())
        {
            return;
        }

        auto const current = document.Pages[pageIndex].OpenSequenceName;

        controls::MenuFlyoutSubItem opens{};
        opens.Text(resources::GetString(L"PageMenuOpenSequence"));

        auto const add = [&](winrt::hstring const& text, std::wstring const& name)
            {
                controls::RadioMenuFlyoutItem item{};
                item.Text(text);
                item.GroupName(L"PageOpenSequence");
                item.IsChecked(name == current);
                item.Click([weak = get_weak(), pageIndex, name](auto&&, auto&&)
                    {
                        if (auto strong = weak.get())
                        {
                            strong->ApplyPageEdit([pageIndex, name](glass::EditorController& editor)
                                { return editor.SetPageOpenSequence(pageIndex, name); });
                        }
                    });
                opens.Items().Append(item);
            };

        add(resources::GetString(L"PageMenuOpenNothing"), std::wstring{});

        for (auto const& sequence : document.Sequences)
        {
            add(winrt::hstring{ sequence.Name }, sequence.Name);
        }

        menu.Items().Append(opens);
    }
}
