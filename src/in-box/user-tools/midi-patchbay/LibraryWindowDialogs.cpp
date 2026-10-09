// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "LibraryWindow.xaml.h"

#include "AssistantPrompt.h"
#include "PatchCanvas.h"
#include "StringResources.h"

using namespace winrt::Microsoft::UI::Xaml;

namespace patchbay = ::midipatchbay;
namespace resources = ::midipatchbay::resources;

namespace winrt::midipatchbay::implementation
{
    namespace
    {
        // A short link, so the guide can move without changing the app.
        constexpr wchar_t AssistantGuideUrl[] = L"https://aka.ms/AgentGuideMidiPatchbay";

        // A quick patch's two nodes, side by side with room for a step between them later.
        constexpr double QuickPatchLeft = 48.0;
        constexpr double QuickPatchTop = 48.0;
        constexpr double QuickPatchGap = 200.0;

        // "groups 1 - 4", "group 1", or nothing at all, for one direction of an endpoint.
        std::wstring DescribeGroupsForPrompt(_In_ std::array<bool, patchbay::MaximumGroupCount> const& groups)
        {
            auto const count = std::count(groups.begin(), groups.end(), true);

            if (count == 0)
            {
                return std::wstring{ resources::GetString(L"AssistantPromptNoGroups") };
            }

            auto const runs = patchbay::DescribeRuns(groups.data(), groups.size(), 1);

            return std::wstring{ count == 1
                ? resources::FormatString(L"AssistantPromptGroupFormat", runs)
                : resources::FormatString(L"AssistantPromptGroupsFormat", runs) };
        }
    }

    // ------------------------------------------------------------ quick patch

    _Use_decl_annotations_
    void LibraryWindow::OnQuickPatchSourceChanged(
        foundation::IInspectable const& sender,
        controls::SelectionChangedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        if (m_fillingQuickPatch)
        {
            return;
        }

        FillQuickPatchGroups(true);
        ValidateQuickPatch();
    }

    _Use_decl_annotations_
    void LibraryWindow::OnQuickPatchDestinationChanged(
        foundation::IInspectable const& sender,
        controls::SelectionChangedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        if (m_fillingQuickPatch)
        {
            return;
        }

        FillQuickPatchGroups(false);
        ValidateQuickPatch();
    }

    _Use_decl_annotations_
    void LibraryWindow::OnQuickPatchGroupChanged(
        foundation::IInspectable const& sender,
        controls::SelectionChangedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        if (m_fillingQuickPatch)
        {
            return;
        }

        ValidateQuickPatch();
    }

    _Use_decl_annotations_
    void LibraryWindow::FillQuickPatchGroups(bool isSource) noexcept
    {
        try
        {
            m_fillingQuickPatch = true;
            auto const reset = wil::scope_exit([this]() { m_fillingQuickPatch = false; });

            auto const endpointCombo = isSource ? QuickPatchSourceCombo() : QuickPatchDestinationCombo();
            auto const groupCombo = isSource ? QuickPatchSourceGroupCombo() : QuickPatchDestinationGroupCombo();
            auto const& ids = isSource ? m_quickSourceIds : m_quickDestinationIds;

            auto& groups = isSource ? m_quickSourceGroups : m_quickDestinationGroups;

            groups.clear();
            groups.push_back(patchbay::AllGroups);

            auto const index = endpointCombo.SelectedIndex();

            std::optional<patchbay::LiveEndpoint> live{};

            if (index >= 0 && static_cast<size_t>(index) < ids.size())
            {
                live = patchbay::EndpointCatalog::Current().Find(ids[static_cast<size_t>(index)]);
            }

            if (live.has_value())
            {
                for (int32_t group = 0; group < patchbay::MaximumGroupCount; group++)
                {
                    if (live->DeclaredGroups[static_cast<size_t>(group)])
                    {
                        groups.push_back(group);
                    }
                }
            }

            auto items = winrt::single_threaded_vector<foundation::IInspectable>();

            for (auto const group : groups)
            {
                if (group == patchbay::AllGroups)
                {
                    items.Append(winrt::box_value(resources::GetString(isSource ? L"PortAllGroups" : L"PortAnyGroup")));
                    continue;
                }

                items.Append(winrt::box_value(patchbay::DescribeGroupIndex(
                    group, live.has_value() ? live->GroupName(group, isSource) : std::wstring{})));
            }

            groupCombo.ItemsSource(items);
            groupCombo.SelectedIndex(0);
            groupCombo.IsEnabled(groups.size() > 1);
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to list the groups for the quick patch.")
    }

    void LibraryWindow::ValidateQuickPatch() noexcept
    {
        try
        {
            auto const sourceIndex = QuickPatchSourceCombo().SelectedIndex();
            auto const destinationIndex = QuickPatchDestinationCombo().SelectedIndex();

            winrt::hstring problem{};

            if (sourceIndex < 0 || destinationIndex < 0)
            {
                problem = resources::GetString(L"QuickPatchPickBoth");
            }
            else if (static_cast<size_t>(sourceIndex) < m_quickSourceIds.size() &&
                static_cast<size_t>(destinationIndex) < m_quickDestinationIds.size())
            {
                auto const sourceGroup = QuickPatchSourceGroupCombo().SelectedIndex();
                auto const destinationGroup = QuickPatchDestinationGroupCombo().SelectedIndex();

                // The same endpoint on both ends is a real routing, but only across groups.
                if (m_quickSourceIds[static_cast<size_t>(sourceIndex)] ==
                    m_quickDestinationIds[static_cast<size_t>(destinationIndex)] &&
                    sourceGroup == destinationGroup)
                {
                    problem = resources::GetString(L"QuickPatchSameGroup");
                }
            }

            QuickPatchErrorText().Text(problem);
            QuickPatchErrorText().Visibility(problem.empty()
                ? xaml::Visibility::Collapsed : xaml::Visibility::Visible);

            QuickPatchDialog().IsPrimaryButtonEnabled(problem.empty());
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to check the quick patch.")
    }

    winrt::fire_and_forget LibraryWindow::ShowQuickPatchDialogAsync()
    {
        auto strong = get_strong();

        try
        {
            auto const live = patchbay::PatchLibrary::Current().LiveEndpoints();

            if (live.empty())
            {
                ShowStatus(resources::GetString(L"QuickPatchNoEndpoints"), controls::InfoBarSeverity::Warning);
                co_return;
            }

            m_fillingQuickPatch = true;

            m_quickSourceIds.clear();
            m_quickDestinationIds.clear();

            auto sourceItems = winrt::single_threaded_vector<foundation::IInspectable>();
            auto destinationItems = winrt::single_threaded_vector<foundation::IInspectable>();

            for (auto const& endpoint : live)
            {
                m_quickSourceIds.push_back(endpoint.EndpointDeviceId);
                m_quickDestinationIds.push_back(endpoint.EndpointDeviceId);

                sourceItems.Append(winrt::box_value(winrt::hstring{ endpoint.Name }));
                destinationItems.Append(winrt::box_value(winrt::hstring{ endpoint.Name }));
            }

            QuickPatchSourceCombo().ItemsSource(sourceItems);
            QuickPatchDestinationCombo().ItemsSource(destinationItems);

            QuickPatchSourceCombo().SelectedIndex(0);
            QuickPatchDestinationCombo().SelectedIndex(live.size() > 1 ? 1 : 0);

            m_fillingQuickPatch = false;

            FillQuickPatchGroups(true);
            FillQuickPatchGroups(false);
            ValidateQuickPatch();

            QuickPatchDialog().XamlRoot(Content().XamlRoot());

            auto const result = co_await QuickPatchDialog().ShowAsync();

            if (result != controls::ContentDialogResult::Primary)
            {
                co_return;
            }

            CreateQuickPatch();
        }
        catch (...)
        {
            m_fillingQuickPatch = false;
            MIDI_PATCHBAY_LOG_GENERAL_EXCEPTION(L"Unable to show the quick patch dialog.");
        }
    }

    void LibraryWindow::CreateQuickPatch() noexcept
    {
        try
        {
            auto const sourceIndex = QuickPatchSourceCombo().SelectedIndex();
            auto const destinationIndex = QuickPatchDestinationCombo().SelectedIndex();

            if (sourceIndex < 0 || static_cast<size_t>(sourceIndex) >= m_quickSourceIds.size() ||
                destinationIndex < 0 || static_cast<size_t>(destinationIndex) >= m_quickDestinationIds.size())
            {
                return;
            }

            auto const source = patchbay::EndpointCatalog::Current()
                .Find(m_quickSourceIds[static_cast<size_t>(sourceIndex)]);
            auto const destination = patchbay::EndpointCatalog::Current()
                .Find(m_quickDestinationIds[static_cast<size_t>(destinationIndex)]);

            if (!source.has_value() || !destination.has_value())
            {
                ShowStatus(resources::GetString(L"QuickPatchEndpointGone"), controls::InfoBarSeverity::Warning);
                return;
            }

            auto const groupAt = [](std::vector<int32_t> const& groups, int32_t index)
                {
                    return index >= 0 && static_cast<size_t>(index) < groups.size()
                        ? groups[static_cast<size_t>(index)] : patchbay::AllGroups;
                };

            auto const sourceGroup = groupAt(m_quickSourceGroups, QuickPatchSourceGroupCombo().SelectedIndex());
            auto const destinationGroup = groupAt(m_quickDestinationGroups, QuickPatchDestinationGroupCombo().SelectedIndex());

            auto& library = patchbay::PatchLibrary::Current();

            auto* patch = library.Create(std::wstring{
                resources::FormatString(L"QuickPatchNameFormat", source->Name, destination->Name) });

            if (patch == nullptr)
            {
                ShowStatus(library.LastErrorMessage(), controls::InfoBarSeverity::Error);
                return;
            }

            auto const addNode = [patch](patchbay::LiveEndpoint const& endpoint, double x)
                {
                    patchbay::PatchEndpoint node{};

                    node.Id = patchbay::PatchDocument::NewId();
                    node.DisplayName = endpoint.Name;
                    node.TransportCode = endpoint.TransportCode;
                    node.Match = endpoint.BuildMatch();
                    node.MatchMode = patchbay::EndpointMatchMode::EndpointDeviceId;
                    node.CanvasX = x;
                    node.CanvasY = QuickPatchTop;

                    auto const id = node.Id;

                    patch->Endpoints.push_back(std::move(node));

                    return id;
                };

            auto const sourceNodeId = addNode(source.value(), QuickPatchLeft);

            // One endpoint routed across its own groups needs one node, not two stacked copies.
            auto const destinationNodeId = source->EndpointDeviceId == destination->EndpointDeviceId
                ? sourceNodeId
                : addNode(destination.value(), QuickPatchLeft + patchbay::PatchCanvas::MinimumNodeWidth + QuickPatchGap);

            patchbay::PatchConnection connection{};

            connection.Id = patchbay::PatchDocument::NewId();
            connection.SourceId = sourceNodeId;
            connection.SourceGroupIndex = sourceGroup;
            connection.DestinationId = destinationNodeId;
            connection.DestinationGroupIndex = destinationGroup;

            patch->Connections.push_back(connection);

            auto const key = patch->SessionKey;

            library.Changed(key, true);

            OpenPatch(key);
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to create the quick patch.")
    }

    // ------------------------------------------------------ ask an AI assistant

    winrt::fire_and_forget LibraryWindow::ShowAssistantDialogAsync()
    {
        auto strong = get_strong();

        try
        {
            std::wstring const guide{ AssistantGuideUrl };

            // Each endpoint with the groups it sends and receives on, because a patch names
            // groups and an assistant cannot see the device to find out.
            auto live = patchbay::PatchLibrary::Current().LiveEndpoints();

            std::sort(live.begin(), live.end(), [](patchbay::LiveEndpoint const& a, patchbay::LiveEndpoint const& b)
                {
                    return ::CompareStringOrdinal(a.Name.c_str(), -1, b.Name.c_str(), -1, TRUE) == CSTR_LESS_THAN;
                });

            std::wstring devices{};

            for (auto const& endpoint : live)
            {
                auto const name = patchbay::SanitizeStoredString(endpoint.Name);

                if (name.empty())
                {
                    continue;
                }

                if (!devices.empty())
                {
                    devices += L"\r\n";
                }

                devices += resources::FormatString(L"AssistantPromptEndpointFormat",
                    name,
                    DescribeGroupsForPrompt(endpoint.SourceGroups),
                    DescribeGroupsForPrompt(endpoint.DestinationGroups));
            }

            std::wstring prompt{ resources::FormatString(L"AssistantPromptIntroFormat", guide) };
            prompt += L"\r\n\r\n";

            if (devices.empty())
            {
                prompt += resources::GetString(L"AssistantPromptNoDevices");
            }
            else
            {
                prompt += resources::GetString(L"AssistantPromptDevices");
                prompt += L"\r\n";
                prompt += devices;
            }

            prompt += L"\r\n\r\n";
            prompt += resources::GetString(L"AssistantPromptRequest");

            // The customer types their request straight after it.
            prompt += L" ";

            midiapp::AssistantPromptStrings strings{};
            strings.Title = resources::GetString(L"AssistantTitle");
            strings.Message = resources::GetString(L"AssistantMessage");
            strings.PromptHeader = resources::GetString(L"AssistantPromptHeader");
            strings.GuideLink = resources::GetString(L"AssistantGuideLink");
            strings.CopyButton = resources::GetString(L"AssistantCopy");
            strings.CopiedButton = resources::GetString(L"AssistantCopied");
            strings.CopyFailedButton = resources::GetString(L"AssistantCopyFailed");
            strings.CloseButton = resources::GetString(L"AssistantClose");

            co_await midiapp::ShowAssistantPromptAsync(
                Content().XamlRoot(), strings, winrt::hstring{ prompt }, foundation::Uri{ guide });
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to show the AI assistant prompt.")
    }
}
