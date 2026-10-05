// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MainWindow.xaml.h"

#include "StringResources.h"
#include "ThemeBrushes.h"

using namespace winrt::Microsoft::UI::Xaml;

namespace patchbay = ::midipatchbay;
namespace resources = ::midipatchbay::resources;

namespace winrt::midipatchbay::implementation
{
    namespace
    {
        media::Brush BrushOrNull(_In_ std::wstring_view key) noexcept
        {
            return patchbay::ThemeBrushes::Current().Get(key);
        }

        controls::TextBlock SectionLabel(_In_ winrt::hstring const& text) noexcept
        {
            controls::TextBlock block{};

            block.Text(text);
            block.FontSize(12);
            block.Foreground(BrushOrNull(L"TextFillColorTertiaryBrush"));

            return block;
        }

        controls::TextBlock ValueText(
            _In_ winrt::hstring const& text,
            _In_ double size = 13,
            _In_ bool wrap = false) noexcept
        {
            controls::TextBlock block{};

            block.Text(text);
            block.FontSize(size);

            if (wrap)
            {
                block.TextWrapping(xaml::TextWrapping::Wrap);
            }
            else
            {
                block.TextTrimming(xaml::TextTrimming::CharacterEllipsis);
            }

            return block;
        }

        controls::Border Card(_In_ xaml::UIElement const& content) noexcept
        {
            controls::Border border{};

            border.CornerRadius(xaml::CornerRadiusHelper::FromUniformRadius(6));
            border.Padding(xaml::ThicknessHelper::FromUniformLength(12));
            border.BorderThickness(xaml::ThicknessHelper::FromUniformLength(1));
            border.BorderBrush(BrushOrNull(L"CardStrokeColorDefaultBrush"));
            border.Background(BrushOrNull(L"CardBackgroundFillColorSecondaryBrush"));
            border.Child(content);

            return border;
        }

        controls::StackPanel Section(_In_ winrt::hstring const& label) noexcept
        {
            controls::StackPanel panel{};

            panel.Spacing(6);
            panel.Children().Append(SectionLabel(label));

            return panel;
        }

        // A toolbar-sized row of buttons, which is what every inspector ends with.
        controls::StackPanel ActionRow() noexcept
        {
            controls::StackPanel actions{};

            actions.Spacing(8);
            actions.Orientation(controls::Orientation::Horizontal);

            return actions;
        }
    }

    void MainWindow::RefreshInspector() noexcept
    {
        try
        {
            if (m_closing)
            {
                return;
            }

            InspectorContent().Children().Clear();

            m_activityText = nullptr;
            m_activityElementId.clear();

            auto* patch = CurrentPatch();
            auto const kind = m_canvas.SelectionKind();

            if (patch == nullptr || kind == patchbay::CanvasSelectionKind::None)
            {
                InspectorPanel().Visibility(xaml::Visibility::Collapsed);
                return;
            }

            InspectorPanel().Visibility(xaml::Visibility::Visible);

            // Several nodes at once: only what can be done to all of them.
            if (auto const count = m_canvas.SelectedNodeIds().size(); count > 1)
            {
                InspectorTitle().Text(resources::FormatString(L"InspectorSelectionTitleFormat", count));
                BuildSelectionInspector(count);
                return;
            }

            if (kind == patchbay::CanvasSelectionKind::Endpoint)
            {
                if (auto const* endpoint = patch->FindEndpoint(m_canvas.SelectedNodeId()))
                {
                    InspectorTitle().Text(winrt::hstring{ endpoint->DisplayName });
                    BuildEndpointInspector(*endpoint);
                    return;
                }
            }
            else if (kind == patchbay::CanvasSelectionKind::Block)
            {
                if (auto const* block = patch->FindBlock(m_canvas.SelectedNodeId()))
                {
                    InspectorTitle().Text(patchbay::BlockDisplayName(*block));
                    BuildBlockInspector(*block);
                    return;
                }
            }
            else if (auto const* connection = patch->FindConnection(m_canvas.SelectedConnectionId()))
            {
                InspectorTitle().Text(resources::GetString(L"InspectorConnectionTitle"));
                BuildConnectionInspector(*connection);
                return;
            }

            InspectorPanel().Visibility(xaml::Visibility::Collapsed);
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to build the inspector.")
    }

    _Use_decl_annotations_
    winrt::hstring MainWindow::ActivityText(std::wstring const& elementId) noexcept
    {
        try
        {
            auto const* patch = CurrentPatch();

            if (patch == nullptr)
            {
                return {};
            }

            if (!patchbay::PatchLibrary::Current().IsRouting(m_patchKey))
            {
                return resources::GetString(L"ActivityNotRouting");
            }

            auto const found = m_activity.find(elementId);

            if (auto const* block = patch->FindBlock(elementId))
            {
                auto const generator = patchbay::IsGenerator(block->Kind);

                if (block->Bypassed)
                {
                    return resources::GetString(generator ? L"ActivityGeneratorBypassed" : L"ActivityBypassed");
                }

                if (generator)
                {
                    auto const connected = std::any_of(patch->Connections.begin(), patch->Connections.end(),
                        [&elementId](patchbay::PatchConnection const& link) { return link.SourceId == elementId; });

                    if (!connected)
                    {
                        return resources::GetString(L"ActivityGeneratorNotConnected");
                    }

                    return found == m_activity.end()
                        ? resources::GetString(L"ActivityNothingSentYet")
                        : resources::FormatString(L"ActivitySentFormat", found->second.MessagesForwarded);
                }

                if (found == m_activity.end())
                {
                    return resources::GetString(L"ActivityNothingYet");
                }

                auto const& stats = found->second;

                if (patchbay::CategoryOf(block->Kind) == patchbay::BlockCategory::Filter)
                {
                    return resources::FormatString(L"ActivityFilterFormat", stats.MessagesForwarded, stats.MessagesKeptOut);
                }

                auto text = resources::FormatString(L"ActivityThroughFormat", stats.MessagesForwarded);

                if (stats.MessagesWaiting > 0)
                {
                    text = text + winrt::hstring{ L"\n" } +
                        resources::FormatString(L"ConnectionWaitingToSendFormat", stats.MessagesWaiting);
                }

                if (stats.MessagesDropped > 0)
                {
                    text = text + winrt::hstring{ L"\n" } +
                        resources::FormatString(L"ConnectionDroppedFormat", stats.MessagesDropped);
                }

                return text;
            }

            if (Analysis().LoopMutedConnectionIds.count(elementId) != 0)
            {
                return resources::GetString(L"ConnectionLoopMuted");
            }

            auto const* connection = patch->FindConnection(elementId);

            if (connection != nullptr && connection->Muted)
            {
                return resources::GetString(L"ConnectionMuted");
            }

            if (found == m_activity.end())
            {
                return resources::GetString(L"ConnectionWaiting");
            }

            auto text = resources::FormatString(L"ConnectionForwardedFormat", found->second.MessagesForwarded);

            if (found->second.SendFailures > 0)
            {
                text = text + winrt::hstring{ L"\n" } +
                    resources::FormatString(L"ConnectionSendFailuresFormat", found->second.SendFailures);
            }

            if (found->second.MessagesWaiting > 0)
            {
                text = text + winrt::hstring{ L"\n" } +
                    resources::FormatString(L"ConnectionWaitingToSendFormat", found->second.MessagesWaiting);
            }

            if (found->second.MessagesDropped > 0)
            {
                text = text + winrt::hstring{ L"\n" } +
                    resources::FormatString(L"ConnectionDroppedFormat", found->second.MessagesDropped);
            }

            return text;
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to describe the activity.")

        return {};
    }

    void MainWindow::UpdateInspectorActivity() noexcept
    {
        try
        {
            if (m_activityText == nullptr || m_activityElementId.empty())
            {
                return;
            }

            m_activityText.Text(ActivityText(m_activityElementId));
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to refresh the activity.")
    }

    _Use_decl_annotations_
    void MainWindow::BuildEndpointInspector(patchbay::PatchEndpoint const& endpoint) noexcept
    {
        try
        {
            auto const endpointId = endpoint.Id;
            auto weak = get_weak();

            auto const live = patchbay::ResolveEndpoint(endpoint);

            // ------------------------------------------------------- identity
            {
                controls::StackPanel body{};
                body.Spacing(4);

                body.Children().Append(ValueText(winrt::hstring{ endpoint.DisplayName }, 13, true));

                if (live.has_value())
                {
                    auto detail = live->ManufacturerName;

                    if (!live->TransportCode.empty())
                    {
                        detail = detail.empty() ? live->TransportCode : detail + L" \u00B7 " + live->TransportCode;
                    }

                    auto line = ValueText(winrt::hstring{ detail }, 11, true);
                    line.Foreground(BrushOrNull(L"TextFillColorTertiaryBrush"));
                    body.Children().Append(line);

                    // The catalog already prefers the customer's own description over the transport's.
                    if (!live->Description.empty())
                    {
                        auto description = ValueText(winrt::hstring{ live->Description }, 12, true);
                        description.Foreground(BrushOrNull(L"TextFillColorSecondaryBrush"));
                        description.Margin(xaml::ThicknessHelper::FromLengths(0, 4, 0, 0));
                        body.Children().Append(description);
                    }
                }
                else
                {
                    auto line = ValueText(resources::GetString(L"NodeNotConnected"), 11, true);
                    line.Foreground(BrushOrNull(L"SystemFillColorCriticalBrush"));
                    body.Children().Append(line);
                }

                InspectorContent().Children().Append(Card(body));
            }

            // -------------------------------------------------- match choices
            {
                auto section = Section(resources::GetString(L"InspectorIdentifyBy"));

                auto const addChoice = [&](patchbay::EndpointMatchMode mode,
                    winrt::hstring const& text, winrt::hstring const& hint, bool enabled)
                    {
                        controls::RadioButton button{};

                        button.Content(winrt::box_value(text));
                        button.IsChecked(endpoint.MatchMode == mode);
                        button.IsEnabled(enabled);
                        button.GroupName(L"InspectorMatchMode");

                        button.Checked([weak, endpointId, mode](auto&&, auto&&)
                            {
                                auto strong = weak.get();

                                if (strong == nullptr)
                                {
                                    return;
                                }

                                auto* current = strong->CurrentPatch();

                                if (current == nullptr)
                                {
                                    return;
                                }

                                auto* target = current->FindEndpoint(endpointId);

                                if (target == nullptr || target->MatchMode == mode)
                                {
                                    return;
                                }

                                target->MatchMode = mode;

                                strong->CommitChange(true);
                            });

                        section.Children().Append(button);

                        if (!hint.empty())
                        {
                            auto hintText = ValueText(hint, 11, true);
                            hintText.Margin(xaml::ThicknessHelper::FromLengths(30, -4, 0, 4));
                            hintText.Foreground(BrushOrNull(L"TextFillColorTertiaryBrush"));
                            section.Children().Append(hintText);
                        }
                    };

                addChoice(patchbay::EndpointMatchMode::EndpointDeviceId,
                    resources::GetString(L"MatchModeDeviceId"),
                    resources::GetString(L"MatchModeDeviceIdHint"),
                    true);

                addChoice(patchbay::EndpointMatchMode::UsbVendorAndProduct,
                    resources::GetString(L"MatchModeUsb"),
                    endpoint.Match.HasUsbIdentity()
                        ? resources::FormatString(L"MatchModeUsbHintFormat",
                            endpoint.Match.UsbVendorId, endpoint.Match.UsbProductId)
                        : resources::GetString(L"MatchModeUsbUnavailable"),
                    endpoint.Match.HasUsbIdentity());

                addChoice(patchbay::EndpointMatchMode::EndpointName,
                    resources::GetString(L"MatchModeName"),
                    resources::GetString(L"MatchModeNameHint"),
                    true);

                InspectorContent().Children().Append(section);
            }

            // --------------------------------------------------- replacement
            if (!live.has_value())
            {
                auto const suggestion = patchbay::SuggestReplacementFor(endpoint);

                if (suggestion.has_value())
                {
                    controls::StackPanel body{};
                    body.Spacing(8);

                    auto title = ValueText(resources::GetString(L"ReplacementTitle"), 12, true);
                    title.FontWeight(winrt::Microsoft::UI::Text::FontWeights::SemiBold());
                    body.Children().Append(title);

                    body.Children().Append(ValueText(
                        resources::FormatString(L"ReplacementMessageFormat", suggestion->Name), 12, true));

                    controls::Button useButton{};
                    useButton.Content(winrt::box_value(resources::GetString(L"ReplacementUse")));

                    auto const replacementId = suggestion->EndpointDeviceId;

                    useButton.Click([weak, endpointId, replacementId](auto&&, auto&&)
                        {
                            auto strong = weak.get();

                            if (strong == nullptr)
                            {
                                return;
                            }

                            auto const replacement = patchbay::EndpointCatalog::Current().Find(replacementId);

                            if (!replacement.has_value())
                            {
                                return;
                            }

                            auto* current = strong->CurrentPatch();

                            if (current == nullptr)
                            {
                                return;
                            }

                            auto* target = current->FindEndpoint(endpointId);

                            if (target == nullptr)
                            {
                                return;
                            }

                            target->Match = replacement->BuildMatch();
                            target->DisplayName = replacement->Name;
                            target->TransportCode = replacement->TransportCode;

                            strong->CommitChange(true);
                        });

                    body.Children().Append(useButton);

                    auto card = Card(body);
                    card.BorderBrush(BrushOrNull(L"AccentFillColorDefaultBrush"));
                    InspectorContent().Children().Append(card);
                }
            }

            // ------------------------------------------------------- actions
            {
                controls::StackPanel actions{};
                actions.Spacing(8);

                controls::Button groupsButton{};
                groupsButton.Content(winrt::box_value(endpoint.ShowAllGroups
                    ? resources::GetString(L"ActionShowDeclaredGroups")
                    : resources::GetString(L"ActionShowAllGroups")));

                groupsButton.Click([weak, endpointId](auto&&, auto&&)
                    {
                        auto strong = weak.get();

                        if (strong == nullptr)
                        {
                            return;
                        }

                        auto* current = strong->CurrentPatch();

                        if (current == nullptr)
                        {
                            return;
                        }

                        if (auto* target = current->FindEndpoint(endpointId))
                        {
                            target->ShowAllGroups = !target->ShowAllGroups;

                            // Only which points are drawn, so the routes stay as they are.
                            strong->CommitChange(false);
                        }
                    });

                actions.Children().Append(groupsButton);

                controls::Button removeButton{};
                removeButton.Content(winrt::box_value(resources::GetString(L"ActionRemoveEndpoint")));

                removeButton.Click([weak, endpointId](auto&&, auto&&)
                    {
                        auto strong = weak.get();

                        if (strong == nullptr)
                        {
                            return;
                        }

                        auto* current = strong->CurrentPatch();

                        if (current == nullptr)
                        {
                            return;
                        }

                        current->RemoveEndpoint(endpointId);

                        strong->m_canvas.ClearSelection();
                        strong->CommitChange(true);
                    });

                actions.Children().Append(removeButton);

                InspectorContent().Children().Append(actions);
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to build the endpoint inspector.")
    }

    _Use_decl_annotations_
    void MainWindow::BuildConnectionInspector(patchbay::PatchConnection const& connection) noexcept
    {
        try
        {
            auto* patch = CurrentPatch();

            if (patch == nullptr)
            {
                return;
            }

            auto const connectionId = connection.Id;
            auto weak = get_weak();

            // Only an endpoint has groups. A step takes whatever group each message carries.
            auto const* destination = patch->FindEndpoint(connection.DestinationId);
            auto const liveDestination = destination == nullptr ? std::nullopt : patchbay::ResolveEndpoint(*destination);

            // ----------------------------------------------------- from / to
            {
                controls::StackPanel body{};
                body.Spacing(3);

                auto const addEnd = [&](winrt::hstring const& label, winrt::hstring const& text)
                    {
                        auto labelText = ValueText(label, 11);
                        labelText.Foreground(BrushOrNull(L"TextFillColorTertiaryBrush"));
                        body.Children().Append(labelText);

                        auto value = ValueText(text, 13, true);
                        value.Margin(xaml::ThicknessHelper::FromLengths(0, 0, 0, 6));
                        body.Children().Append(value);
                    };

                addEnd(resources::GetString(L"InspectorFrom"),
                    DescribeLinkEnd(connection.SourceId, connection.SourceGroupIndex, true));
                addEnd(resources::GetString(L"InspectorTo"),
                    DescribeLinkEnd(connection.DestinationId, connection.DestinationGroupIndex, false));

                InspectorContent().Children().Append(Card(body));
            }

            // -------------------------------------------------- destination group
            if (destination != nullptr)
            {
                auto section = Section(resources::GetString(L"InspectorSendToGroup"));

                controls::ComboBox combo{};

                std::vector<int32_t> options{ patchbay::AllGroups };

                if (liveDestination.has_value())
                {
                    for (int32_t i = 0; i < patchbay::MaximumGroupCount; i++)
                    {
                        if (liveDestination->DeclaredGroups[static_cast<size_t>(i)])
                        {
                            options.push_back(i);
                        }
                    }
                }
                else
                {
                    for (int32_t i = 0; i < patchbay::MaximumGroupCount; i++)
                    {
                        options.push_back(i);
                    }
                }

                // a connection can point at a group the device no longer declares; keep it
                // selectable so the customer can see what it was set to
                if (connection.DestinationGroupIndex != patchbay::AllGroups &&
                    std::find(options.begin(), options.end(), connection.DestinationGroupIndex) == options.end())
                {
                    options.push_back(connection.DestinationGroupIndex);
                }

                auto items = winrt::single_threaded_vector<foundation::IInspectable>();

                int32_t selectedIndex{ 0 };

                for (size_t i = 0; i < options.size(); i++)
                {
                    std::wstring groupName{};

                    if (liveDestination.has_value() && options[i] != patchbay::AllGroups)
                    {
                        groupName = liveDestination->GroupName(options[i], false);
                    }

                    items.Append(winrt::box_value(options[i] == patchbay::AllGroups
                        ? resources::GetString(L"GroupSameAsSource")
                        : patchbay::DescribeGroupIndex(options[i], groupName)));

                    if (options[i] == connection.DestinationGroupIndex)
                    {
                        selectedIndex = static_cast<int32_t>(i);
                    }
                }

                combo.ItemsSource(items);
                combo.SelectedIndex(selectedIndex);
                combo.HorizontalAlignment(xaml::HorizontalAlignment::Stretch);

                combo.SelectionChanged([weak, connectionId, options](
                    foundation::IInspectable const& s, controls::SelectionChangedEventArgs const&)
                    {
                        auto strong = weak.get();

                        if (strong == nullptr)
                        {
                            return;
                        }

                        auto const control = s.try_as<controls::ComboBox>();

                        if (control == nullptr)
                        {
                            return;
                        }

                        auto const index = control.SelectedIndex();

                        // -1 happens while the list is being replaced; acting on it would
                        // rewrite the connection on every refresh
                        if (index < 0 || static_cast<size_t>(index) >= options.size())
                        {
                            return;
                        }

                        auto* current = strong->CurrentPatch();

                        if (current == nullptr)
                        {
                            return;
                        }

                        auto* target = current->FindConnection(connectionId);

                        if (target == nullptr || target->DestinationGroupIndex == options[index])
                        {
                            return;
                        }

                        target->DestinationGroupIndex = options[index];

                        strong->CommitChange(true);
                    });

                section.Children().Append(combo);
                InspectorContent().Children().Append(section);
            }

            // ------------------------------------------------------ add a step
            {
                controls::DropDownButton addStep{};

                addStep.Content(winrt::box_value(resources::GetString(L"ActionAddStepHere")));
                addStep.HorizontalAlignment(xaml::HorizontalAlignment::Stretch);
                addStep.Flyout(BuildAddStepMenu(connectionId));

                auto hint = ValueText(resources::GetString(L"ActionAddStepHereHint"), 11, true);
                hint.Foreground(BrushOrNull(L"TextFillColorTertiaryBrush"));

                controls::StackPanel body{};
                body.Spacing(6);
                body.Children().Append(addStep);
                body.Children().Append(hint);

                InspectorContent().Children().Append(body);
            }

            // ------------------------------------------------------- activity
            {
                auto section = Section(resources::GetString(L"InspectorActivity"));

                m_activityElementId = connectionId;
                m_activityText = ValueText(ActivityText(connectionId), 13, true);

                section.Children().Append(m_activityText);

                InspectorContent().Children().Append(section);
            }

            // -------------------------------------------------------- actions
            {
                auto actions = ActionRow();

                controls::Button muteButton{};
                muteButton.Content(winrt::box_value(connection.Muted
                    ? resources::GetString(L"ActionUnmute")
                    : resources::GetString(L"ActionMute")));

                muteButton.Click([weak, connectionId](auto&&, auto&&)
                    {
                        auto strong = weak.get();
                        auto* current = strong == nullptr ? nullptr : strong->CurrentPatch();

                        if (current == nullptr)
                        {
                            return;
                        }

                        if (auto* target = current->FindConnection(connectionId))
                        {
                            target->Muted = !target->Muted;
                            strong->CommitChange(true);
                        }
                    });

                actions.Children().Append(muteButton);

                controls::Button removeButton{};
                removeButton.Content(winrt::box_value(resources::GetString(L"ActionRemoveConnection")));

                removeButton.Click([weak, connectionId](auto&&, auto&&)
                    {
                        auto strong = weak.get();
                        auto* current = strong == nullptr ? nullptr : strong->CurrentPatch();

                        if (current == nullptr)
                        {
                            return;
                        }

                        current->RemoveConnection(connectionId);

                        strong->m_canvas.ClearSelection();
                        strong->CommitChange(true);
                    });

                actions.Children().Append(removeButton);

                InspectorContent().Children().Append(actions);
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to build the connection inspector.")
    }

    _Use_decl_annotations_
    void MainWindow::BuildBlockInspector(patchbay::PatchBlock const& block) noexcept
    {
        try
        {
            auto const blockId = block.Id;
            auto const kind = block.Kind;
            auto const category = patchbay::CategoryOf(kind);
            auto weak = get_weak();

            // ---------------------------------------------------- what it is
            {
                controls::StackPanel body{};
                body.Spacing(10);

                controls::StackPanel kindRow{};
                kindRow.Orientation(controls::Orientation::Horizontal);
                kindRow.Spacing(8);

                controls::Border badge{};
                badge.MinWidth(36);
                badge.Height(20);
                badge.Padding(xaml::ThicknessHelper::FromLengths(4, 0, 4, 0));
                badge.CornerRadius(xaml::CornerRadiusHelper::FromUniformRadius(4));
                badge.BorderThickness(xaml::ThicknessHelper::FromUniformLength(1));
                badge.BorderBrush(patchbay::PatchCanvas::CategoryBrush(category));
                badge.Background(patchbay::PatchCanvas::CategoryBrush(category, 0.18));

                controls::TextBlock badgeText{};
                badgeText.Text(patchbay::BlockKindBadge(kind));
                badgeText.FontSize(11);
                badgeText.FontWeight(winrt::Microsoft::UI::Text::FontWeights::SemiBold());
                badgeText.HorizontalAlignment(xaml::HorizontalAlignment::Center);
                badgeText.VerticalAlignment(xaml::VerticalAlignment::Center);
                badgeText.Foreground(patchbay::PatchCanvas::CategoryBrush(category));
                badge.Child(badgeText);

                kindRow.Children().Append(badge);

                auto kindText = ValueText(resources::FormatString(L"InspectorStepKindFormat",
                    patchbay::BlockKindName(kind), patchbay::BlockCategoryName(category)), 12);
                kindText.VerticalAlignment(xaml::VerticalAlignment::Center);
                kindText.Foreground(BrushOrNull(L"TextFillColorSecondaryBrush"));
                kindRow.Children().Append(kindText);

                body.Children().Append(kindRow);

                // Empty means the step goes by its kind's name, which the placeholder shows.
                controls::TextBox nameBox{};
                nameBox.Header(winrt::box_value(resources::GetString(L"InspectorStepName")));
                nameBox.PlaceholderText(patchbay::BlockKindName(kind));
                nameBox.Text(winrt::hstring{ block.Name });
                nameBox.MaxLength(96);

                auto const commitName = [weak, blockId](controls::TextBox const& box)
                    {
                        auto strong = weak.get();
                        auto* current = strong == nullptr ? nullptr : strong->CurrentPatch();
                        auto* target = current == nullptr ? nullptr : current->FindBlock(blockId);

                        if (target == nullptr)
                        {
                            return;
                        }

                        auto name = patchbay::SanitizeStoredString(std::wstring{ box.Text() });

                        // Typing the kind's own name is the same as giving it no name.
                        if (name == std::wstring{ patchbay::BlockKindName(target->Kind) })
                        {
                            name.clear();
                        }

                        if (name == target->Name)
                        {
                            return;
                        }

                        target->Name = name;

                        // The inspector stays as it is, so the box keeps its focus.
                        strong->CommitChange(true, false);
                        strong->RebuildCanvas();

                        if (auto const* renamed = current->FindBlock(blockId))
                        {
                            strong->InspectorTitle().Text(patchbay::BlockDisplayName(*renamed));
                        }
                    };

                nameBox.LostFocus([commitName](foundation::IInspectable const& sender, auto&&)
                    {
                        if (auto const box = sender.try_as<controls::TextBox>())
                        {
                            commitName(box);
                        }
                    });

                nameBox.KeyDown([commitName](foundation::IInspectable const& sender, input::KeyRoutedEventArgs const& args)
                    {
                        if (args.Key() != winrt::Windows::System::VirtualKey::Enter)
                        {
                            return;
                        }

                        args.Handled(true);

                        if (auto const box = sender.try_as<controls::TextBox>())
                        {
                            commitName(box);
                        }
                    });

                body.Children().Append(nameBox);

                InspectorContent().Children().Append(Card(body));
            }

            // ------------------------------------------------- what it does
            {
                auto section = Section(resources::GetString(L"InspectorWhatItDoes"));

                controls::StackPanel body{};
                body.Spacing(10);

                auto summary = ValueText(patchbay::DescribeBlock(kind, block.Settings), 12, true);
                body.Children().Append(summary);

                // The two settings people change most often are right here, not behind the dialog.
                if (kind == patchbay::BlockKind::Transpose)
                {
                    controls::NumberBox semitones{};

                    semitones.Header(winrt::box_value(resources::GetString(L"TransformSemitones")));
                    semitones.Minimum(patchbay::MinimumTranspose);
                    semitones.Maximum(patchbay::MaximumTranspose);
                    semitones.SmallChange(1);
                    semitones.LargeChange(12);
                    semitones.SpinButtonPlacementMode(controls::NumberBoxSpinButtonPlacementMode::Inline);
                    semitones.ValidationMode(controls::NumberBoxValidationMode::InvalidInputOverwritten);
                    semitones.Value(block.Settings.Transform.TransposeSemitones);

                    semitones.ValueChanged([weak, blockId, summary](auto&&, controls::NumberBoxValueChangedEventArgs const& args)
                        {
                            auto strong = weak.get();
                            auto* current = strong == nullptr ? nullptr : strong->CurrentPatch();
                            auto* target = current == nullptr ? nullptr : current->FindBlock(blockId);

                            if (target == nullptr || std::isnan(args.NewValue()))
                            {
                                return;
                            }

                            auto const value = static_cast<int32_t>(std::clamp(args.NewValue(),
                                static_cast<double>(patchbay::MinimumTranspose),
                                static_cast<double>(patchbay::MaximumTranspose)));

                            if (value == target->Settings.Transform.TransposeSemitones)
                            {
                                return;
                            }

                            target->Settings.Transform.TransposeSemitones = value;
                            summary.Text(patchbay::DescribeBlock(target->Kind, target->Settings));

                            strong->CommitChange(true, false);
                            strong->RebuildCanvas();
                        });

                    body.Children().Append(semitones);
                }
                else if (kind == patchbay::BlockKind::Throttle)
                {
                    // The speeds Network MIDI Setup offers, and whatever else a file asked for.
                    std::vector<uint32_t> options{ 0, 1, 2, 4, 8, 16, 32 };

                    if (std::find(options.begin(), options.end(), block.Settings.SendSpeedLimit) == options.end())
                    {
                        options.push_back(block.Settings.SendSpeedLimit);
                    }

                    auto items = winrt::single_threaded_vector<foundation::IInspectable>();
                    int32_t selectedIndex{ 0 };

                    for (size_t i = 0; i < options.size(); i++)
                    {
                        items.Append(winrt::box_value(patchbay::DescribeSendSpeed(options[i])));

                        if (options[i] == block.Settings.SendSpeedLimit)
                        {
                            selectedIndex = static_cast<int32_t>(i);
                        }
                    }

                    controls::ComboBox speed{};

                    speed.Header(winrt::box_value(resources::GetString(L"InspectorSendingSpeed")));
                    speed.ItemsSource(items);
                    speed.SelectedIndex(selectedIndex);
                    speed.HorizontalAlignment(xaml::HorizontalAlignment::Stretch);

                    speed.SelectionChanged([weak, blockId, options, summary](foundation::IInspectable const& s, auto&&)
                        {
                            auto strong = weak.get();
                            auto const control = s.try_as<controls::ComboBox>();
                            auto* current = strong == nullptr ? nullptr : strong->CurrentPatch();
                            auto* target = current == nullptr ? nullptr : current->FindBlock(blockId);

                            if (target == nullptr || control == nullptr)
                            {
                                return;
                            }

                            auto const index = control.SelectedIndex();

                            // -1 while the list is replaced. Acting on it would rewrite the step.
                            if (index < 0 || static_cast<size_t>(index) >= options.size() ||
                                target->Settings.SendSpeedLimit == options[static_cast<size_t>(index)])
                            {
                                return;
                            }

                            target->Settings.SendSpeedLimit = options[static_cast<size_t>(index)];
                            summary.Text(patchbay::DescribeBlock(target->Kind, target->Settings));

                            strong->CommitChange(true, false);
                            strong->RebuildCanvas();
                        });

                    body.Children().Append(speed);

                    auto help = ValueText(resources::GetString(L"InspectorSendingSpeedHelp"), 11, true);
                    help.Foreground(BrushOrNull(L"TextFillColorTertiaryBrush"));
                    body.Children().Append(help);
                }
                else if (kind == patchbay::BlockKind::ClockGenerator)
                {
                    controls::NumberBox tempo{};

                    tempo.Header(winrt::box_value(resources::GetString(L"GeneratorBeatsPerMinute")));
                    tempo.Minimum(patchbay::MinimumGeneratorBeatsPerMinute);
                    tempo.Maximum(patchbay::MaximumGeneratorBeatsPerMinute);
                    tempo.SmallChange(1);
                    tempo.LargeChange(10);
                    tempo.SpinButtonPlacementMode(controls::NumberBoxSpinButtonPlacementMode::Inline);
                    tempo.ValidationMode(controls::NumberBoxValidationMode::InvalidInputOverwritten);
                    tempo.Value(block.Settings.Clock.BeatsPerMinute);

                    tempo.ValueChanged([weak, blockId, summary](auto&&, controls::NumberBoxValueChangedEventArgs const& args)
                        {
                            auto strong = weak.get();
                            auto* current = strong == nullptr ? nullptr : strong->CurrentPatch();
                            auto* target = current == nullptr ? nullptr : current->FindBlock(blockId);

                            if (target == nullptr || std::isnan(args.NewValue()))
                            {
                                return;
                            }

                            // Two places, the same as the file keeps.
                            auto const value = std::round(std::clamp(args.NewValue(),
                                patchbay::MinimumGeneratorBeatsPerMinute,
                                patchbay::MaximumGeneratorBeatsPerMinute) * 100.0) / 100.0;

                            if (value == target->Settings.Clock.BeatsPerMinute)
                            {
                                return;
                            }

                            target->Settings.Clock.BeatsPerMinute = value;
                            summary.Text(patchbay::DescribeBlock(target->Kind, target->Settings));

                            strong->CommitChange(true, false);
                            strong->RebuildCanvas();
                        });

                    body.Children().Append(tempo);
                }

                if (kind != patchbay::BlockKind::Throttle)
                {
                    controls::Button editButton{};

                    editButton.Content(winrt::box_value(resources::GetString(L"ActionEditStep")));
                    editButton.HorizontalAlignment(xaml::HorizontalAlignment::Stretch);

                    editButton.Click([weak, blockId](auto&&, auto&&)
                        {
                            if (auto strong = weak.get())
                            {
                                strong->ShowBlockDialogAsync(blockId);
                            }
                        });

                    body.Children().Append(editButton);
                }

                section.Children().Append(Card(body));
                InspectorContent().Children().Append(section);
            }

            // ------------------------------------------------------- bypass
            {
                controls::StackPanel body{};
                body.Spacing(2);

                controls::ToggleSwitch bypass{};

                bypass.Header(winrt::box_value(resources::GetString(L"InspectorBypass")));
                bypass.IsOn(block.Bypassed);
                bypass.OnContent(winrt::box_value(resources::GetString(L"InspectorBypassOn")));
                bypass.OffContent(winrt::box_value(resources::GetString(L"InspectorBypassOff")));

                bypass.Toggled([weak, blockId](foundation::IInspectable const& s, auto&&)
                    {
                        auto strong = weak.get();
                        auto const control = s.try_as<controls::ToggleSwitch>();

                        if (strong != nullptr && control != nullptr)
                        {
                            strong->SetBlockBypassed(blockId, control.IsOn());
                        }
                    });

                body.Children().Append(bypass);

                auto hint = ValueText(resources::GetString(patchbay::IsGenerator(kind)
                    ? L"InspectorBypassGeneratorHint"
                    : L"InspectorBypassHint"), 11, true);
                hint.Foreground(BrushOrNull(L"TextFillColorTertiaryBrush"));
                body.Children().Append(hint);

                InspectorContent().Children().Append(body);
            }

            // ------------------------------------------------------- activity
            {
                auto section = Section(resources::GetString(L"InspectorActivity"));

                m_activityElementId = blockId;
                m_activityText = ValueText(ActivityText(blockId), 13, true);

                section.Children().Append(m_activityText);

                InspectorContent().Children().Append(section);
            }

            // -------------------------------------------------------- actions
            {
                auto actions = ActionRow();

                controls::Button duplicateButton{};
                duplicateButton.Content(winrt::box_value(resources::GetString(L"ActionDuplicateStep")));

                duplicateButton.Click([weak](auto&&, auto&&)
                    {
                        if (auto strong = weak.get())
                        {
                            strong->DuplicateSelection();
                        }
                    });

                actions.Children().Append(duplicateButton);

                controls::Button removeButton{};
                removeButton.Content(winrt::box_value(resources::GetString(L"ActionRemoveStep")));

                removeButton.Click([weak](auto&&, auto&&)
                    {
                        if (auto strong = weak.get())
                        {
                            strong->DeleteSelection();
                        }
                    });

                actions.Children().Append(removeButton);

                InspectorContent().Children().Append(actions);
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to build the step inspector.")
    }

    _Use_decl_annotations_
    void MainWindow::BuildSelectionInspector(size_t count) noexcept
    {
        try
        {
            auto weak = get_weak();

            InspectorContent().Children().Append(Card(ValueText(
                resources::FormatString(L"InspectorSelectionMessageFormat", count), 12, true)));

            auto actions = ActionRow();

            auto const addAction = [&actions, weak](wchar_t const* key, std::function<void(MainWindow&)> action)
                {
                    controls::Button button{};

                    button.Content(winrt::box_value(resources::GetString(key)));
                    button.Click([weak, action](auto&&, auto&&)
                        {
                            if (auto strong = weak.get())
                            {
                                action(*strong);
                            }
                        });

                    actions.Children().Append(button);
                };

            addAction(L"ActionCopy", [](MainWindow& window) { window.CopySelection(); });
            addAction(L"ActionDuplicate", [](MainWindow& window) { window.DuplicateSelection(); });
            addAction(L"ActionRemove", [](MainWindow& window) { window.DeleteSelection(); });

            InspectorContent().Children().Append(actions);
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to build the selection inspector.")
    }

    _Use_decl_annotations_
    controls::MenuFlyout MainWindow::BuildAddStepMenu(std::wstring const& connectionId) noexcept
    {
        controls::MenuFlyout menu{};

        try
        {
            auto weak = get_weak();

            for (auto const category : { patchbay::BlockCategory::Filter, patchbay::BlockCategory::Transform, patchbay::BlockCategory::Sending })
            {
                std::vector<patchbay::BlockKind> kinds{};

                for (auto const kind : patchbay::AllBlockKinds)
                {
                    if (patchbay::CategoryOf(kind) == category)
                    {
                        kinds.push_back(kind);
                    }
                }

                auto const makeItem = [weak, connectionId](patchbay::BlockKind kind)
                    {
                        controls::MenuFlyoutItem item{};

                        item.Text(patchbay::BlockKindName(kind));
                        controls::ToolTipService::SetToolTip(item, winrt::box_value(patchbay::BlockKindHint(kind)));

                        item.Click([weak, connectionId, kind](auto&&, auto&&)
                            {
                                if (auto strong = weak.get())
                                {
                                    strong->InsertBlockIntoConnection(kind, connectionId, std::nullopt);
                                }
                            });

                        return item;
                    };

                // A category of one is its one item, not a menu with one thing in it.
                if (kinds.size() == 1)
                {
                    menu.Items().Append(makeItem(kinds.front()));
                    continue;
                }

                controls::MenuFlyoutSubItem submenu{};
                submenu.Text(patchbay::BlockCategoryName(category));

                for (auto const kind : kinds)
                {
                    submenu.Items().Append(makeItem(kind));
                }

                menu.Items().Append(submenu);
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to build the add a step menu.")

        return menu;
    }

    _Use_decl_annotations_
    winrt::hstring MainWindow::DescribeLinkEnd(std::wstring const& nodeId, int32_t groupIndex, bool isSource) noexcept
    {
        try
        {
            auto const* patch = CurrentPatch();

            if (patch == nullptr)
            {
                return {};
            }

            if (auto const* block = patch->FindBlock(nodeId))
            {
                return patchbay::BlockDisplayName(*block);
            }

            if (auto const* endpoint = patch->FindEndpoint(nodeId))
            {
                std::wstring groupName{};

                if (groupIndex != patchbay::AllGroups)
                {
                    if (auto const live = patchbay::ResolveEndpoint(*endpoint))
                    {
                        groupName = live->GroupName(groupIndex, isSource);
                    }
                }

                return resources::FormatString(L"LinkEndFormat",
                    endpoint->DisplayName,
                    patchbay::DescribeGroupIndex(groupIndex, groupName, !isSource));
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to describe the end of a connection.")

        return {};
    }
}
