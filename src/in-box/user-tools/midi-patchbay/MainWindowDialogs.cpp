// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MainWindow.xaml.h"

#include "BackgroundWork.h"
#include "StringResources.h"

using namespace winrt::Microsoft::UI::Xaml;

namespace patchbay = ::midipatchbay;
namespace resources = ::midipatchbay::resources;

namespace winrt::midipatchbay::implementation
{
    namespace
    {
        // New nodes land in a two column grid. A diagonal step looks tidier on paper but runs
        // nodes over each other as soon as a patch has more than two or three endpoints.
        constexpr double FirstNodeX = 48.0;
        constexpr double FirstNodeY = 32.0;
        constexpr double NodeColumnPitch = 340.0;
        constexpr double NodeRowPitch = 230.0;

        void PlaceNewNode(_Inout_ patchbay::PatchEndpoint& endpoint, _In_ size_t index) noexcept
        {
            endpoint.CanvasX = FirstNodeX + static_cast<double>(index % 2) * NodeColumnPitch;
            endpoint.CanvasY = FirstNodeY + static_cast<double>(index / 2) * NodeRowPitch;
        }

        // The other tools in this family all take an endpoint device id on the command line,
        // which is what makes testing a route a launch rather than a feature to rebuild here.
        constexpr wchar_t MonitorExeName[] = L"midi2monitor.exe";
        constexpr wchar_t KeyboardExeName[] = L"midikeyboard.exe";
        constexpr wchar_t ScratchPadExeName[] = L"midiscratchpad.exe";

        std::wstring ExecutableFolder() noexcept
        {
            std::wstring buffer(MAX_PATH, L'\0');

            auto const length = ::GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));

            if (length == 0 || length >= buffer.size())
            {
                return {};
            }

            buffer.resize(length);

            std::error_code ec{};
            auto const parent = std::filesystem::path{ buffer }.parent_path();

            return parent.wstring();
        }
    }

    // ------------------------------------------------------------ save a patch

    _Use_decl_annotations_
    void MainWindow::OnSavePatchClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        ShowSavePatchDialogAsync();
    }

    _Use_decl_annotations_
    void MainWindow::OnSavePatchNameChanged(
        foundation::IInspectable const& sender,
        controls::TextChangedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        try
        {
            // live feedback rather than a modal complaint after the fact
            SavePatchDialog().IsPrimaryButtonEnabled(
                !patchbay::SanitizeStoredString(std::wstring{ SavePatchNameBox().Text() }).empty());
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to validate the patch name.")
    }

    winrt::fire_and_forget MainWindow::ShowSavePatchDialogAsync()
    {
        auto strong = get_strong();

        try
        {
            auto* patch = CurrentPatch();

            if (patch == nullptr)
            {
                co_return;
            }

            auto const previousKey = PatchKey(*patch);

            SavePatchNameBox().Text(winrt::hstring{ patch->Name });
            SavePatchDescriptionBox().Text(winrt::hstring{ patch->Description });
            SavePatchKeepRadio().IsChecked(!patch->IsTemporary || patch->FilePath.empty());
            SavePatchTemporaryRadio().IsChecked(false);
            SavePatchStartupCheck().IsChecked(patch->ActivateAtStartup);
            SavePatchDialog().IsPrimaryButtonEnabled(!patch->Name.empty());
            SavePatchDialog().XamlRoot(Content().XamlRoot());

            auto const result = co_await SavePatchDialog().ShowAsync();

            if (result != controls::ContentDialogResult::Primary)
            {
                co_return;
            }

            // The patch pointer cannot survive the suspends above, so it is looked up again.
            auto it = std::find_if(m_patches.begin(), m_patches.end(),
                [&previousKey, this](patchbay::PatchDocument& p) { return PatchKey(p) == previousKey; });

            if (it == m_patches.end())
            {
                co_return;
            }

            auto const name = patchbay::SanitizeStoredString(std::wstring{ SavePatchNameBox().Text() });

            if (name.empty())
            {
                co_return;
            }

            it->Name = name;
            it->Description = patchbay::SanitizeStoredString(std::wstring{ SavePatchDescriptionBox().Text() });

            auto const startup = SavePatchStartupCheck().IsChecked();
            it->ActivateAtStartup = startup && startup.Value();

            auto const temporary = SavePatchTemporaryRadio().IsChecked();

            if (temporary && temporary.Value())
            {
                // A patch that was on disk and is now temporary has to leave disk, or it would
                // come back on the next start having been asked not to.
                if (!it->FilePath.empty())
                {
                    patchbay::PatchStore::Current().Delete(*it);
                    it->FilePath.clear();
                }

                it->IsTemporary = true;

                ShowStatus(resources::GetString(L"StatusPatchTemporary"), controls::InfoBarSeverity::Informational);
            }
            else
            {
                if (!patchbay::PatchStore::Current().Save(*it))
                {
                    ShowStatus(patchbay::PatchStore::Current().LastErrorMessage(), controls::InfoBarSeverity::Error);
                    co_return;
                }

                ShowStatus(resources::FormatString(L"StatusPatchSavedFormat", it->Name),
                    controls::InfoBarSeverity::Success);
            }

            auto const newKey = PatchKey(*it);

            if (m_routingPatchKeys.erase(previousKey) > 0)
            {
                m_routingPatchKeys.insert(newKey);
            }
            else if (it->ActivateAtStartup)
            {
                m_routingPatchKeys.insert(newKey);
            }

            m_currentPatchKey = newKey;
            m_unsavedPatchKeys.erase(previousKey);
            m_unsavedPatchKeys.erase(newKey);

            UpdatePatchHeader();

            RebuildNavigation();
            ApplyRouting();
            UpdateTray();
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to save the patch.")
    }

    // -------------------------------------------------------------- patch menu

    _Use_decl_annotations_
    void MainWindow::OnPatchMenuClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(args);

        try
        {
            auto const anchor = sender.try_as<xaml::FrameworkElement>();

            if (anchor == nullptr)
            {
                return;
            }

            auto* patch = CurrentPatch();

            if (patch == nullptr)
            {
                return;
            }

            auto const key = PatchKey(*patch);
            auto weak = get_weak();

            controls::MenuFlyout menu{};

            controls::ToggleMenuFlyoutItem routingItem{};
            routingItem.Text(resources::GetString(L"MenuRouteThisPatch"));
            routingItem.IsChecked(m_routingPatchKeys.count(key) != 0);

            routingItem.Click([weak, key](foundation::IInspectable const& s, auto&&)
                {
                    if (auto strong = weak.get())
                    {
                        auto const item = s.try_as<controls::ToggleMenuFlyoutItem>();
                        strong->SetPatchRouting(key, item != nullptr && item.IsChecked());
                    }
                });

            menu.Items().Append(routingItem);

            controls::MenuFlyoutSeparator separator{};
            menu.Items().Append(separator);

            controls::MenuFlyoutItem renameItem{};
            renameItem.Text(resources::GetString(L"MenuRenameAndSave"));
            renameItem.Click([weak](auto&&, auto&&)
                {
                    if (auto strong = weak.get())
                    {
                        strong->ShowSavePatchDialogAsync();
                    }
                });
            menu.Items().Append(renameItem);

            controls::MenuFlyoutItem deleteItem{};
            deleteItem.Text(resources::GetString(L"MenuDeletePatch"));
            deleteItem.Click([weak](auto&&, auto&&)
                {
                    if (auto strong = weak.get())
                    {
                        strong->ShowDeletePatchDialogAsync();
                    }
                });
            menu.Items().Append(deleteItem);

            menu.ShowAt(anchor);
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to show the patch menu.")
    }

    winrt::fire_and_forget MainWindow::ShowDeletePatchDialogAsync()
    {
        auto strong = get_strong();

        try
        {
            auto* patch = CurrentPatch();

            if (patch == nullptr)
            {
                co_return;
            }

            auto const key = PatchKey(*patch);

            ConfirmDeleteText().Text(resources::FormatString(L"DeletePatchMessageFormat", patch->Name));
            ConfirmDeleteDialog().XamlRoot(Content().XamlRoot());

            auto const result = co_await ConfirmDeleteDialog().ShowAsync();

            if (result != controls::ContentDialogResult::Primary)
            {
                co_return;
            }

            auto it = std::find_if(m_patches.begin(), m_patches.end(),
                [&key, this](patchbay::PatchDocument& p) { return PatchKey(p) == key; });

            if (it == m_patches.end())
            {
                co_return;
            }

            if (!it->FilePath.empty() && !patchbay::PatchStore::Current().Delete(*it))
            {
                ShowStatus(patchbay::PatchStore::Current().LastErrorMessage(), controls::InfoBarSeverity::Error);
                co_return;
            }

            m_routingPatchKeys.erase(key);
            m_patches.erase(it);
            m_currentPatchKey.clear();

            RebuildNavigation();

            if (!m_patches.empty())
            {
                SelectPatch(PatchKey(m_patches.front()));
            }
            else
            {
                RebuildCanvas();
                RefreshInspector();
            }

            ApplyRouting();
            UpdateTray();
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to delete the patch.")
    }

    // ------------------------------------------------------- endpoint palette

    _Use_decl_annotations_
    void MainWindow::OnAddEndpointClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(args);

        if (auto const anchor = sender.try_as<xaml::FrameworkElement>())
        {
            ShowEndpointPalette(anchor);
        }
    }

    std::vector<patchbay::PatchEndpoint> MainWindow::RememberedEndpoints() const noexcept
    {
        std::vector<patchbay::PatchEndpoint> remembered{};

        try
        {
            auto& catalog = patchbay::EndpointCatalog::Current();

            for (auto const& patch : m_patches)
            {
                for (auto const& endpoint : patch.Endpoints)
                {
                    if (catalog.Resolve(endpoint).has_value())
                    {
                        continue;
                    }

                    auto const already = std::any_of(remembered.begin(), remembered.end(),
                        [&endpoint](patchbay::PatchEndpoint const& e)
                        {
                            return ::CompareStringOrdinal(
                                e.Match.EndpointDeviceId.c_str(), -1,
                                endpoint.Match.EndpointDeviceId.c_str(), -1, TRUE) == CSTR_EQUAL;
                        });

                    if (!already)
                    {
                        remembered.push_back(endpoint);
                    }
                }
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to collect remembered endpoints.")

        return remembered;
    }

    _Use_decl_annotations_
    void MainWindow::ShowEndpointPalette(xaml::FrameworkElement const& anchor) noexcept
    {
        try
        {
            auto* patch = CurrentPatch();

            if (patch == nullptr)
            {
                ShowStatus(resources::GetString(L"StatusNoPatchOpen"), controls::InfoBarSeverity::Informational);
                return;
            }

            controls::MenuFlyout menu{};
            auto weak = get_weak();

            auto const alreadyOnCanvas = [patch](std::wstring const& endpointDeviceId)
                {
                    return std::any_of(patch->Endpoints.begin(), patch->Endpoints.end(),
                        [&endpointDeviceId](patchbay::PatchEndpoint const& e)
                        {
                            return ::CompareStringOrdinal(
                                e.Match.EndpointDeviceId.c_str(), -1,
                                endpointDeviceId.c_str(), -1, TRUE) == CSTR_EQUAL;
                        });
                };

            size_t added{ 0 };

            for (auto const& endpoint : m_liveEndpoints)
            {
                if (alreadyOnCanvas(endpoint.EndpointDeviceId))
                {
                    continue;
                }

                controls::MenuFlyoutItem item{};

                item.Text(winrt::hstring{ endpoint.Name });

                auto const endpointDeviceId = endpoint.EndpointDeviceId;

                item.Click([weak, endpointDeviceId](auto&&, auto&&)
                    {
                        if (auto strong = weak.get())
                        {
                            if (auto const live = patchbay::EndpointCatalog::Current().Find(endpointDeviceId))
                            {
                                strong->AddEndpointToPatch(live.value());
                            }
                        }
                    });

                menu.Items().Append(item);
                added++;
            }

            if (added == 0)
            {
                controls::MenuFlyoutItem empty{};
                empty.Text(resources::GetString(L"PaletteNothingToAdd"));
                empty.IsEnabled(false);
                menu.Items().Append(empty);
            }

            // Devices the customer has used before but that are not here now, derived from the
            // saved patches rather than a list of their own.
            auto const remembered = RememberedEndpoints();

            std::vector<patchbay::PatchEndpoint> offerable{};

            for (auto const& endpoint : remembered)
            {
                if (!alreadyOnCanvas(endpoint.Match.EndpointDeviceId))
                {
                    offerable.push_back(endpoint);
                }
            }

            if (!offerable.empty())
            {
                controls::MenuFlyoutSeparator separator{};
                menu.Items().Append(separator);

                controls::MenuFlyoutItem header{};
                header.Text(resources::GetString(L"PaletteSeenBefore"));
                header.IsEnabled(false);
                menu.Items().Append(header);

                for (auto const& endpoint : offerable)
                {
                    controls::MenuFlyoutItem item{};

                    item.Text(winrt::hstring{ endpoint.DisplayName });

                    auto const copy = endpoint;

                    item.Click([weak, copy](auto&&, auto&&)
                        {
                            if (auto strong = weak.get())
                            {
                                strong->AddRememberedEndpointToPatch(copy);
                            }
                        });

                    menu.Items().Append(item);
                }
            }

            menu.ShowAt(anchor);
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to show the endpoint palette.")
    }

    _Use_decl_annotations_
    void MainWindow::AddEndpointToPatch(patchbay::LiveEndpoint const& endpoint) noexcept
    {
        try
        {
            auto* patch = CurrentPatch();

            if (patch == nullptr)
            {
                return;
            }

            if (patch->Endpoints.size() >= patchbay::MaximumEndpointsPerPatch)
            {
                ShowStatus(resources::GetString(L"StatusEndpointLimit"), controls::InfoBarSeverity::Warning);
                return;
            }

            patchbay::PatchEndpoint added{};

            added.Id = patchbay::PatchDocument::NewId();
            added.DisplayName = endpoint.Name;
            added.TransportCode = endpoint.TransportCode;
            added.Match = endpoint.BuildMatch();
            added.MatchMode = patchbay::EndpointMatchMode::EndpointDeviceId;

            PlaceNewNode(added, patch->Endpoints.size());

            patch->Endpoints.push_back(std::move(added));

            MarkDirty();
            RefreshAnalysis();
            RebuildCanvas();
            UpdateMessages();
            ApplyRouting();

            // a node dropped outside the viewport looks like nothing happened
            m_canvas.FitToContent();
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to add the endpoint.")
    }

    _Use_decl_annotations_
    void MainWindow::AddRememberedEndpointToPatch(patchbay::PatchEndpoint const& remembered) noexcept
    {
        try
        {
            auto* patch = CurrentPatch();

            if (patch == nullptr)
            {
                return;
            }

            if (patch->Endpoints.size() >= patchbay::MaximumEndpointsPerPatch)
            {
                ShowStatus(resources::GetString(L"StatusEndpointLimit"), controls::InfoBarSeverity::Warning);
                return;
            }

            auto added = remembered;

            // a new identity within this patch; everything else about it is carried over
            added.Id = patchbay::PatchDocument::NewId();

            PlaceNewNode(added, patch->Endpoints.size());

            patch->Endpoints.push_back(std::move(added));

            MarkDirty();
            RefreshAnalysis();
            RebuildCanvas();
            UpdateMessages();
            ApplyRouting();

            m_canvas.FitToContent();
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to add the remembered endpoint.")
    }

    // ------------------------------------------------------- create a loopback

    _Use_decl_annotations_
    void MainWindow::OnCreateLoopbackClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        ShowCreateLoopbackDialogAsync();
    }

    _Use_decl_annotations_
    void MainWindow::OnLoopbackNameChanged(
        foundation::IInspectable const& sender,
        controls::TextChangedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        try
        {
            CreateLoopbackDialog().IsPrimaryButtonEnabled(
                !patchbay::SanitizeStoredString(std::wstring{ LoopbackNameBox().Text() }).empty());
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to validate the loopback name.")
    }

    winrt::fire_and_forget MainWindow::ShowCreateLoopbackDialogAsync()
    {
        auto strong = get_strong();

        try
        {
            LoopbackNameBox().Text(resources::GetString(L"LoopbackDefaultName"));
            LoopbackBasicRadio().IsChecked(true);
            LoopbackKeepCheck().IsChecked(true);
            LoopbackErrorText().Visibility(xaml::Visibility::Collapsed);
            CreateLoopbackDialog().IsPrimaryButtonEnabled(true);
            CreateLoopbackDialog().XamlRoot(Content().XamlRoot());

            auto const result = co_await CreateLoopbackDialog().ShowAsync();

            if (result != controls::ContentDialogResult::Primary)
            {
                co_return;
            }

            auto const name = patchbay::SanitizeStoredString(std::wstring{ LoopbackNameBox().Text() });

            if (name.empty())
            {
                co_return;
            }

            auto const basicChecked = LoopbackBasicRadio().IsChecked();
            auto const isBasic = basicChecked && basicChecked.Value();

            auto const keepChecked = LoopbackKeepCheck().IsChecked();
            auto const keep = keepChecked && keepChecked.Value();

            bool created{ false };
            winrt::hstring error{};

            // Creating a loopback talks to the service, so it happens off the UI thread. It goes
            // through the shipped Windows MIDI Services API, exactly as MIDI Loopback Setup does.
            co_await patchbay::RunOnBackgroundAsync([&created, &error, name, isBasic, keep]()
                {
                    try
                    {
                        if (isBasic)
                        {
                            if (!midi2bloop::MidiBasicLoopbackManager::IsTransportAvailable())
                            {
                                error = resources::GetString(L"ErrorBasicLoopbackUnavailable");
                                return;
                            }

                            midi2bloop::MidiBasicLoopbackEndpointDefinition definition{ winrt::hstring{ name } };

                            midi2bloop::MidiBasicLoopbackCreationConfig config{ definition };

                            auto const response = midi2bloop::MidiBasicLoopbackManager::CreateTransientLoopback(config);

                            created = response != nullptr && response.Success();

                            if (created && keep)
                            {
                                midi2config::MidiServiceTransportPluginConfigManager::EnsureConfigurationFile();
                                midi2config::MidiServiceTransportPluginConfigManager::SaveUpdate(config);
                            }
                        }
                        else
                        {
                            if (!midi2loop::MidiLoopbackManager::IsTransportAvailable())
                            {
                                error = resources::GetString(L"ErrorLoopbackUnavailable");
                                return;
                            }

                            midi2loop::MidiLoopbackEndpointDefinition definitionA{
                                resources::FormatString(L"LoopbackSideAFormat", name) };
                            midi2loop::MidiLoopbackEndpointDefinition definitionB{
                                resources::FormatString(L"LoopbackSideBFormat", name) };

                            midi2loop::MidiLoopbackCreationConfig config{ definitionA, definitionB };

                            auto const response = midi2loop::MidiLoopbackManager::CreateTransientLoopback(config);

                            created = response != nullptr && response.Success();

                            if (created && keep)
                            {
                                midi2config::MidiServiceTransportPluginConfigManager::EnsureConfigurationFile();
                                midi2config::MidiServiceTransportPluginConfigManager::SaveUpdate(config);
                            }
                        }
                    }
                    catch (...)
                    {
                        created = false;
                    }
                });

            if (!created)
            {
                ShowStatus(error.empty() ? resources::GetString(L"ErrorLoopbackCreateFailed") : error,
                    controls::InfoBarSeverity::Error);
                co_return;
            }

            // The new endpoint has to exist before it can be dropped on the canvas, and the
            // watcher notification can lag the creation call.
            co_await patchbay::RunOnBackgroundAsync([]()
                {
                    patchbay::EndpointCatalog::Current().Refresh();
                });

            m_liveEndpoints = patchbay::EndpointCatalog::Current().Snapshot();

            for (auto const& endpoint : m_liveEndpoints)
            {
                auto const matches = isBasic
                    ? endpoint.Name == name
                    : endpoint.Name == std::wstring{ resources::FormatString(L"LoopbackSideAFormat", name) };

                if (matches)
                {
                    AddEndpointToPatch(endpoint);
                    break;
                }
            }

            ShowStatus(resources::FormatString(L"StatusLoopbackCreatedFormat", name),
                controls::InfoBarSeverity::Success);

            RebuildCanvas();
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to create the loopback.")
    }

    // --------------------------------------------- remove what is selected

    void MainWindow::DeleteSelection() noexcept
    {
        DeleteSelectionAsync();
    }

    winrt::fire_and_forget MainWindow::DeleteSelectionAsync()
    {
        auto strong = get_strong();

        try
        {
            auto const kind = m_canvas.SelectionKind();

            if (kind == patchbay::CanvasSelectionKind::None)
            {
                co_return;
            }

            auto const endpointId = m_canvas.SelectedEndpointId();
            auto const connectionId = m_canvas.SelectedConnectionId();

            auto* patch = CurrentPatch();

            if (patch == nullptr)
            {
                co_return;
            }

            winrt::hstring what{};

            if (kind == patchbay::CanvasSelectionKind::Endpoint)
            {
                auto const* endpoint = patch->FindEndpoint(endpointId);

                if (endpoint == nullptr)
                {
                    co_return;
                }

                what = resources::FormatString(L"RemoveEndpointMessageFormat", endpoint->DisplayName);
            }
            else
            {
                if (patch->FindConnection(connectionId) == nullptr)
                {
                    co_return;
                }

                what = resources::GetString(L"RemoveConnectionMessage");
            }

            if (patchbay::AppSettings::Current().ConfirmCanvasRemove())
            {
                ConfirmRemoveText().Text(what);
                ConfirmRemoveSkipCheck().IsChecked(false);
                ConfirmRemoveDialog().XamlRoot(Content().XamlRoot());

                auto const result = co_await ConfirmRemoveDialog().ShowAsync();

                if (result != controls::ContentDialogResult::Primary)
                {
                    co_return;
                }

                auto const skip = ConfirmRemoveSkipCheck().IsChecked();

                if (skip && skip.Value())
                {
                    patchbay::AppSettings::Current().ConfirmCanvasRemove(false);
                }
            }

            // Re-fetched: the dialog gave the customer time to switch patches.
            patch = CurrentPatch();

            if (patch == nullptr)
            {
                co_return;
            }

            if (kind == patchbay::CanvasSelectionKind::Endpoint)
            {
                patch->RemoveEndpoint(endpointId);
            }
            else
            {
                patch->RemoveConnection(connectionId);
            }

            m_canvas.ClearSelection();

            MarkDirty();
            RefreshAnalysis();
            RebuildCanvas();
            RefreshInspector();
            UpdateMessages();
            ApplyRouting();
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to remove the selection.")
    }

    // ------------------------------------------------------------ quick patch


    _Use_decl_annotations_
    void MainWindow::OnNewQuickPatchClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        ShowQuickPatchDialogAsync();
    }

    _Use_decl_annotations_
    void MainWindow::OnQuickPatchSourceChanged(
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
    void MainWindow::OnQuickPatchDestinationChanged(
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
    void MainWindow::OnQuickPatchGroupChanged(
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
    void MainWindow::FillQuickPatchGroups(bool isSource) noexcept
    {
        try
        {
            m_fillingQuickPatch = true;

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
                    items.Append(winrt::box_value(resources::GetString(L"PortAllGroups")));
                    continue;
                }

                items.Append(winrt::box_value(patchbay::DescribeGroupIndex(
                    group, live.has_value() ? live->PortName(group, isSource) : std::wstring{})));
            }

            groupCombo.ItemsSource(items);
            groupCombo.SelectedIndex(0);
            groupCombo.IsEnabled(groups.size() > 1);

            m_fillingQuickPatch = false;
        }
        catch (...)
        {
            m_fillingQuickPatch = false;
            MIDI_PATCHBAY_LOG_GENERAL_EXCEPTION(L"Unable to list the groups for the quick patch.");
        }
    }

    void MainWindow::ValidateQuickPatch() noexcept
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
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to validate the quick patch.")
    }

    winrt::fire_and_forget MainWindow::ShowQuickPatchDialogAsync()
    {
        auto strong = get_strong();

        try
        {
            m_liveEndpoints = patchbay::EndpointCatalog::Current().Snapshot();

            if (m_liveEndpoints.empty())
            {
                ShowStatus(resources::GetString(L"QuickPatchNoEndpoints"), controls::InfoBarSeverity::Warning);
                co_return;
            }

            m_fillingQuickPatch = true;

            m_quickSourceIds.clear();
            m_quickDestinationIds.clear();

            auto sourceItems = winrt::single_threaded_vector<foundation::IInspectable>();
            auto destinationItems = winrt::single_threaded_vector<foundation::IInspectable>();

            for (auto const& endpoint : m_liveEndpoints)
            {
                m_quickSourceIds.push_back(endpoint.EndpointDeviceId);
                m_quickDestinationIds.push_back(endpoint.EndpointDeviceId);

                sourceItems.Append(winrt::box_value(winrt::hstring{ endpoint.Name }));
                destinationItems.Append(winrt::box_value(winrt::hstring{ endpoint.Name }));
            }

            QuickPatchSourceCombo().ItemsSource(sourceItems);
            QuickPatchDestinationCombo().ItemsSource(destinationItems);

            QuickPatchSourceCombo().SelectedIndex(0);
            QuickPatchDestinationCombo().SelectedIndex(m_liveEndpoints.size() > 1 ? 1 : 0);

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

    void MainWindow::CreateQuickPatch() noexcept
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

            CreateNewPatch(std::wstring{
                resources::FormatString(L"QuickPatchNameFormat", source->Name, destination->Name) });

            auto* patch = CurrentPatch();

            if (patch == nullptr)
            {
                return;
            }

            auto const addNode = [&patch](patchbay::LiveEndpoint const& endpoint)
                {
                    patchbay::PatchEndpoint node{};

                    node.Id = patchbay::PatchDocument::NewId();
                    node.DisplayName = endpoint.Name;
                    node.TransportCode = endpoint.TransportCode;
                    node.Match = endpoint.BuildMatch();
                    node.MatchMode = patchbay::EndpointMatchMode::EndpointDeviceId;

                    PlaceNewNode(node, patch->Endpoints.size());

                    auto const id = node.Id;

                    patch->Endpoints.push_back(std::move(node));

                    return id;
                };

            auto const sourceNodeId = addNode(source.value());

            // One endpoint routed across its own groups needs one node, not two stacked copies.
            auto const destinationNodeId = source->EndpointDeviceId == destination->EndpointDeviceId
                ? sourceNodeId : addNode(destination.value());

            patchbay::PatchConnection connection{};

            connection.Id = patchbay::PatchDocument::NewId();
            connection.SourceEndpointId = sourceNodeId;
            connection.SourceGroupIndex = sourceGroup;
            connection.DestinationEndpointId = destinationNodeId;
            connection.DestinationGroupIndex = destinationGroup;

            patch->Connections.push_back(connection);

            MarkDirty();
            RefreshAnalysis();
            RebuildCanvas();
            ApplyRouting();
            UpdateMessages();

            m_canvas.Select(patchbay::CanvasSelectionKind::Connection, connection.Id);

            // Selecting opens the details panel, which takes its width from the canvas. Fitting
            // before that lands leaves the second node half off the right edge.
            RootGrid().UpdateLayout();

            m_canvas.FitToContent();
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to create the quick patch.")
    }

    // ---------------------------------------------------------- test and menus

    _Use_decl_annotations_
    void MainWindow::OnTestClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(args);

        if (auto const anchor = sender.try_as<xaml::FrameworkElement>())
        {
            ShowTestMenu(anchor);
        }
    }

    _Use_decl_annotations_
    void MainWindow::ShowTestMenu(xaml::FrameworkElement const& anchor) noexcept
    {
        try
        {
            auto* patch = CurrentPatch();

            if (patch == nullptr || patch->Endpoints.empty())
            {
                ShowStatus(resources::GetString(L"StatusNothingToTest"), controls::InfoBarSeverity::Informational);
                return;
            }

            controls::MenuFlyout menu{};
            auto weak = get_weak();

            for (auto const& endpoint : patch->Endpoints)
            {
                auto const live = patchbay::EndpointCatalog::Current().Resolve(endpoint);

                if (!live.has_value())
                {
                    continue;
                }

                controls::MenuFlyoutSubItem submenu{};
                submenu.Text(winrt::hstring{ endpoint.DisplayName });

                auto const deviceId = live->EndpointDeviceId;

                auto const addTool = [&submenu, weak, deviceId](winrt::hstring const& text, std::wstring const& exe)
                    {
                        controls::MenuFlyoutItem item{};

                        item.Text(text);
                        item.Click([weak, deviceId, exe](auto&&, auto&&)
                            {
                                if (auto strong = weak.get())
                                {
                                    strong->LaunchTool(exe, deviceId);
                                }
                            });

                        submenu.Items().Append(item);
                    };

                addTool(resources::GetString(L"TestOpenMonitor"), MonitorExeName);
                addTool(resources::GetString(L"TestOpenKeyboard"), KeyboardExeName);
                addTool(resources::GetString(L"TestOpenScratchPad"), ScratchPadExeName);

                menu.Items().Append(submenu);
            }

            if (menu.Items().Size() == 0)
            {
                controls::MenuFlyoutItem empty{};
                empty.Text(resources::GetString(L"StatusNothingToTest"));
                empty.IsEnabled(false);
                menu.Items().Append(empty);
            }

            menu.ShowAt(anchor);
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to show the test menu.")
    }

    _Use_decl_annotations_
    void MainWindow::ShowEndpointMenu(std::wstring const& endpointId, foundation::Point const& position) noexcept
    {
        try
        {
            auto* patch = CurrentPatch();

            if (patch == nullptr)
            {
                return;
            }

            auto const* endpoint = patch->FindEndpoint(endpointId);

            if (endpoint == nullptr)
            {
                return;
            }

            m_canvas.Select(patchbay::CanvasSelectionKind::Endpoint, endpointId);

            controls::MenuFlyout menu{};
            auto weak = get_weak();

            auto const live = patchbay::EndpointCatalog::Current().Resolve(*endpoint);

            if (live.has_value())
            {
                auto const deviceId = live->EndpointDeviceId;

                auto const addTool = [&menu, weak, deviceId](winrt::hstring const& text, std::wstring const& exe)
                    {
                        controls::MenuFlyoutItem item{};

                        item.Text(text);
                        item.Click([weak, deviceId, exe](auto&&, auto&&)
                            {
                                if (auto strong = weak.get())
                                {
                                    strong->LaunchTool(exe, deviceId);
                                }
                            });

                        menu.Items().Append(item);
                    };

                addTool(resources::GetString(L"TestOpenMonitor"), MonitorExeName);
                addTool(resources::GetString(L"TestOpenKeyboard"), KeyboardExeName);
                addTool(resources::GetString(L"TestOpenScratchPad"), ScratchPadExeName);

                controls::MenuFlyoutSeparator separator{};
                menu.Items().Append(separator);
            }

            controls::MenuFlyoutItem groupsItem{};
            groupsItem.Text(endpoint->ShowAllGroups
                ? resources::GetString(L"ActionShowDeclaredGroups")
                : resources::GetString(L"ActionShowAllGroups"));

            groupsItem.Click([weak, endpointId](auto&&, auto&&)
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

            menu.Items().Append(groupsItem);

            controls::MenuFlyoutItem removeItem{};
            removeItem.Text(resources::GetString(L"ActionRemoveEndpoint"));

            removeItem.Click([weak, endpointId](auto&&, auto&&)
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

            menu.Items().Append(removeItem);

            primitives::FlyoutShowOptions options{};

            options.Position(position);
            options.Placement(primitives::FlyoutPlacementMode::BottomEdgeAlignedLeft);

            menu.ShowAt(CanvasScroller(), options);
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to show the endpoint menu.")
    }

    _Use_decl_annotations_
    void MainWindow::LaunchTool(std::wstring const& toolExeName, std::wstring const& endpointDeviceId) noexcept
    {
        try
        {
            auto const folder = ExecutableFolder();

            if (folder.empty())
            {
                return;
            }

            // Installed layout first (each tool has its own folder under Tools), then the dev
            // build layout, then a sibling of this executable.
            std::vector<std::filesystem::path> candidates{};

            auto const toolsRoot = std::filesystem::path{ folder }.parent_path();
            auto const stem = std::filesystem::path{ toolExeName }.stem().wstring();

            for (auto const& entry : { L"Monitor", L"Keyboard", L"ScratchPad" })
            {
                candidates.push_back(toolsRoot / entry / toolExeName);
            }

            // dev build: ...\out\<tool>\<platform>\<configuration>\<tool>.exe
            auto const devRoot = std::filesystem::path{ folder }.parent_path().parent_path().parent_path();

            candidates.push_back(devRoot / stem / std::filesystem::path{ folder }.parent_path().filename()
                / std::filesystem::path{ folder }.filename() / toolExeName);

            candidates.push_back(std::filesystem::path{ folder } / toolExeName);

            for (auto const& candidate : candidates)
            {
                std::error_code ec{};

                if (!std::filesystem::exists(candidate, ec))
                {
                    continue;
                }

                auto const arguments = L'"' + endpointDeviceId + L'"';
                auto const path = candidate.wstring();

                SHELLEXECUTEINFOW info{};

                info.cbSize = sizeof(info);
                info.fMask = SEE_MASK_NOASYNC;
                info.lpVerb = L"open";
                info.lpFile = path.c_str();
                info.lpParameters = arguments.c_str();
                info.nShow = SW_SHOWNORMAL;

                if (::ShellExecuteExW(&info))
                {
                    return;
                }
            }

            ShowStatus(resources::FormatString(L"ErrorToolNotFoundFormat", toolExeName),
                controls::InfoBarSeverity::Warning);
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to launch a tool.")
    }
}
