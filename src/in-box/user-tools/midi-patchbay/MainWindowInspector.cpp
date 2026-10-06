// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MainWindow.xaml.h"

#include "FontCatalog.h"
#include "RoundedShape.h"
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

        controls::Grid Card(
            _In_ xaml::UIElement const& content,
            _In_ std::wstring_view stroke = L"CardStrokeColorDefaultBrush") noexcept
        {
            return patchbay::MakeRoundedPanel(
                6,
                BrushOrNull(L"CardBackgroundFillColorSecondaryBrush"),
                BrushOrNull(stroke),
                xaml::ThicknessHelper::FromUniformLength(12),
                content).Panel;
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

        // Colors that read on the dark and the light theme alike.
        struct AnnotationSwatch
        {
            wchar_t const* NameKey;
            wchar_t const* Code;
        };

        constexpr AnnotationSwatch AnnotationSwatches[]
        {
            { L"AnnotationColorRed", L"#E74856" },
            { L"AnnotationColorOrange", L"#F7630C" },
            { L"AnnotationColorGold", L"#C19C00" },
            { L"AnnotationColorGreen", L"#16C60C" },
            { L"AnnotationColorTeal", L"#00B7C3" },
            { L"AnnotationColorBlue", L"#0078D4" },
            { L"AnnotationColorPurple", L"#8764B8" },
            { L"AnnotationColorPink", L"#E3008C" },
            { L"AnnotationColorGray", L"#7A7574" },
        };

        // The sizes a text editor offers. A size from a file that is not one of them is added.
        constexpr double AnnotationSizes[]{ 10, 12, 14, 16, 20, 24, 28, 32, 40, 48, 64, 80, 96 };

        bool SameFamilyName(_In_ std::wstring const& left, _In_ std::wstring const& right) noexcept
        {
            return ::CompareStringOrdinal(
                left.c_str(), static_cast<int>(left.size()),
                right.c_str(), static_cast<int>(right.size()), TRUE) == CSTR_EQUAL;
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
            m_ciStatusText = nullptr;
            m_annotationTextBox = nullptr;
            m_inspectorSummary = nullptr;
            m_stepSettingsFocus = nullptr;

            auto* patch = CurrentPatch();
            auto const kind = m_canvas.SelectionKind();

            if (patch == nullptr || kind == patchbay::CanvasSelectionKind::None)
            {
                SetInspectorVisible(false);
                return;
            }

            SetInspectorVisible(true);

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

                    if (patchbay::IsAnnotation(block->Kind))
                    {
                        BuildAnnotationInspector(*block);
                    }
                    else
                    {
                        BuildBlockInspector(*block);
                    }

                    return;
                }
            }
            else if (auto const* connection = patch->FindConnection(m_canvas.SelectedConnectionId()))
            {
                InspectorTitle().Text(resources::GetString(L"InspectorConnectionTitle"));
                BuildConnectionInspector(*connection);
                return;
            }

            SetInspectorVisible(false);
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

                    if (found == m_activity.end() || found->second.MessagesForwarded == 0)
                    {
                        return resources::GetString(LfoFollowsClock(elementId) ? L"ActivityWaitingForClock" : L"ActivityNothingSentYet");
                    }

                    return resources::FormatString(L"ActivitySentFormat", found->second.MessagesForwarded);
                }

                if (found == m_activity.end())
                {
                    return resources::GetString(L"ActivityNothingYet");
                }

                auto const& stats = found->second;

                // The MIDI-CI steps keep messages out too: MIDI-CI, packet by packet.
                if (patchbay::CategoryOf(block->Kind) == patchbay::BlockCategory::Filter ||
                    patchbay::CategoryOf(block->Kind) == patchbay::BlockCategory::CapabilityInquiry)
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

            if (m_ciStatusText != nullptr)
            {
                auto const status = CiStatusText(m_activityElementId);

                // Only when it changed, so a screen reader isn't told the same thing twice a second.
                if (m_ciStatusText.Text() != status)
                {
                    m_ciStatusText.Text(status);
                }
            }
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

                    InspectorContent().Children().Append(Card(body, L"AccentFillColorDefaultBrush"));
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

                controls::TextBlock badgeText{};
                badgeText.Text(patchbay::BlockKindBadge(kind));
                badgeText.FontSize(11);
                badgeText.FontWeight(winrt::Microsoft::UI::Text::FontWeights::SemiBold());
                badgeText.HorizontalAlignment(xaml::HorizontalAlignment::Center);
                badgeText.VerticalAlignment(xaml::VerticalAlignment::Center);
                badgeText.Foreground(patchbay::PatchCanvas::CategoryBrush(category));

                auto const badge = patchbay::MakeRoundedPanel(
                    4,
                    patchbay::PatchCanvas::CategoryBrush(category, 0.18),
                    patchbay::PatchCanvas::CategoryBrush(category),
                    xaml::ThicknessHelper::FromLengths(4, 0, 4, 0),
                    badgeText).Panel;

                badge.MinWidth(36);
                badge.Height(20);

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

                m_inspectorSummary = ValueText(patchbay::DescribeBlock(kind, block.Settings), 12, true);
                body.Children().Append(m_inspectorSummary);

                // Settings that fit are right here; the rest are behind the dialog.
                BuildStepSettings(block, body);

                if (!EditsInInspector(kind))
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

                if (kind == patchbay::BlockKind::CiResponder)
                {
                    m_ciStatusText = ValueText(CiStatusText(blockId), 12, true);
                    m_ciStatusText.Margin(xaml::ThicknessHelper::FromLengths(0, 8, 0, 0));
                    m_ciStatusText.IsTextSelectionEnabled(true);

                    section.Children().Append(m_ciStatusText);
                }

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
    void MainWindow::BuildAnnotationInspector(patchbay::PatchBlock const& block) noexcept
    {
        try
        {
            auto const blockId = block.Id;
            auto const note = block.Settings.Annotation;
            auto weak = get_weak();

            // Anything but the text is one step for Undo, and is drawn at once.
            auto const change = [weak, blockId](std::function<void(patchbay::AnnotationSettings&)> const& edit)
                {
                    auto strong = weak.get();
                    auto* current = strong == nullptr ? nullptr : strong->CurrentPatch();
                    auto* target = current == nullptr ? nullptr : current->FindBlock(blockId);

                    if (target == nullptr || !patchbay::IsAnnotation(target->Kind))
                    {
                        return;
                    }

                    auto edited = target->Settings.Annotation;
                    edit(edited);

                    if (edited == target->Settings.Annotation)
                    {
                        return;
                    }

                    target->Settings.Annotation = std::move(edited);

                    strong->m_canvas.RefreshAnnotation(blockId);
                    strong->CommitChange(false, false);
                };

            // ------------------------------------------------------- text
            {
                controls::StackPanel body{};
                body.Spacing(8);

                auto hint = ValueText(patchbay::BlockKindHint(patchbay::BlockKind::Annotation), 12, true);
                hint.Foreground(BrushOrNull(L"TextFillColorSecondaryBrush"));
                body.Children().Append(hint);

                // AcceptsReturn before Text: a box that is still one line when the text arrives
                // keeps only the first line of it.
                controls::TextBox textBox{};
                textBox.Header(winrt::box_value(resources::GetString(L"AnnotationTextHeader")));
                textBox.PlaceholderText(resources::GetString(L"AnnotationTextPlaceholder"));
                textBox.MaxLength(static_cast<int32_t>(patchbay::MaximumAnnotationLength));
                textBox.AcceptsReturn(true);
                textBox.TextWrapping(xaml::TextWrapping::Wrap);
                textBox.MinHeight(120);
                textBox.MaxHeight(320);
                textBox.Text(winrt::hstring{ note.Text });

                // Drawn on the canvas as it is typed, and one step for Undo once it is done.
                textBox.TextChanged([weak, blockId](foundation::IInspectable const& sender, auto&&)
                    {
                        auto strong = weak.get();
                        auto* current = strong == nullptr ? nullptr : strong->CurrentPatch();
                        auto* target = current == nullptr ? nullptr : current->FindBlock(blockId);
                        auto const box = sender.try_as<controls::TextBox>();

                        if (target == nullptr || box == nullptr || !patchbay::IsAnnotation(target->Kind))
                        {
                            return;
                        }

                        auto text = patchbay::AnnotationTextFrom(std::wstring_view{ box.Text() });

                        if (text == target->Settings.Annotation.Text)
                        {
                            return;
                        }

                        target->Settings.Annotation.Text = std::move(text);
                        strong->m_annotationTextChanged = true;
                        strong->m_canvas.RefreshAnnotation(blockId);
                    });

                textBox.LostFocus([weak](auto&&, auto&&)
                    {
                        if (auto strong = weak.get(); strong != nullptr && strong->m_annotationTextChanged)
                        {
                            strong->m_annotationTextChanged = false;
                            strong->CommitChange(false, false);
                        }
                    });

                body.Children().Append(textBox);
                m_annotationTextBox = textBox;

                InspectorContent().Children().Append(Card(body));
            }

            // ------------------------------------------------------- font
            {
                auto section = Section(resources::GetString(L"AnnotationFontHeader"));

                auto const families = std::make_shared<std::vector<std::wstring>>();

                // Filling the list raises SelectionChanged, which is not the customer choosing.
                auto const filling = std::make_shared<bool>(false);

                controls::ComboBox family{};
                family.HorizontalAlignment(xaml::HorizontalAlignment::Stretch);
                xaml::Automation::AutomationProperties::SetName(family, resources::GetString(L"AnnotationFontHeader"));

                // The fonts every PC has, or every font on this one. A font the annotation already
                // names stays in the list either way, and says so when this PC doesn't have it.
                auto const fill = [family, families, filling](bool all, std::wstring const& keep)
                    {
                        *filling = true;
                        auto const done = wil::scope_exit([filling]() { *filling = false; });

                        auto const offered = all ? midiapp::fonts::InstalledFamilies(true) : midiapp::fonts::InBoxFamilies();

                        families->clear();
                        family.Items().Clear();
                        family.Items().Append(winrt::box_value(resources::GetString(L"AnnotationFontDefault")));

                        auto const listed = std::any_of(offered.begin(), offered.end(),
                            [&keep](std::wstring const& name) { return SameFamilyName(name, keep); });

                        if (!keep.empty() && !listed)
                        {
                            families->push_back(keep);
                            family.Items().Append(winrt::box_value(midiapp::fonts::IsInstalled(keep)
                                ? winrt::hstring{ keep }
                                : resources::FormatString(L"AnnotationFontMissingFormat", keep)));
                        }

                        for (auto const& name : offered)
                        {
                            families->push_back(name);
                            family.Items().Append(winrt::box_value(winrt::hstring{ name }));
                        }

                        int32_t selected{ 0 };

                        for (size_t index = 0; index < families->size() && !keep.empty(); ++index)
                        {
                            if (SameFamilyName((*families)[index], keep))
                            {
                                selected = static_cast<int32_t>(index) + 1;
                                break;
                            }
                        }

                        family.SelectedIndex(selected);
                    };

                fill(patchbay::AppSettings::Current().ShowAllFonts(), note.FontFamily);

                family.SelectionChanged([change, families, filling](foundation::IInspectable const& sender, auto&&)
                    {
                        auto const box = sender.try_as<controls::ComboBox>();

                        if (*filling || box == nullptr || box.SelectedIndex() < 0)
                        {
                            return;
                        }

                        auto const index = static_cast<size_t>(box.SelectedIndex());
                        auto const name = index == 0 || index > families->size() ? std::wstring{} : (*families)[index - 1];

                        change([name](patchbay::AnnotationSettings& settings) { settings.FontFamily = name; });
                    });

                section.Children().Append(family);

                controls::CheckBox showAll{};
                showAll.Content(winrt::box_value(resources::GetString(L"AnnotationShowAllFonts")));
                showAll.IsChecked(patchbay::AppSettings::Current().ShowAllFonts());

                auto const onShowAll = [weak, blockId, fill](bool all)
                    {
                        patchbay::AppSettings::Current().ShowAllFonts(all);

                        auto strong = weak.get();
                        auto* current = strong == nullptr ? nullptr : strong->CurrentPatch();
                        auto const* target = current == nullptr ? nullptr : current->FindBlock(blockId);

                        fill(all, target == nullptr ? std::wstring{} : target->Settings.Annotation.FontFamily);
                    };

                showAll.Checked([onShowAll](auto&&, auto&&) { onShowAll(true); });
                showAll.Unchecked([onShowAll](auto&&, auto&&) { onShowAll(false); });

                section.Children().Append(showAll);

                auto showAllHint = ValueText(resources::GetString(L"AnnotationShowAllFontsHint"), 11, true);
                showAllHint.Margin(xaml::ThicknessHelper::FromLengths(0, -6, 0, 4));
                showAllHint.Foreground(BrushOrNull(L"TextFillColorTertiaryBrush"));
                section.Children().Append(showAllHint);

                // ---- size and style, side by side
                controls::Grid row{};
                row.ColumnSpacing(8);

                for (auto const width : { xaml::GridLength{ 1, xaml::GridUnitType::Star },
                                          xaml::GridLength{ 0, xaml::GridUnitType::Auto } })
                {
                    controls::ColumnDefinition column{};
                    column.Width(width);
                    row.ColumnDefinitions().Append(column);
                }

                std::vector<double> sizes(std::begin(AnnotationSizes), std::end(AnnotationSizes));

                if (std::find(sizes.begin(), sizes.end(), note.FontSize) == sizes.end())
                {
                    sizes.push_back(note.FontSize);
                    std::sort(sizes.begin(), sizes.end());
                }

                controls::ComboBox size{};
                size.HorizontalAlignment(xaml::HorizontalAlignment::Stretch);
                xaml::Automation::AutomationProperties::SetName(size, resources::GetString(L"AnnotationSizeName"));

                for (size_t index = 0; index < sizes.size(); ++index)
                {
                    size.Items().Append(winrt::box_value(patchbay::DescribeNumber(sizes[index], 1)));

                    if (sizes[index] == note.FontSize)
                    {
                        size.SelectedIndex(static_cast<int32_t>(index));
                    }
                }

                size.SelectionChanged([change, sizes](foundation::IInspectable const& sender, auto&&)
                    {
                        auto const box = sender.try_as<controls::ComboBox>();

                        if (box == nullptr || box.SelectedIndex() < 0 || static_cast<size_t>(box.SelectedIndex()) >= sizes.size())
                        {
                            return;
                        }

                        auto const value = sizes[static_cast<size_t>(box.SelectedIndex())];

                        change([value](patchbay::AnnotationSettings& settings) { settings.FontSize = value; });
                    });

                row.Children().Append(size);

                controls::StackPanel styles{};
                styles.Orientation(controls::Orientation::Horizontal);
                styles.Spacing(4);

                auto const addStyle = [&styles, change](wchar_t const* glyph, wchar_t const* nameKey, bool on,
                    void (*set)(patchbay::AnnotationSettings&, bool))
                    {
                        primitives::ToggleButton toggle{};

                        toggle.Width(40);
                        toggle.Height(32);
                        toggle.Padding(xaml::ThicknessHelper::FromUniformLength(0));
                        toggle.IsChecked(on);

                        controls::FontIcon icon{};
                        icon.Glyph(glyph);
                        icon.FontSize(14);
                        toggle.Content(icon);

                        auto const name = resources::GetString(nameKey);
                        xaml::Automation::AutomationProperties::SetName(toggle, name);
                        controls::ToolTipService::SetToolTip(toggle, winrt::box_value(name));

                        toggle.Click([change, set](foundation::IInspectable const& sender, auto&&)
                            {
                                auto const button = sender.try_as<primitives::ToggleButton>();

                                if (button == nullptr)
                                {
                                    return;
                                }

                                auto const value = button.IsChecked() != nullptr && button.IsChecked().Value();

                                change([set, value](patchbay::AnnotationSettings& settings) { set(settings, value); });
                            });

                        styles.Children().Append(toggle);
                    };

                addStyle(L"\uE8DD", L"AnnotationBold", note.Bold,
                    [](patchbay::AnnotationSettings& settings, bool value) { settings.Bold = value; });
                addStyle(L"\uE8DB", L"AnnotationItalic", note.Italic,
                    [](patchbay::AnnotationSettings& settings, bool value) { settings.Italic = value; });
                addStyle(L"\uE8DC", L"AnnotationUnderline", note.Underline,
                    [](patchbay::AnnotationSettings& settings, bool value) { settings.Underline = value; });

                controls::Grid::SetColumn(styles, 1);
                row.Children().Append(styles);

                section.Children().Append(row);

                InspectorContent().Children().Append(section);
            }

            // ------------------------------------------------------- color
            {
                auto section = Section(resources::GetString(L"AnnotationColorHeader"));

                auto const accent = BrushOrNull(L"AccentFillColorDefaultBrush");
                auto const swatchButtons = std::make_shared<std::vector<std::pair<controls::Button, std::wstring>>>();

                auto currentText = ValueText({}, 11, true);
                currentText.Foreground(BrushOrNull(L"TextFillColorTertiaryBrush"));

                auto const mark = [swatchButtons, accent, currentText](std::wstring const& code)
                    {
                        for (auto const& [button, buttonCode] : *swatchButtons)
                        {
                            button.BorderBrush(buttonCode == code && accent != nullptr
                                ? accent
                                : media::SolidColorBrush{ winrt::Windows::UI::Colors::Transparent() });
                        }

                        currentText.Text(code.empty() ? resources::GetString(L"AnnotationColorTheme") : winrt::hstring{ code });
                    };

                auto const apply = [change, mark](std::wstring const& code)
                    {
                        change([code](patchbay::AnnotationSettings& settings) { settings.Color = code; });
                        mark(code);
                    };

                controls::VariableSizedWrapGrid swatches{};
                swatches.Orientation(controls::Orientation::Horizontal);
                swatches.ItemWidth(36);
                swatches.ItemHeight(36);

                auto const addSwatch = [&swatches, swatchButtons, apply](winrt::hstring const& name, std::wstring const& code, media::Brush const& fill)
                    {
                        controls::Button button{};

                        button.Width(32);
                        button.Height(32);
                        button.Padding(xaml::ThicknessHelper::FromUniformLength(3));
                        button.BorderThickness(xaml::ThicknessHelper::FromUniformLength(2));
                        button.BorderBrush(media::SolidColorBrush{ winrt::Windows::UI::Colors::Transparent() });

                        // A Rectangle rather than a Border: a Border's corners are stepped at
                        // fractional scaling.
                        shapes::Rectangle swatch{};
                        swatch.Width(22);
                        swatch.Height(22);
                        swatch.RadiusX(4);
                        swatch.RadiusY(4);
                        swatch.UseLayoutRounding(false);
                        swatch.StrokeThickness(1);
                        swatch.Stroke(BrushOrNull(L"CardStrokeColorDefaultBrush"));
                        swatch.Fill(fill);

                        button.Content(swatch);

                        xaml::Automation::AutomationProperties::SetName(button, name);
                        controls::ToolTipService::SetToolTip(button, winrt::box_value(name));

                        button.Click([apply, code](auto&&, auto&&) { apply(code); });

                        swatchButtons->emplace_back(button, code);
                        swatches.Children().Append(button);
                    };

                // The theme's own text color first, because it is the one that reads in both themes.
                addSwatch(resources::GetString(L"AnnotationColorTheme"), std::wstring{}, BrushOrNull(L"TextFillColorPrimaryBrush"));

                for (auto const& entry : AnnotationSwatches)
                {
                    auto const color = patchbay::PatchCanvas::ParseColorCode(entry.Code);

                    addSwatch(resources::GetString(entry.NameKey), entry.Code,
                        media::SolidColorBrush{ color.value_or(winrt::Windows::UI::Colors::Gray()) });
                }

                section.Children().Append(swatches);

                // ---- anything else
                controls::Button more{};
                more.Content(winrt::box_value(resources::GetString(L"AnnotationColorMore")));

                controls::Flyout flyout{};

                controls::StackPanel flyoutBody{};
                flyoutBody.Spacing(8);

                controls::ColorPicker picker{};
                picker.IsAlphaEnabled(false);
                picker.IsColorSliderVisible(true);
                picker.IsHexInputVisible(true);
                picker.IsColorChannelTextInputVisible(false);
                picker.ColorSpectrumShape(controls::ColorSpectrumShape::Box);

                if (auto const color = patchbay::PatchCanvas::ParseColorCode(note.Color))
                {
                    picker.Color(color.value());
                }

                controls::Button use{};
                use.Content(winrt::box_value(resources::GetString(L"AnnotationColorUse")));
                use.Style(xaml::Application::Current().Resources()
                    .Lookup(winrt::box_value(L"AccentButtonStyle")).as<xaml::Style>());

                use.Click([apply, picker, flyout](auto&&, auto&&)
                    {
                        apply(patchbay::PatchCanvas::ColorCode(picker.Color()));
                        flyout.Hide();
                    });

                flyoutBody.Children().Append(picker);
                flyoutBody.Children().Append(use);
                flyout.Content(flyoutBody);
                more.Flyout(flyout);

                section.Children().Append(more);
                section.Children().Append(currentText);

                mark(note.Color);

                InspectorContent().Children().Append(section);
            }

            // -------------------------------------------------------- actions
            {
                auto actions = ActionRow();

                controls::Button duplicateButton{};
                duplicateButton.Content(winrt::box_value(resources::GetString(L"ActionDuplicate")));

                duplicateButton.Click([weak](auto&&, auto&&)
                    {
                        if (auto strong = weak.get())
                        {
                            strong->DuplicateSelection();
                        }
                    });

                actions.Children().Append(duplicateButton);

                controls::Button removeButton{};
                removeButton.Content(winrt::box_value(resources::GetString(L"ActionRemove")));

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
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to build the annotation inspector.")
    }

    _Use_decl_annotations_
    void MainWindow::FocusAnnotationText(std::wstring const& blockId) noexcept
    {
        try
        {
            auto const* patch = CurrentPatch();
            auto const* block = patch == nullptr ? nullptr : patch->FindBlock(blockId);

            if (block == nullptr || !patchbay::IsAnnotation(block->Kind))
            {
                return;
            }

            if (m_canvas.SelectionKind() != patchbay::CanvasSelectionKind::Block ||
                m_canvas.SelectedNodeId() != blockId ||
                m_canvas.SelectedNodeIds().size() != 1)
            {
                m_canvas.Select(patchbay::CanvasSelectionKind::Block, blockId);
            }

            // After the press or the drop that got here has finished, or it takes the focus back.
            DispatcherQueue().TryEnqueue([weak = get_weak()]()
                {
                    auto strong = weak.get();

                    if (strong == nullptr || strong->m_annotationTextBox == nullptr)
                    {
                        return;
                    }

                    strong->m_annotationTextBox.Focus(xaml::FocusState::Programmatic);
                    strong->m_annotationTextBox.SelectAll();
                });
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to go to the annotation's text.")
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

            for (auto const category : { patchbay::BlockCategory::Filter, patchbay::BlockCategory::Transform, patchbay::BlockCategory::Sending, patchbay::BlockCategory::Distribution, patchbay::BlockCategory::CapabilityInquiry })
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
