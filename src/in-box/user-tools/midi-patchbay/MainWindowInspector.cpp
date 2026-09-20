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
    }

    void MainWindow::RefreshInspector() noexcept
    {
        try
        {
            InspectorContent().Children().Clear();

            m_activityText = nullptr;
            m_activityConnectionId.clear();

            auto* patch = CurrentPatch();

            if (patch == nullptr || m_canvas.SelectionKind() == patchbay::CanvasSelectionKind::None)
            {
                InspectorPanel().Visibility(xaml::Visibility::Collapsed);
                return;
            }

            if (m_canvas.SelectionKind() == patchbay::CanvasSelectionKind::Endpoint)
            {
                auto const* endpoint = patch->FindEndpoint(m_canvas.SelectedEndpointId());

                if (endpoint == nullptr)
                {
                    InspectorPanel().Visibility(xaml::Visibility::Collapsed);
                    return;
                }

                InspectorPanel().Visibility(xaml::Visibility::Visible);
                InspectorTitle().Text(winrt::hstring{ endpoint->DisplayName });
                BuildEndpointInspector(*endpoint);
                return;
            }

            auto const* connection = patch->FindConnection(m_canvas.SelectedConnectionId());

            if (connection == nullptr)
            {
                InspectorPanel().Visibility(xaml::Visibility::Collapsed);
                return;
            }

            InspectorPanel().Visibility(xaml::Visibility::Visible);
            InspectorTitle().Text(resources::GetString(L"InspectorConnectionTitle"));
            BuildConnectionInspector(*connection);
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to build the inspector.")
    }

    _Use_decl_annotations_
    winrt::hstring MainWindow::ConnectionActivityText(std::wstring const& connectionId) const noexcept
    {
        try
        {
            if (m_analysis.LoopMutedConnectionIds.count(connectionId) != 0)
            {
                return resources::GetString(L"ConnectionLoopMuted");
            }

            auto const* patch = const_cast<MainWindow*>(this)->CurrentPatch();
            auto const* connection = patch == nullptr ? nullptr : patch->FindConnection(connectionId);

            if (connection != nullptr && connection->Muted)
            {
                return resources::GetString(L"ConnectionMuted");
            }

            auto const it = m_routeStats.find(connectionId);

            if (it == m_routeStats.end())
            {
                return resources::GetString(L"ConnectionWaiting");
            }

            auto text = resources::FormatString(L"ConnectionForwardedFormat", it->second.MessagesForwarded);

            if (it->second.SendFailures > 0)
            {
                text = text + winrt::hstring{ L"\n" } +
                    resources::FormatString(L"ConnectionSendFailuresFormat", it->second.SendFailures);
            }

            return text;
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to describe the connection activity.")

        return {};
    }

    void MainWindow::UpdateInspectorActivity() noexcept
    {
        try
        {
            if (m_activityText == nullptr || m_activityConnectionId.empty())
            {
                return;
            }

            m_activityText.Text(ConnectionActivityText(m_activityConnectionId));
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to refresh the connection activity.")
    }

    _Use_decl_annotations_
    void MainWindow::BuildEndpointInspector(patchbay::PatchEndpoint const& endpoint) noexcept
    {
        try
        {
            auto const endpointId = endpoint.Id;
            auto weak = get_weak();

            auto& catalog = patchbay::EndpointCatalog::Current();

            auto const live = catalog.Resolve(endpoint);

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

                                strong->MarkDirty();
                                strong->RebuildCanvas();
                                strong->UpdateMessages();
                                strong->ApplyRouting();
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
                auto const suggestion = catalog.SuggestReplacement(endpoint);

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

                            strong->MarkDirty();
                            strong->RefreshAnalysis();
                            strong->RebuildCanvas();
                            strong->RefreshInspector();
                            strong->UpdateMessages();
                            strong->ApplyRouting();
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

                            strong->MarkDirty();
                            strong->RebuildCanvas();
                            strong->RefreshInspector();
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
                        strong->MarkDirty();
                        strong->RefreshAnalysis();
                        strong->RebuildCanvas();
                        strong->RefreshInspector();
                        strong->UpdateMessages();
                        strong->ApplyRouting();
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

            auto const* source = patch->FindEndpoint(connection.SourceEndpointId);
            auto const* destination = patch->FindEndpoint(connection.DestinationEndpointId);

            auto& catalog = patchbay::EndpointCatalog::Current();

            auto const liveSource = source == nullptr ? std::nullopt : catalog.Resolve(*source);
            auto const liveDestination = destination == nullptr ? std::nullopt : catalog.Resolve(*destination);

            // ----------------------------------------------------- from / to
            {
                controls::StackPanel body{};
                body.Spacing(3);

                auto const addEnd = [&](winrt::hstring const& label,
                    patchbay::PatchEndpoint const* endpoint,
                    std::optional<patchbay::LiveEndpoint> const& live,
                    int32_t groupIndex,
                    bool isOutput)
                    {
                        auto labelText = ValueText(label, 11);
                        labelText.Foreground(BrushOrNull(L"TextFillColorTertiaryBrush"));
                        body.Children().Append(labelText);

                        body.Children().Append(ValueText(
                            winrt::hstring{ endpoint == nullptr ? L"" : endpoint->DisplayName }, 13, true));

                        std::wstring portName{};

                        if (live.has_value() && groupIndex != patchbay::AllGroups)
                        {
                            portName = live->PortName(groupIndex, isOutput);
                        }

                        auto detail = ValueText(patchbay::DescribeGroupIndex(groupIndex, portName), 11, true);
                        detail.Foreground(BrushOrNull(L"TextFillColorTertiaryBrush"));
                        detail.Margin(xaml::ThicknessHelper::FromLengths(0, 0, 0, 6));
                        body.Children().Append(detail);
                    };

                addEnd(resources::GetString(L"InspectorFrom"), source, liveSource,
                    connection.SourceGroupIndex, true);
                addEnd(resources::GetString(L"InspectorTo"), destination, liveDestination,
                    connection.DestinationGroupIndex, false);

                InspectorContent().Children().Append(Card(body));
            }

            // -------------------------------------------------- destination group
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
                    std::wstring portName{};

                    if (liveDestination.has_value() && options[i] != patchbay::AllGroups)
                    {
                        portName = liveDestination->PortName(options[i], false);
                    }

                    items.Append(winrt::box_value(options[i] == patchbay::AllGroups
                        ? resources::GetString(L"GroupSameAsSource")
                        : patchbay::DescribeGroupIndex(options[i], portName)));

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

                        strong->MarkDirty();
                        strong->RefreshAnalysis();
                        strong->RebuildCanvas();
                        strong->UpdateMessages();
                        strong->ApplyRouting();
                    });

                section.Children().Append(combo);
                InspectorContent().Children().Append(section);
            }

            // ------------------------------------------------ filters
            {
                auto section = Section(resources::GetString(L"InspectorFilters"));

                controls::StackPanel body{};
                body.Spacing(8);

                auto summary = ValueText(patchbay::SummarizeFilter(connection.Filter), 12, false);
                summary.TextWrapping(xaml::TextWrapping::Wrap);
                summary.TextTrimming(xaml::TextTrimming::None);

                if (connection.Filter.PassesEverything())
                {
                    summary.Foreground(BrushOrNull(L"TextFillColorTertiaryBrush"));
                }

                body.Children().Append(summary);

                controls::Button editButton{};
                editButton.Content(winrt::box_value(resources::GetString(L"ActionEditFilters")));
                editButton.HorizontalAlignment(xaml::HorizontalAlignment::Stretch);

                editButton.Click([weak, connectionId](auto&&, auto&&)
                    {
                        if (auto strong = weak.get())
                        {
                            strong->ShowFilterDialogAsync(connectionId);
                        }
                    });

                body.Children().Append(editButton);

                section.Children().Append(Card(body));
                InspectorContent().Children().Append(section);
            }

            // ------------------------------------------------ transforms
            {
                auto section = Section(resources::GetString(L"InspectorTransforms"));

                controls::StackPanel body{};
                body.Spacing(8);

                auto summary = ValueText(patchbay::SummarizeTransform(connection.Transform), 12, false);
                summary.TextWrapping(xaml::TextWrapping::Wrap);
                summary.TextTrimming(xaml::TextTrimming::None);

                if (connection.Transform.ChangesNothing())
                {
                    summary.Foreground(BrushOrNull(L"TextFillColorTertiaryBrush"));
                }

                body.Children().Append(summary);

                controls::Button transformButton{};
                transformButton.Content(winrt::box_value(resources::GetString(L"ActionEditTransforms")));
                transformButton.HorizontalAlignment(xaml::HorizontalAlignment::Stretch);

                transformButton.Click([weak, connectionId](auto&&, auto&&)
                    {
                        if (auto strong = weak.get())
                        {
                            strong->ShowTransformDialogAsync(connectionId);
                        }
                    });

                body.Children().Append(transformButton);

                section.Children().Append(Card(body));
                InspectorContent().Children().Append(section);
            }

            // ------------------------------------------------------- activity
            {
                auto section = Section(resources::GetString(L"InspectorActivity"));

                m_activityConnectionId = connectionId;
                m_activityText = ValueText(ConnectionActivityText(connectionId), 13, true);

                section.Children().Append(m_activityText);

                InspectorContent().Children().Append(section);
            }

            // -------------------------------------------------------- actions
            {
                controls::StackPanel actions{};
                actions.Spacing(8);
                actions.Orientation(controls::Orientation::Horizontal);

                controls::Button muteButton{};
                muteButton.Content(winrt::box_value(connection.Muted
                    ? resources::GetString(L"ActionUnmute")
                    : resources::GetString(L"ActionMute")));

                muteButton.Click([weak, connectionId](auto&&, auto&&)
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

                        if (auto* target = current->FindConnection(connectionId))
                        {
                            target->Muted = !target->Muted;

                            strong->MarkDirty();
                            strong->RefreshAnalysis();
                            strong->RebuildCanvas();
                            strong->RefreshInspector();
                            strong->UpdateMessages();
                            strong->ApplyRouting();
                        }
                    });

                actions.Children().Append(muteButton);

                controls::Button removeButton{};
                removeButton.Content(winrt::box_value(resources::GetString(L"ActionRemoveConnection")));

                removeButton.Click([weak, connectionId](auto&&, auto&&)
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

                        current->RemoveConnection(connectionId);

                        strong->m_canvas.ClearSelection();
                        strong->MarkDirty();
                        strong->RefreshAnalysis();
                        strong->RebuildCanvas();
                        strong->RefreshInspector();
                        strong->UpdateMessages();
                        strong->ApplyRouting();
                    });

                actions.Children().Append(removeButton);

                InspectorContent().Children().Append(actions);
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to build the connection inspector.")
    }
}
