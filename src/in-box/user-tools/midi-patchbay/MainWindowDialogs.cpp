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
#include "TextMatch.h"

using namespace winrt::Microsoft::UI::Xaml;

namespace patchbay = ::midipatchbay;
namespace resources = ::midipatchbay::resources;

namespace winrt::midipatchbay::implementation
{
    namespace
    {
        // Where a new endpoint goes relative to the point it is put at, so it lands under the
        // pointer rather than hanging off it.
        constexpr double EndpointHalfWidth = patchbay::PatchCanvas::MinimumNodeWidth / 2;
        constexpr double EndpointHalfHeight = 40.0;

        // The other tools in this family all take an endpoint device id on the command line,
        // which is what makes testing a route a launch rather than a feature to rebuild here.
        constexpr wchar_t MonitorExeName[] = L"midi2monitor.exe";
        constexpr wchar_t KeyboardExeName[] = L"midikeyboard.exe";
        constexpr wchar_t ScratchPadExeName[] = L"midiscratchpad.exe";

        using patchbay::SameText;

        std::wstring ExecutableFolder() noexcept
        {
            try
            {
                std::wstring buffer(MAX_PATH, L'\0');

                auto const length = ::GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));

                if (length == 0 || length >= buffer.size())
                {
                    return {};
                }

                buffer.resize(length);

                return std::filesystem::path{ buffer }.parent_path().wstring();
            }
            catch (...)
            {
            }

            return {};
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

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::ShowSavePatchDialogAsync(bool startAutomatically)
    {
        auto strong = get_strong();

        try
        {
            auto* patch = CurrentPatch();

            if (patch == nullptr)
            {
                co_return;
            }

            auto const key = m_patchKey;

            SavePatchNameBox().Text(winrt::hstring{ patch->Name });
            SavePatchDescriptionBox().Text(winrt::hstring{ patch->Description });
            SavePatchKeepRadio().IsChecked(!patch->IsTemporary || patch->FilePath.empty());
            SavePatchTemporaryRadio().IsChecked(false);
            SavePatchStartupCheck().IsChecked(startAutomatically || patch->ActivateAtStartup);
            SavePatchDialog().IsPrimaryButtonEnabled(!patch->Name.empty());
            SavePatchDialog().XamlRoot(Content().XamlRoot());

            auto const result = co_await SavePatchDialog().ShowAsync();

            if (result != controls::ContentDialogResult::Primary)
            {
                // Turns the Start automatically switch back off if it opened this dialog.
                UpdatePatchHeader();
                co_return;
            }

            // Looked up again: the patch can go while the dialog is open.
            auto& library = patchbay::PatchLibrary::Current();

            patch = library.Find(key);

            if (patch == nullptr)
            {
                co_return;
            }

            auto const name = patchbay::SanitizeStoredString(std::wstring{ SavePatchNameBox().Text() });

            if (name.empty())
            {
                co_return;
            }

            patch->Name = name;
            patch->Description = patchbay::SanitizeStoredString(std::wstring{ SavePatchDescriptionBox().Text() });

            auto const startup = SavePatchStartupCheck().IsChecked();
            patch->ActivateAtStartup = startup && startup.Value();

            auto const temporary = SavePatchTemporaryRadio().IsChecked();

            if (temporary && temporary.Value())
            {
                // A patch that was on disk and is now temporary has to leave disk, or it would
                // come back on the next start having been asked not to.
                if (!patch->FilePath.empty())
                {
                    patchbay::PatchStore::Current().Delete(*patch);
                    patch->FilePath.clear();
                }

                patch->IsTemporary = true;

                // Not an edit Undo takes back, but the library and its tiles need the new name.
                m_committing = true;
                auto const reset = wil::scope_exit([this]() { m_committing = false; });

                library.Changed(key, false);

                ShowStatus(resources::GetString(L"StatusPatchTemporary"), controls::InfoBarSeverity::Informational);
            }
            else
            {
                if (!library.Save(key))
                {
                    ShowStatus(library.LastErrorMessage(), controls::InfoBarSeverity::Error);
                    co_return;
                }

                ShowStatus(resources::FormatString(L"StatusPatchSavedFormat", name),
                    controls::InfoBarSeverity::Success);
            }

            // A patch set to start by itself starts now, the way it would when the app starts.
            if (auto const* saved = library.Find(key); saved != nullptr && saved->ActivateAtStartup)
            {
                library.SetRouting(key, true);
            }

            UpdatePatchHeader();
            UpdateTitle();
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

            auto const key = m_patchKey;
            auto weak = get_weak();

            controls::MenuFlyout menu{};

            controls::ToggleMenuFlyoutItem routingItem{};
            routingItem.Text(resources::GetString(L"MenuRouteThisPatch"));
            routingItem.IsChecked(patchbay::PatchLibrary::Current().IsRouting(key));

            routingItem.Click([key](foundation::IInspectable const& s, auto&&)
                {
                    auto const item = s.try_as<controls::ToggleMenuFlyoutItem>();
                    patchbay::PatchLibrary::Current().SetRouting(key, item != nullptr && item.IsChecked());
                });

            menu.Items().Append(routingItem);

            // A patch setting rather than a connection setting, because it is how Patchbay
            // connects to each device
            controls::ToggleMenuFlyoutItem waitItem{};
            waitItem.Text(resources::GetString(L"MenuWaitForSendComplete"));
            waitItem.IsChecked(patch->WaitForSendComplete);
            controls::ToolTipService::SetToolTip(waitItem,
                winrt::box_value(resources::GetString(L"MenuWaitForSendCompleteTip")));

            waitItem.Click([weak, key](foundation::IInspectable const& s, auto&&)
                {
                    auto strong = weak.get();

                    if (strong == nullptr)
                    {
                        return;
                    }

                    auto const item = s.try_as<controls::ToggleMenuFlyoutItem>();
                    auto* current = strong->CurrentPatch();

                    // The menu belongs to the patch that was showing when it opened
                    if (item == nullptr || current == nullptr || strong->m_patchKey != key ||
                        current->WaitForSendComplete == item.IsChecked())
                    {
                        return;
                    }

                    current->WaitForSendComplete = item.IsChecked();

                    strong->CommitChange(true, false);
                });

            menu.Items().Append(waitItem);

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

            auto const key = m_patchKey;

            ConfirmDeleteText().Text(resources::FormatString(L"DeletePatchMessageFormat", patch->Name));
            ConfirmDeleteDialog().XamlRoot(Content().XamlRoot());

            auto const result = co_await ConfirmDeleteDialog().ShowAsync();

            if (result != controls::ContentDialogResult::Primary)
            {
                co_return;
            }

            // The library tells every window, and this one closes when it hears.
            auto& library = patchbay::PatchLibrary::Current();

            if (!library.Remove(key))
            {
                ShowStatus(library.LastErrorMessage(), controls::InfoBarSeverity::Error);
            }
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

    std::vector<patchbay::PatchEndpoint> MainWindow::RememberedEndpoints() noexcept
    {
        std::vector<patchbay::PatchEndpoint> remembered{};

        try
        {
            for (auto const* patch : patchbay::PatchLibrary::Current().Patches())
            {
                for (auto const& endpoint : patch->Endpoints)
                {
                    // Without a device ID there is no telling one of these from another, so it
                    // can't be offered by itself. It still shows on its own patch.
                    if (endpoint.Match.EndpointDeviceId.empty() || patchbay::ResolveEndpoint(endpoint).has_value())
                    {
                        continue;
                    }

                    auto const already = std::any_of(remembered.begin(), remembered.end(),
                        [&endpoint](patchbay::PatchEndpoint const& e)
                        { return SameText(e.Match.EndpointDeviceId, endpoint.Match.EndpointDeviceId); });

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
    bool MainWindow::IsOnCanvas(std::wstring const& endpointDeviceId) noexcept
    {
        auto const* patch = CurrentPatch();

        return patch != nullptr && std::any_of(patch->Endpoints.begin(), patch->Endpoints.end(),
            [&endpointDeviceId](patchbay::PatchEndpoint const& e) { return patchbay::StandsFor(e, endpointDeviceId); });
    }

    _Use_decl_annotations_
    bool MainWindow::IsOnCanvas(patchbay::PatchEndpoint const& endpoint) noexcept
    {
        auto const* patch = CurrentPatch();

        return patch != nullptr && std::any_of(patch->Endpoints.begin(), patch->Endpoints.end(),
            [&endpoint](patchbay::PatchEndpoint const& e) { return patchbay::IsSameDevice(e, endpoint); });
    }

    _Use_decl_annotations_
    void MainWindow::ShowEndpointPalette(xaml::FrameworkElement const& anchor) noexcept
    {
        try
        {
            if (CurrentPatch() == nullptr)
            {
                return;
            }

            controls::MenuFlyout menu{};
            auto weak = get_weak();

            size_t added{ 0 };

            for (auto const& endpoint : patchbay::PatchLibrary::Current().LiveEndpoints())
            {
                if (IsOnCanvas(endpoint.EndpointDeviceId))
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
            std::vector<patchbay::PatchEndpoint> offerable{};

            for (auto const& endpoint : RememberedEndpoints())
            {
                if (!IsOnCanvas(endpoint))
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
    void MainWindow::AddEndpointToPatch(
        patchbay::LiveEndpoint const& endpoint,
        std::optional<foundation::Point> const& center) noexcept
    {
        try
        {
            patchbay::PatchEndpoint added{};

            added.DisplayName = endpoint.Name;
            added.TransportCode = endpoint.TransportCode;
            added.Match = endpoint.BuildMatch();
            added.MatchMode = patchbay::EndpointMatchMode::EndpointDeviceId;

            AddRememberedEndpointToPatch(added, center);
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to add the endpoint.")
    }

    _Use_decl_annotations_
    void MainWindow::AddRememberedEndpointToPatch(
        patchbay::PatchEndpoint const& remembered,
        std::optional<foundation::Point> const& center) noexcept
    {
        try
        {
            auto* patch = CurrentPatch();

            if (patch == nullptr)
            {
                return;
            }

            for (auto const& endpoint : patch->Endpoints)
            {
                if (patchbay::IsSameDevice(endpoint, remembered))
                {
                    m_canvas.Select(patchbay::CanvasSelectionKind::Endpoint, endpoint.Id);
                    ShowStatus(resources::GetString(L"StatusEndpointAlreadyHere"), controls::InfoBarSeverity::Informational);
                    return;
                }
            }

            if (patch->Endpoints.size() >= patchbay::MaximumEndpointsPerPatch)
            {
                ShowStatus(resources::GetString(L"StatusEndpointLimit"), controls::InfoBarSeverity::Warning);
                return;
            }

            auto added = remembered;

            // A new identity within this patch; everything else about it is carried over.
            added.Id = patchbay::PatchDocument::NewId();

            auto const middle = center.value_or(m_canvas.ViewCenter());

            added.CanvasX = (std::max)(0.0, middle.X - EndpointHalfWidth);
            added.CanvasY = (std::max)(0.0, middle.Y - EndpointHalfHeight);

            auto const addedId = added.Id;

            patch->Endpoints.push_back(std::move(added));

            // Built first, so its real size is known before it is moved clear of the others,
            // and the move is part of the same undo step.
            RebuildCanvas();
            m_canvas.MoveClearOfOtherNodes(addedId);

            CommitChange(true);

            m_canvas.Select(patchbay::CanvasSelectionKind::Endpoint, addedId);

            // Moved clear of the others, it can end up out of view, which looks like nothing
            // happened. Added from a menu, the whole patch is fitted; dropped, the view moves only
            // as far as it has to.
            if (!center.has_value())
            {
                m_canvas.FitToContent();
            }
            else
            {
                m_canvas.BringIntoView(addedId);
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to add the endpoint.")
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

            if (m_closing)
            {
                co_return;
            }

            // The library hears about the new endpoint now rather than whenever the watcher says.
            patchbay::PatchLibrary::Current().EndpointsChanged();

            for (auto const& endpoint : patchbay::PatchLibrary::Current().LiveEndpoints())
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

            // Copies, because the dialog below gives the selection time to change.
            auto const nodeIds = m_canvas.SelectedNodeIds();
            auto const connectionId = m_canvas.SelectedConnectionId();

            auto* patch = CurrentPatch();

            if (patch == nullptr)
            {
                co_return;
            }

            winrt::hstring what{};

            if (nodeIds.size() > 1)
            {
                what = resources::FormatString(L"RemoveSelectionMessageFormat", nodeIds.size());
            }
            else if (nodeIds.size() == 1)
            {
                if (auto const* block = patch->FindBlock(nodeIds.front()))
                {
                    what = resources::FormatString(L"RemoveStepMessageFormat", patchbay::BlockDisplayName(*block));
                }
                else if (auto const* endpoint = patch->FindEndpoint(nodeIds.front()))
                {
                    what = resources::FormatString(L"RemoveEndpointMessageFormat", endpoint->DisplayName);
                }
                else
                {
                    co_return;
                }
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

            // Looked up again: the patch can go while the dialog is open.
            patch = CurrentPatch();

            if (patch == nullptr)
            {
                co_return;
            }

            if (nodeIds.empty())
            {
                patch->RemoveConnection(connectionId);
            }

            for (auto const& id : nodeIds)
            {
                if (patch->IsBlock(id))
                {
                    patch->RemoveBlock(id);
                }
                else
                {
                    patch->RemoveEndpoint(id);
                }
            }

            m_canvas.ClearSelection();
            CommitChange(true);
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to remove the selection.")
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
                auto const live = patchbay::ResolveEndpoint(endpoint);

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
    void MainWindow::ShowNodeMenu(std::wstring const& nodeId, foundation::Point const& position) noexcept
    {
        try
        {
            auto* patch = CurrentPatch();

            if (patch == nullptr)
            {
                return;
            }

            controls::MenuFlyout menu{};
            auto weak = get_weak();

            auto const addItem = [&menu, weak](winrt::hstring const& text, std::function<void(MainWindow&)> action)
                {
                    controls::MenuFlyoutItem item{};

                    item.Text(text);
                    item.Click([weak, action](auto&&, auto&&)
                        {
                            if (auto strong = weak.get())
                            {
                                action(*strong);
                            }
                        });

                    menu.Items().Append(item);
                };

            auto const addSeparator = [&menu]()
                {
                    menu.Items().Append(controls::MenuFlyoutSeparator{});
                };

            // Copy, cut, duplicate and remove act on everything selected, which is what a
            // right click on part of a selection means.
            auto const addEditItems = [&addItem, &addSeparator]()
                {
                    addItem(resources::GetString(L"ActionCopy"), [](MainWindow& window) { window.CopySelection(); });
                    addItem(resources::GetString(L"ActionCut"), [](MainWindow& window) { window.CutSelection(); });
                    addItem(resources::GetString(L"ActionDuplicate"), [](MainWindow& window) { window.DuplicateSelection(); });
                    addSeparator();
                    addItem(resources::GetString(L"ActionRemove"), [](MainWindow& window) { window.DeleteSelection(); });
                };

            if (auto const* block = patch->FindBlock(nodeId))
            {
                auto const blockId = block->Id;

                // Nothing goes through an annotation, so there is nothing to bypass.
                if (patchbay::IsAnnotation(block->Kind))
                {
                    addItem(resources::GetString(L"ActionEditAnnotation"),
                        [blockId](MainWindow& window) { window.FocusAnnotationText(blockId); });
                    addSeparator();
                    addEditItems();
                }
                else
                {
                    if (!EditsInInspector(block->Kind))
                    {
                        addItem(resources::GetString(L"ActionEditStep"),
                            [blockId](MainWindow& window) { window.ShowBlockDialogAsync(blockId); });
                    }

                    controls::ToggleMenuFlyoutItem bypassItem{};

                    bypassItem.Text(resources::GetString(L"InspectorBypass"));
                    bypassItem.IsChecked(block->Bypassed);

                    bypassItem.Click([weak, blockId](foundation::IInspectable const& s, auto&&)
                        {
                            auto strong = weak.get();
                            auto const item = s.try_as<controls::ToggleMenuFlyoutItem>();

                            if (strong != nullptr && item != nullptr)
                            {
                                strong->SetBlockBypassed(blockId, item.IsChecked());
                            }
                        });

                    menu.Items().Append(bypassItem);
                    addSeparator();
                    addEditItems();
                }
            }
            else if (auto const* endpoint = patch->FindEndpoint(nodeId))
            {
                auto const live = patchbay::ResolveEndpoint(*endpoint);

                if (live.has_value())
                {
                    auto const deviceId = live->EndpointDeviceId;

                    addItem(resources::GetString(L"TestOpenMonitor"),
                        [deviceId](MainWindow& window) { window.LaunchTool(MonitorExeName, deviceId); });
                    addItem(resources::GetString(L"TestOpenKeyboard"),
                        [deviceId](MainWindow& window) { window.LaunchTool(KeyboardExeName, deviceId); });
                    addItem(resources::GetString(L"TestOpenScratchPad"),
                        [deviceId](MainWindow& window) { window.LaunchTool(ScratchPadExeName, deviceId); });

                    addSeparator();
                }

                auto const endpointId = endpoint->Id;

                addItem(endpoint->ShowAllGroups
                    ? resources::GetString(L"ActionShowDeclaredGroups")
                    : resources::GetString(L"ActionShowAllGroups"),
                    [endpointId](MainWindow& window)
                    {
                        auto* current = window.CurrentPatch();

                        if (auto* target = current == nullptr ? nullptr : current->FindEndpoint(endpointId))
                        {
                            target->ShowAllGroups = !target->ShowAllGroups;
                            window.CommitChange(false);
                        }
                    });

                addSeparator();
                addEditItems();
            }
            else
            {
                return;
            }

            primitives::FlyoutShowOptions options{};

            options.Position(position);
            options.Placement(primitives::FlyoutPlacementMode::BottomEdgeAlignedLeft);

            menu.ShowAt(CanvasScroller(), options);
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to show the menu.")
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
