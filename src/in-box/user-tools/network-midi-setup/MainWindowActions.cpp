// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================
//
// Everything the customer can act on. The order is always the same: ask the service to make
// the change, and only write it to the configuration file once the service has agreed. A
// configuration entry for something the service refused would come back on the next restart.

#include "pch.h"
#include "MainWindow.xaml.h"

#include "StringResources.h"

namespace native = ::midinetworksetup;
namespace res = ::midinetworksetup::resources;

namespace winrt::midinetworksetup::implementation
{
    namespace
    {
        constexpr std::chrono::milliseconds TransportSettingsWriteDelay{ 750 };

        winrt::hstring EntryKeyOf(_In_ winrt::guid const& value) noexcept
        {
            try
            {
                return winrt::to_hstring(value);
            }
            catch (...)
            {
                return {};
            }
        }

        bool TryParseKey(_In_ winrt::hstring const& text, _Out_ winrt::guid& value) noexcept
        {
            value = winrt::guid{};

            if (text.empty())
            {
                return false;
            }

            try
            {
                value = winrt::guid{ std::wstring_view{ text } };
                return true;
            }
            catch (...)
            {
                return false;
            }
        }

        template <typename TItem>
        TItem ItemOf(_In_ foundation::IInspectable const& sender) noexcept
        {
            try
            {
                auto const element = sender.try_as<xaml::FrameworkElement>();

                if (element == nullptr)
                {
                    return nullptr;
                }

                return element.DataContext().try_as<TItem>();
            }
            catch (...)
            {
                return nullptr;
            }
        }

        // The image is loaded by name from a known folder, so a path would either escape that
        // folder or simply fail to load.
        bool IsBareFileName(_In_ winrt::hstring const& value) noexcept
        {
            std::wstring const copy{ value };

            return copy.find(L'\\') == std::wstring::npos &&
                copy.find(L'/') == std::wstring::npos &&
                copy.find(L':') == std::wstring::npos;
        }

        // Awaiting this from the UI thread does the copy on a worker and comes back on the UI
        // thread, because awaiting a WinRT async object restores the apartment context.
        foundation::IAsyncOperation<winrt::hstring> ImportImageAsync(winrt::hstring sourcePath)
        {
            co_await winrt::resume_background();

            co_return midiapp::ImportEndpointImage(sourcePath);
        }

        winrt::hstring TextOf(_In_ controls::TextBox const& box) noexcept
        {
            try
            {
                std::wstring value{ box.Text() };

                auto const first = value.find_first_not_of(L" \t\r\n");

                if (first == std::wstring::npos)
                {
                    return {};
                }

                auto const last = value.find_last_not_of(L" \t\r\n");

                return winrt::hstring{ value.substr(first, last - first + 1) };
            }
            catch (...)
            {
                return {};
            }
        }

        // NumberBox only pushes Text into Value when it loses focus, so a port typed immediately
        // before the button is invoked is still sitting in Text. Prefer that, fall back to Value.
        uint16_t PortFrom(_In_ controls::NumberBox const& box) noexcept
        {
            try
            {
                uint32_t parsed{};

                std::wstring const text{ box.Text() };

                if (!text.empty() &&
                    std::all_of(text.begin(), text.end(), [](wchar_t const c) { return c >= L'0' && c <= L'9'; }))
                {
                    for (auto const c : text)
                    {
                        parsed = parsed * 10 + static_cast<uint32_t>(c - L'0');

                        if (parsed > 65535)
                        {
                            parsed = 0;
                            break;
                        }
                    }
                }

                if (parsed == 0)
                {
                    auto const value = box.Value();

                    // NaN when the box is empty
                    if (value == value && value >= 1.0 && value <= 65535.0)
                    {
                        parsed = static_cast<uint32_t>(value);
                    }
                }

                return static_cast<uint16_t>(parsed);
            }
            catch (...)
            {
                return 0;
            }
        }

        bool IsCheckBoxChecked(_In_ controls::CheckBox const& box) noexcept
        {
            try
            {
                auto const value = box.IsChecked();

                return value != nullptr && value.Value();
            }
            catch (...)
            {
                return false;
            }
        }
    }


    foundation::IAsyncOperation<bool> MainWindow::ConfirmAsync(winrt::hstring const& title, winrt::hstring const& message)
    {
        // only one dialog can be open at a time, and a second ShowAsync throws
        if (m_openDialog != nullptr)
        {
            co_return false;
        }

        try
        {
            ConfirmDialog().Title(winrt::box_value(title));
            ConfirmDialogText().Text(message);
            ConfirmDialog().XamlRoot(Content().XamlRoot());

            m_openDialog = ConfirmDialog();

            auto const result = co_await ConfirmDialog().ShowAsync();

            m_openDialog = nullptr;

            co_return result == controls::ContentDialogResult::Primary;
        }
        catch (...)
        {
            m_openDialog = nullptr;

            co_return false;
        }
    }


    // Asks for an optional display name before connecting. Returns false if the user canceled.
    // The name is left empty when they accept without typing one, which means "use the name the
    // device reports".
    foundation::IAsyncOperation<bool> MainWindow::PromptForConnectNameAsync(
        winrt::hstring const deviceName,
        std::shared_ptr<winrt::hstring> customName)
    {
        if (m_openDialog != nullptr)
        {
            co_return false;
        }

        try
        {
            ConnectNameDialogText().Text(res::FormatString(L"ConnectNamePromptFormat", deviceName));
            ConnectNameDialogTextBox().Text(L"");
            ConnectNameDialog().XamlRoot(Content().XamlRoot());

            m_openDialog = ConnectNameDialog();

            auto const result = co_await ConnectNameDialog().ShowAsync();

            m_openDialog = nullptr;

            if (result != controls::ContentDialogResult::Primary)
            {
                co_return false;
            }

            *customName = TextOf(ConnectNameDialogTextBox());

            co_return true;
        }
        catch (...)
        {
            m_openDialog = nullptr;

            co_return false;
        }
    }


    // ------------------------------------------------------------------------------------
    // page 1: connecting to remote hosts
    // ------------------------------------------------------------------------------------

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnConnectRemoteHostClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const&)
    {
        auto item = ItemOf<midinetworksetup::RemoteHostItem>(sender);

        if (item == nullptr)
        {
            co_return;
        }

        auto customName = std::make_shared<winrt::hstring>();

        if (!co_await PromptForConnectNameAsync(item.DisplayName(), customName))
        {
            co_return;
        }

        ConnectRemoteHostAsync(item, false, *customName);
    }

    _Use_decl_annotations_
    void MainWindow::OnMonitorRemoteHostClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const&)
    {
        try
        {
            auto item = ItemOf<midinetworksetup::RemoteHostItem>(sender);

            if (item == nullptr || item.EndpointDeviceId().empty())
            {
                return;
            }

            if (!midiapp::LaunchMonitorForEndpoint(item.EndpointDeviceId()))
            {
                SetRemoteStatus(res::GetString(L"StatusMonitorNotAvailable"));
            }
        }
        MIDI_NETSETUP_CATCH_AND_LOG(L"Unable to open the MIDI monitor for a remote device.")
    }

    _Use_decl_annotations_
    void MainWindow::OnMonitorHostConnectionClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const&)
    {
        try
        {
            auto item = ItemOf<midinetworksetup::HostConnectionItem>(sender);

            if (item == nullptr || item.EndpointDeviceId().empty())
            {
                return;
            }

            if (!midiapp::LaunchMonitorForEndpoint(item.EndpointDeviceId()))
            {
                SetLocalStatus(res::GetString(L"StatusMonitorNotAvailable"));
            }
        }
        MIDI_NETSETUP_CATCH_AND_LOG(L"Unable to open the MIDI monitor for a connected client.")
    }

    _Use_decl_annotations_
    void MainWindow::OnCopyEndpointDeviceIdClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const&)
    {
        auto item = ItemOf<midinetworksetup::RemoteHostItem>(sender);

        if (item == nullptr || item.EndpointDeviceId().empty())
        {
            return;
        }

        try
        {
            winrt::Windows::ApplicationModel::DataTransfer::DataPackage package{};

            package.RequestedOperation(
                winrt::Windows::ApplicationModel::DataTransfer::DataPackageOperation::Copy);
            package.SetText(item.EndpointDeviceId());

            winrt::Windows::ApplicationModel::DataTransfer::Clipboard::SetContent(package);

            SetRemoteStatus(res::GetString(L"EndpointDeviceIdCopied"));
        }
        catch (...)
        {
            SetRemoteStatus(res::GetString(L"EndpointDeviceIdCopyFailed"));
        }
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnRetryRemoteHostClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const&)
    {
        ConnectRemoteHostAsync(ItemOf<midinetworksetup::RemoteHostItem>(sender), true, winrt::hstring{});

        co_return;
    }

    _Use_decl_annotations_
    void MainWindow::OnReassociateSelectionChanged(
        foundation::IInspectable const&,
        controls::SelectionChangedEventArgs const&)
    {
        try
        {
            ReassociateDialog().IsPrimaryButtonEnabled(
                ReassociateDialogList().SelectedItem() != nullptr);
        }
        catch (...)
        {
        }
    }

    _Use_decl_annotations_
    foundation::IAsyncOperation<bool> MainWindow::PromptForReassociateTargetAsync(
        winrt::hstring const entryName,
        std::shared_ptr<midinetworksetup::RemoteHostItem> chosen)
    {
        if (m_openDialog != nullptr)
        {
            co_return false;
        }

        try
        {
            // Only devices which are on the network and not already saved. Offering one which is
            // already configured would produce two entries pointing at the same device.
            auto candidates = winrt::single_threaded_observable_vector<midinetworksetup::RemoteHostItem>();

            for (auto const& candidate : m_remoteHosts)
            {
                if (candidate != nullptr && candidate.IsAdvertised() && !candidate.IsConfigured())
                {
                    candidates.Append(candidate);
                }
            }

            if (candidates.Size() == 0)
            {
                SetRemoteStatus(res::GetString(L"ReassociateNoCandidates"));
                co_return false;
            }

            ReassociateDialogText().Text(res::FormatString(L"ReassociatePromptFormat", entryName));
            ReassociateDialogList().ItemsSource(candidates);
            ReassociateDialogList().SelectedItem(nullptr);
            ReassociateDialog().IsPrimaryButtonEnabled(false);
            ReassociateDialog().XamlRoot(Content().XamlRoot());

            m_openDialog = ReassociateDialog();

            auto const result = co_await ReassociateDialog().ShowAsync();

            m_openDialog = nullptr;

            if (result != controls::ContentDialogResult::Primary)
            {
                co_return false;
            }

            auto const selected = ReassociateDialogList().SelectedItem();

            if (selected == nullptr)
            {
                co_return false;
            }

            *chosen = selected.as<midinetworksetup::RemoteHostItem>();

            co_return true;
        }
        catch (...)
        {
            m_openDialog = nullptr;

            co_return false;
        }
    }

    // Points a saved entry at a different device, keeping the entry and the name the customer
    // chose. Used when a device changes the identity it advertises, typically after a firmware
    // update, so the saved entry can never match it again.
    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnReassociateRemoteHostClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const&)
    {
        auto item = ItemOf<midinetworksetup::RemoteHostItem>(sender);

        if (item == nullptr || item.ClientId().empty())
        {
            co_return;
        }

        winrt::guid clientId{};

        if (!TryParseKey(item.ClientId(), clientId))
        {
            co_return;
        }

        auto const entryName = item.DisplayName();

        // What the customer actually chose, which may be nothing. Using the row's display name
        // here would pin a name the service had only derived from the device.
        auto const savedCustomName =
            native::NetworkConfigFile::Current().GetClientCustomEndpointName(item.ClientId());

        auto target = std::make_shared<midinetworksetup::RemoteHostItem>(nullptr);

        if (!co_await PromptForReassociateTargetAsync(entryName, target) || *target == nullptr)
        {
            co_return;
        }

        auto weak = get_weak();
        auto queue = DispatcherQueue();

        item.IsBusy(true);

        auto const oldKey = item.ClientId();

        co_await winrt::resume_background();

        bool removed{ false };

        try
        {
            // The old entry has to go first: the identifier is being reused, and two entries
            // cannot hold it. Removing it live also stops the service retrying a device which
            // is never coming back.
            midi2net::MidiNetworkClientDisconnectConfig config{};
            config.ClientId(clientId);

            auto const response = co_await midi2net::MidiNetworkTransportManager::DisconnectNetworkClientAsync(config);

            removed = response != nullptr && response.Success();

            if (removed)
            {
                removed = native::NetworkConfigFile::Current().RemoveClient(oldKey);
            }
        }
        catch (...)
        {
            removed = false;
        }

        if (queue != nullptr)
        {
            queue.TryEnqueue([weak, item, target, entryName, savedCustomName, clientId, removed]()
                {
                    item.IsBusy(false);

                    auto strong = weak.get();

                    if (strong == nullptr)
                    {
                        return;
                    }

                    if (!removed)
                    {
                        strong->SetRemoteStatus(res::GetString(L"ReassociateFailed"));
                        return;
                    }

                    // Whatever name the customer had chosen is carried over, which is the thing
                    // they would otherwise have to set up again.
                    strong->ConnectRemoteHostAsync(*target, false, savedCustomName, clientId);
                });
        }
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::ConnectRemoteHostAsync(
        midinetworksetup::RemoteHostItem const item,
        bool const reuseExistingEntry,
        winrt::hstring const customEndpointName,
        winrt::guid const explicitClientId)
    {
        if (item == nullptr)
        {
            co_return;
        }

        auto weak = get_weak();
        auto queue = DispatcherQueue();

        item.IsBusy(true);
        SetRemoteStatus(res::FormatString(L"ConnectingToDeviceFormat", item.DisplayName()));

        auto const deviceId = item.DeviceId();
        auto const displayName = item.DisplayName();
        auto const productInstanceId = item.ProductInstanceId();

        // re-arming an entry the service already knows about keeps its identifier, so the
        // configuration file entry stays the one the customer already has
        winrt::guid clientId{};

        if (explicitClientId != winrt::guid{})
        {
            // re-associating: the entry being kept is not the one supplying the match criteria
            clientId = explicitClientId;
        }
        else if (!reuseExistingEntry || !TryParseKey(item.ClientId(), clientId))
        {
            clientId = foundation::GuidHelper::CreateNewGuid();
        }

        co_await winrt::resume_background();

        winrt::hstring message{};

        try
        {
            midi2net::MidiNetworkClientMatchCriteria criteria{};

            // Matched on the device id it was discovered with, so the service re-resolves the
            // address from the advertisement every time. That is what lets the connection
            // survive the device moving to a new address or picking a new port.
            criteria.DeviceId(deviceId);

            // The device's own identity, so the entry still resolves if its DNS-SD instance
            // label changes. A responder renames a colliding label, and a firmware update or a
            // user can change it outright.
            criteria.ProductInstanceId(productInstanceId);
            criteria.UmpEndpointName(displayName);

            midi2net::MidiNetworkClientConnectConfig config{};
            config.ClientId(clientId);

            // Deliberately not set: UmpEndpointName here is the name THIS PC announces to the
            // remote, not the remote's name. Leaving it empty lets the service derive it from the
            // machine name, which is what a config file created entry also gets.
            config.CustomEndpointName(customEndpointName);
            config.MatchCriteria(criteria);

            auto const response = co_await midi2net::MidiNetworkTransportManager::ConnectNetworkClientAsync(config);

            if (response != nullptr && response.Success())
            {
                if (!native::NetworkConfigFile::Current().MergeSection(config.ConfigJson()))
                {
                    message = native::NetworkConfigFile::Current().LastErrorMessage();
                }
                else
                {
                    message = res::FormatString(L"ConnectRequestedFormat", displayName);
                }
            }
            else
            {
                message = response == nullptr ?
                    res::GetString(L"ConnectFailedGeneral") :
                    res::FormatString(L"ConnectFailedFormat", response.ErrorMessage());
            }
        }
        catch (...)
        {
            message = res::GetString(L"ConnectFailedGeneral");
        }

        if (queue != nullptr)
        {
            queue.TryEnqueue([weak, item, message]()
                {
                    item.IsBusy(false);

                    if (auto strong = weak.get())
                    {
                        strong->SetRemoteStatus(message);
                        strong->RequestRefreshAsync();
                    }
                });
        }
    }

    // ============================== Customization ==============================

    _Use_decl_annotations_
    foundation::IAsyncOperation<bool> MainWindow::ShowCustomizeDialogAsync(
        midinetworksetup::RemoteHostItem const item,
        std::shared_ptr<winrt::hstring> errorMessage)
    {
        if (m_openDialog != nullptr || item == nullptr || item.EndpointDeviceId().empty())
        {
            co_return false;
        }

        auto strongThis = get_strong();

        // Both the values on screen and the identity the customization is keyed on come from the
        // endpoint rather than the row, because the row carries what was discovered on the
        // network and this has to describe what Windows actually created.
        winrt::hstring deviceInstanceId{};
        winrt::hstring transportSuppliedName{};
        winrt::hstring currentName{};
        winrt::hstring currentDescription{};
        winrt::hstring currentImage{};
        bool currentCreateMidi1Ports{ true };

        try
        {
            auto const info = midi2enum::MidiEndpointDeviceInformation::CreateFromEndpointDeviceId(
                item.EndpointDeviceId());

            if (info == nullptr)
            {
                co_return false;
            }

            deviceInstanceId = info.DeviceInstanceId();
            currentCreateMidi1Ports = info.IsMidi1PortCreationEnabled();

            auto const transportInfo = info.GetTransportSuppliedInfo();
            transportSuppliedName = transportInfo.Name();

            auto const userInfo = info.GetUserSuppliedInfo();

            if (userInfo != nullptr)
            {
                currentName = userInfo.Name();
                currentDescription = userInfo.Description();
                currentImage = userInfo.ImageFileName();
            }
        }
        catch (...)
        {
            co_return false;
        }

        if (deviceInstanceId.empty())
        {
            co_return false;
        }

        controls::ContentDialogResult result{ controls::ContentDialogResult::None };

        try
        {
            CustomizeTransportNameText().Text(transportSuppliedName.empty() ?
                winrt::hstring{} :
                res::FormatString(L"CustomizeTransportNameFormat", transportSuppliedName));

            CustomizeNameBox().Text(currentName);
            CustomizeDescriptionBox().Text(currentDescription);
            CustomizeImageBox().Text(currentImage);
            CustomizeCreateMidi1PortsCheckBox().IsChecked(currentCreateMidi1Ports);

            CustomizeDialog().XamlRoot(Content().XamlRoot());

            m_openDialog = CustomizeDialog();

            result = co_await CustomizeDialog().ShowAsync();

            m_openDialog = nullptr;
        }
        catch (...)
        {
            m_openDialog = nullptr;

            co_return false;
        }

        if (result == controls::ContentDialogResult::None)
        {
            co_return false;
        }

        auto const reset = result == controls::ContentDialogResult::Secondary;

        auto const name = reset ? winrt::hstring{} : TextOf(CustomizeNameBox());
        auto const description = reset ? winrt::hstring{} : TextOf(CustomizeDescriptionBox());
        auto const image = reset ? winrt::hstring{} : TextOf(CustomizeImageBox());

        if (!image.empty() && !IsBareFileName(image))
        {
            SetRemoteStatus(res::GetString(L"StatusImageMustBeFileName"));

            co_return false;
        }

        auto const checkBoxState = CustomizeCreateMidi1PortsCheckBox().IsChecked();

        // Reset clears the display customization only. Taking the ports away as well would be a
        // destructive surprise from a button pressed to clear a name, and would not be undone by
        // pressing it again.
        auto const createMidi1Ports = reset ?
            currentCreateMidi1Ports :
            (checkBoxState != nullptr && checkBoxState.Value());

        auto const clientKey = item.ClientId();

        co_await winrt::resume_background();

        bool succeeded{ false };
        winrt::hstring failure{};

        try
        {
            midi2svc::MidiServiceConfigEndpointMatchCriteria match{};
            match.DeviceInstanceId(deviceInstanceId);

            midi2svc::MidiServiceEndpointCustomizationConfig config{
                midi2net::MidiNetworkTransportManager::TransportId() };

            config.MatchCriteria(match);
            config.Name(name);
            config.Description(description);
            config.ImageFileName(image);

            // What is on screen is the whole customization, so emptying a box has to clear the
            // stored value rather than leaving it out of the save.
            config.ClearDisplayProperties(true);

            auto const sendResponse = midi2svc::MidiServiceTransportPluginConfigManager::SendUpdate(config);

            if (sendResponse != nullptr &&
                sendResponse.Status() == midi2svc::MidiServiceConfigResponseStatus::Success)
            {
                if (reset)
                {
                    // Saving three empty values would leave a stored entry which says nothing,
                    // so the entry comes out of the file instead.
                    midi2svc::MidiServiceEndpointCustomizationRemovalConfig removal{
                        midi2net::MidiNetworkTransportManager::TransportId(), match };

                    auto const saveResponse = midi2svc::MidiServiceTransportPluginConfigManager::SaveUpdate(removal);

                    succeeded = saveResponse != nullptr && saveResponse.Success();

                    if (!succeeded && saveResponse != nullptr)
                    {
                        failure = saveResponse.ErrorMessage();
                    }
                }
                else
                {
                    auto const saveResponse = midi2svc::MidiServiceTransportPluginConfigManager::SaveUpdate(config);

                    succeeded = saveResponse != nullptr && saveResponse.Success();

                    if (!succeeded && saveResponse != nullptr)
                    {
                        failure = saveResponse.ErrorMessage();
                    }
                }
            }
            else if (sendResponse != nullptr)
            {
                failure = sendResponse.ServiceErrorMessage();
            }

            // Kept out of the customization above on purpose: this one is read when the endpoint
            // is built, so it belongs to the entry which creates it and cannot be pushed live.
            if (succeeded && !clientKey.empty() && createMidi1Ports != currentCreateMidi1Ports)
            {
                if (!native::NetworkConfigFile::Current().SetClientCreateMidi1Ports(clientKey, createMidi1Ports))
                {
                    failure = native::NetworkConfigFile::Current().LastErrorMessage();
                    succeeded = false;
                }
            }
        }
        catch (...)
        {
        }

        if (errorMessage != nullptr)
        {
            *errorMessage = failure;
        }

        // awaiting a WinRT async object restores the caller's apartment context, so the caller
        // is back on the UI thread without this having to marshal
        co_return succeeded;
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnCustomizeRemoteHostClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const&)
    {
        auto item = ItemOf<midinetworksetup::RemoteHostItem>(sender);

        if (item == nullptr)
        {
            co_return;
        }

        auto strongThis = get_strong();

        auto errorMessage = std::make_shared<winrt::hstring>();

        auto const succeeded = co_await ShowCustomizeDialogAsync(item, errorMessage);

        try
        {
            if (succeeded)
            {
                SetRemoteStatus(res::GetString(L"StatusCustomizationSaved"));
            }
            else
            {
                SetRemoteStatus(errorMessage->empty() ?
                    res::GetString(L"StatusCustomizationNotSaved") :
                    *errorMessage);
            }

            RequestRefreshAsync();
        }
        MIDI_NETSETUP_CATCH_AND_LOG(L"Unable to report the result of customizing a device.")
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnBrowseForImageClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        auto strongThis = get_strong();

        try
        {
            winrt::Windows::Storage::Pickers::FileOpenPicker picker{};

            // A picker in a desktop app has no window of its own to sit over, so it is given
            // this one. Without it the call fails rather than showing anything.
            HWND handle{ nullptr };

            if (auto const native = try_as<::IWindowNative>())
            {
                LOG_IF_FAILED(native->get_WindowHandle(&handle));
            }

            if (handle == nullptr)
            {
                co_return;
            }

            if (auto const initialize = picker.as<::IInitializeWithWindow>())
            {
                LOG_IF_FAILED(initialize->Initialize(handle));
            }

            picker.ViewMode(winrt::Windows::Storage::Pickers::PickerViewMode::Thumbnail);
            picker.SuggestedStartLocation(winrt::Windows::Storage::Pickers::PickerLocationId::PicturesLibrary);

            for (auto const& extension : { L".png", L".jpg", L".jpeg", L".bmp", L".gif", L".svg" })
            {
                picker.FileTypeFilter().Append(extension);
            }

            auto const file = co_await picker.PickSingleFileAsync();

            if (file == nullptr)
            {
                co_return;
            }

            auto const sourcePath = file.Path();

            // Copied rather than referenced, so the stored value is always a name inside the
            // shared folder and cannot break when the customer moves the original.
            auto const importedName = co_await ImportImageAsync(sourcePath);

            if (importedName.empty())
            {
                SetRemoteStatus(res::GetString(L"StatusImageCopyFailed"));

                co_return;
            }

            CustomizeImageBox().Text(importedName);
        }
        MIDI_NETSETUP_CATCH_AND_LOG(L"Unable to choose an image.")
    }

    _Use_decl_annotations_
    void MainWindow::OnRemoveImageClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            // Only the reference goes. The file stays in the shared folder, where another
            // endpoint may well be using it.
            CustomizeImageBox().Text(winrt::hstring{});
        }
        MIDI_NETSETUP_CATCH_AND_LOG(L"Unable to clear the image.")
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnDisconnectRemoteHostClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const&)
    {
        auto item = ItemOf<midinetworksetup::RemoteHostItem>(sender);

        if (item == nullptr || item.ClientId().empty())
        {
            co_return;
        }

        auto weak = get_weak();
        auto queue = DispatcherQueue();

        auto const clientKey = item.ClientId();
        auto const displayName = item.DisplayName();

        winrt::guid clientId{};

        if (!TryParseKey(clientKey, clientId))
        {
            co_return;
        }

        if (!co_await ConfirmAsync(
            item.IsConnected() ?
                res::GetString(L"DisconnectConfirmTitle") :
                res::GetString(L"ForgetConfirmTitle"),
            item.IsConnected() ?
                res::FormatString(L"DisconnectConfirmMessageFormat", displayName) :
                res::FormatString(L"ForgetConfirmMessageFormat", displayName)))
        {
            co_return;
        }

        auto const wasConnected = item.IsConnected();

        item.IsBusy(true);

        co_await winrt::resume_background();

        winrt::hstring message{};

        try
        {
            midi2net::MidiNetworkClientDisconnectConfig config{};
            config.ClientId(clientId);

            auto const response = co_await midi2net::MidiNetworkTransportManager::DisconnectNetworkClientAsync(config);

            if (response != nullptr && response.Success())
            {
                if (!native::NetworkConfigFile::Current().RemoveClient(clientKey))
                {
                    message = native::NetworkConfigFile::Current().LastErrorMessage();
                }
                else
                {
                    message = wasConnected ?
                        res::FormatString(L"DisconnectedFormat", displayName) :
                        res::FormatString(L"ForgottenFormat", displayName);
                }
            }
            else
            {
                message = response == nullptr ?
                    res::GetString(L"DisconnectFailedGeneral") :
                    res::FormatString(L"DisconnectFailedFormat", response.ErrorMessage());
            }
        }
        catch (...)
        {
            message = res::GetString(L"DisconnectFailedGeneral");
        }

        if (queue != nullptr)
        {
            queue.TryEnqueue([weak, item, message, clientKey]()
                {
                    item.IsBusy(false);

                    if (auto strong = weak.get())
                    {
                        strong->SetRemoteStatus(message);
                        strong->RequestRefreshAsync();
                    }
                });
        }
    }

    _Use_decl_annotations_
    void MainWindow::OnManualConnectFieldChanged(foundation::IInspectable const&, controls::TextChangedEventArgs const&)
    {
        UpdateManualConnectButton();
    }

    _Use_decl_annotations_
    void MainWindow::OnManualConnectPortChanged(controls::NumberBox const&, controls::NumberBoxValueChangedEventArgs const&)
    {
        UpdateManualConnectButton();
    }

    // The transport clamps anything out of range, so the boxes carry the same bounds only to
    // save the customer from typing a number which would be silently corrected.
    void MainWindow::LoadTransportSettings() noexcept
    {
        try
        {
            auto const settings = midi2net::MidiNetworkTransportManager::GetTransportSettings();

            if (settings == nullptr)
            {
                return;
            }

            m_loadingTransportSettings = true;

            auto const applyTo = [](controls::NumberBox const& box, uint32_t const minimum, uint32_t const maximum, uint32_t const value)
                {
                    box.Minimum(static_cast<double>(minimum));
                    box.Maximum(static_cast<double>(maximum));
                    box.Value(static_cast<double>(value));
                };

            applyTo(
                MaxHostConnectionsBox(),
                midi2net::MidiNetworkTransportSettings::MinMaxHostConnections(),
                midi2net::MidiNetworkTransportSettings::MaxMaxHostConnections(),
                settings.MaxHostConnections());

            applyTo(
                InvitationPendingTimeoutBox(),
                midi2net::MidiNetworkTransportSettings::MinInvitationPendingTimeoutMilliseconds(),
                midi2net::MidiNetworkTransportSettings::MaxInvitationPendingTimeoutMilliseconds(),
                settings.InvitationPendingTimeoutMilliseconds());

            applyTo(
                DirectConnectionScanIntervalBox(),
                midi2net::MidiNetworkTransportSettings::MinDirectConnectionScanIntervalMilliseconds(),
                midi2net::MidiNetworkTransportSettings::MaxDirectConnectionScanIntervalMilliseconds(),
                settings.DirectConnectionScanIntervalMilliseconds());

            applyTo(
                OutboundPingIntervalBox(),
                midi2net::MidiNetworkTransportSettings::MinOutboundPingIntervalMilliseconds(),
                midi2net::MidiNetworkTransportSettings::MaxOutboundPingIntervalMilliseconds(),
                settings.OutboundPingIntervalMilliseconds());

            applyTo(
                MaxFecPacketsBox(),
                midi2net::MidiNetworkTransportSettings::MinMaxForwardErrorCorrectionCommandPackets(),
                midi2net::MidiNetworkTransportSettings::MaxMaxForwardErrorCorrectionCommandPackets(),
                settings.MaxForwardErrorCorrectionCommandPackets());

            applyTo(
                MaxRetransmitBufferBox(),
                midi2net::MidiNetworkTransportSettings::MinMaxRetransmitBufferCommandPackets(),
                midi2net::MidiNetworkTransportSettings::MaxMaxRetransmitBufferCommandPackets(),
                settings.MaxRetransmitBufferCommandPackets());

            m_loadingTransportSettings = false;

            AppendTransportSettingDefaults();
            UpdateTransportSettingSecondsText();
        }
        catch (...)
        {
            m_loadingTransportSettings = false;
        }
    }

    void MainWindow::UpdateTransportSettingSecondsText() noexcept
    {
        try
        {
            auto const show = [](controls::NumberBox const& box, controls::TextBlock const& text)
                {
                    auto const value = box.Value();

                    // An empty box reads as NaN rather than zero, and formatting that would put
                    // "nan seconds" on screen.
                    if (std::isnan(value))
                    {
                        text.Text(L"");
                        return;
                    }

                    text.Text(res::FormatString(
                        L"SettingSecondsFormat",
                        winrt::hstring{ std::format(L"{:.2f}", value / 1000.0) }));
                };

            show(InvitationPendingTimeoutBox(), InvitationPendingTimeoutSecondsText());
            show(DirectConnectionScanIntervalBox(), DirectConnectionScanIntervalSecondsText());
            show(OutboundPingIntervalBox(), OutboundPingIntervalSecondsText());
        }
        catch (...)
        {
        }
    }

    void MainWindow::AppendTransportSettingDefaults() noexcept
    {
        if (m_transportSettingDefaultsShown)
        {
            return;
        }

        try
        {
            // A default-constructed settings object holds exactly the defaults the transport
            // uses, so these cannot drift from the service the way a table here would.
            midi2net::MidiNetworkTransportSettings const defaults{};

            auto const append = [](controls::TextBlock const& text, uint32_t const value)
                {
                    text.Text(
                        text.Text() +
                        L" " +
                        res::FormatString(
                            L"SettingDefaultValueFormat",
                            winrt::hstring{ std::format(L"{}", value) }));
                };

            append(MaxHostConnectionsDescriptionText(), defaults.MaxHostConnections());
            append(InvitationPendingTimeoutDescriptionText(), defaults.InvitationPendingTimeoutMilliseconds());
            append(DirectConnectionScanIntervalDescriptionText(), defaults.DirectConnectionScanIntervalMilliseconds());
            append(OutboundPingIntervalDescriptionText(), defaults.OutboundPingIntervalMilliseconds());
            append(MaxFecPacketsDescriptionText(), defaults.MaxForwardErrorCorrectionCommandPackets());
            append(MaxRetransmitBufferDescriptionText(), defaults.MaxRetransmitBufferCommandPackets());

            m_transportSettingDefaultsShown = true;
        }
        catch (...)
        {
        }
    }

    _Use_decl_annotations_
    void MainWindow::OnTransportSettingChanged(controls::NumberBox const&, controls::NumberBoxValueChangedEventArgs const&)
    {
        // Updated even while loading, so the readout matches the box from the moment it is filled.
        UpdateTransportSettingSecondsText();

        if (!m_loaded || m_loadingTransportSettings)
        {
            return;
        }

        QueueTransportSettingsWrite();
    }

    // Long enough that holding a spinner or typing a four digit number settles into one write,
    // short enough that letting go feels like it saved immediately.
    void MainWindow::QueueTransportSettingsWrite() noexcept
    {
        try
        {
            auto queue = DispatcherQueue();

            if (queue == nullptr)
            {
                ApplyTransportSettingsAsync();
                return;
            }

            if (m_transportSettingsWriteTimer == nullptr)
            {
                m_transportSettingsWriteTimer = queue.CreateTimer();

                if (m_transportSettingsWriteTimer == nullptr)
                {
                    ApplyTransportSettingsAsync();
                    return;
                }

                m_transportSettingsWriteTimer.IsRepeating(false);

                m_transportSettingsWriteTimer.Tick([weak = get_weak()](auto&& sender, auto&&)
                    {
                        sender.Stop();

                        auto strong = weak.get();

                        if (strong == nullptr || strong->m_closing)
                        {
                            return;
                        }

                        strong->ApplyTransportSettingsAsync();
                    });
            }

            // Restarting the timer is what collapses a run of changes into one write
            m_transportSettingsWriteTimer.Stop();
            m_transportSettingsWriteTimer.Interval(TransportSettingsWriteDelay);
            m_transportSettingsWriteTimer.Start();
        }
        catch (...)
        {
        }
    }

    void MainWindow::FlushPendingTransportSettingsWrite() noexcept
    {
        try
        {
            if (m_transportSettingsWriteTimer == nullptr || !m_transportSettingsWriteTimer.IsRunning())
            {
                return;
            }

            m_transportSettingsWriteTimer.Stop();

            ApplyTransportSettingsAsync();
        }
        catch (...)
        {
        }
    }

    _Use_decl_annotations_
    void MainWindow::OnRestoreTransportSettingDefaultsClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            // A new settings object starts on the transport's own defaults, so they never have
            // to be repeated here.
            midi2net::MidiNetworkTransportSettings const defaults{};

            m_loadingTransportSettings = true;

            MaxHostConnectionsBox().Value(defaults.MaxHostConnections());
            InvitationPendingTimeoutBox().Value(defaults.InvitationPendingTimeoutMilliseconds());
            DirectConnectionScanIntervalBox().Value(defaults.DirectConnectionScanIntervalMilliseconds());
            OutboundPingIntervalBox().Value(defaults.OutboundPingIntervalMilliseconds());
            MaxFecPacketsBox().Value(defaults.MaxForwardErrorCorrectionCommandPackets());
            MaxRetransmitBufferBox().Value(defaults.MaxRetransmitBufferCommandPackets());

            m_loadingTransportSettings = false;

            QueueTransportSettingsWrite();
        }
        catch (...)
        {
            m_loadingTransportSettings = false;
        }
    }

    // Sent to the service first and only written to the file once it has agreed, the same order
    // every other change on these pages uses.
    winrt::fire_and_forget MainWindow::ApplyTransportSettingsAsync() noexcept
    {
        auto strongThis = get_strong();

        winrt::hstring errorMessage{};
        bool succeeded{ false };

        try
        {
            // An empty NumberBox reports NaN, which would otherwise become a huge number on the
            // way to uint32_t. Keeping the current value means clearing a box does nothing.
            auto const valueOf = [](controls::NumberBox const& box, uint32_t const fallback)
                {
                    auto const value = box.Value();

                    if (value != value || value < 0.0)
                    {
                        return fallback;
                    }

                    return static_cast<uint32_t>(value);
                };

            midi2net::MidiNetworkTransportSettings settings{};

            settings.MaxHostConnections(valueOf(MaxHostConnectionsBox(), settings.MaxHostConnections()));
            settings.InvitationPendingTimeoutMilliseconds(valueOf(InvitationPendingTimeoutBox(), settings.InvitationPendingTimeoutMilliseconds()));
            settings.DirectConnectionScanIntervalMilliseconds(valueOf(DirectConnectionScanIntervalBox(), settings.DirectConnectionScanIntervalMilliseconds()));
            settings.OutboundPingIntervalMilliseconds(valueOf(OutboundPingIntervalBox(), settings.OutboundPingIntervalMilliseconds()));
            settings.MaxForwardErrorCorrectionCommandPackets(valueOf(MaxFecPacketsBox(), settings.MaxForwardErrorCorrectionCommandPackets()));
            settings.MaxRetransmitBufferCommandPackets(valueOf(MaxRetransmitBufferBox(), settings.MaxRetransmitBufferCommandPackets()));

            auto const sendResponse = midi2svc::MidiServiceTransportPluginConfigManager::SendUpdate(settings);

            if (sendResponse != nullptr && sendResponse.Status() == midi2svc::MidiServiceConfigResponseStatus::Success)
            {
                auto const saveResponse = midi2svc::MidiServiceTransportPluginConfigManager::SaveUpdate(settings);

                succeeded = saveResponse != nullptr && saveResponse.Success();

                if (!succeeded && saveResponse != nullptr)
                {
                    errorMessage = saveResponse.ErrorMessage();
                }
            }
            else if (sendResponse != nullptr)
            {
                errorMessage = sendResponse.ServiceErrorMessage();
            }
        }
        catch (...)
        {
        }

        try
        {
            SettingsStatusText().Text(
                succeeded ?
                res::GetString(L"StatusTransportSettingsSaved") :
                (errorMessage.empty() ? res::GetString(L"StatusTransportSettingsFailed") : errorMessage));
        }
        catch (...)
        {
        }

        co_return;
    }

    void MainWindow::UpdateManualConnectButton() noexcept
    {
        try
        {
            if (!m_loaded)
            {
                return;
            }

            auto const address = TextOf(ManualAddressTextBox());

            ManualConnectButton().IsEnabled(!address.empty() && PortFrom(ManualPortNumberBox()) != 0);
        }
        catch (...)
        {
        }
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnManualConnectClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        auto weak = get_weak();
        auto queue = DispatcherQueue();

        auto const address = TextOf(ManualAddressTextBox());
        auto const portValue = PortFrom(ManualPortNumberBox());

        if (address.empty() || portValue == 0)
        {
            co_return;
        }

        auto const port = portValue;

        auto name = TextOf(ManualNameTextBox());

        if (name.empty())
        {
            name = address;
        }

        auto const customName = TextOf(ManualCustomNameTextBox());

        ManualConnectButton().IsEnabled(false);
        SetRemoteStatus(res::FormatString(L"ConnectingToDeviceFormat", address));

        co_await winrt::resume_background();

        winrt::hstring message{};

        try
        {
            midi2net::MidiNetworkClientMatchCriteria criteria{};
            criteria.DirectHostNameOrIPAddress(address);
            criteria.DirectPort(port);

            midi2net::MidiNetworkClientConnectConfig config{};
            config.ClientId(foundation::GuidHelper::CreateNewGuid());
            config.UmpEndpointName(name);
            config.CustomEndpointName(customName);
            config.MatchCriteria(criteria);

            auto const response = co_await midi2net::MidiNetworkTransportManager::ConnectNetworkClientAsync(config);

            if (response != nullptr && response.Success())
            {
                if (!native::NetworkConfigFile::Current().MergeSection(config.ConfigJson()))
                {
                    message = native::NetworkConfigFile::Current().LastErrorMessage();
                }
                else
                {
                    message = res::FormatString(L"ConnectRequestedFormat", name);
                }
            }
            else
            {
                message = response == nullptr ?
                    res::GetString(L"ConnectFailedGeneral") :
                    res::FormatString(L"ConnectFailedFormat", response.ErrorMessage());
            }
        }
        catch (...)
        {
            message = res::GetString(L"ConnectFailedGeneral");
        }

        if (queue != nullptr)
        {
            queue.TryEnqueue([weak, message]()
                {
                    if (auto strong = weak.get())
                    {
                        strong->SetRemoteStatus(message);
                        strong->UpdateManualConnectButton();
                        strong->RequestRefreshAsync();
                    }
                });
        }
    }


    // ------------------------------------------------------------------------------------
    // answering invitations
    // ------------------------------------------------------------------------------------

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::AnswerInvitationAsync(
        midinetworksetup::PendingInvitationItem const item,
        bool const approve,
        bool const thisRequestOnly)
    {
        if (item == nullptr)
        {
            co_return;
        }

        auto weak = get_weak();
        auto queue = DispatcherQueue();

        auto const hostKey = item.HostId();
        auto const name = item.RemoteName();
        auto const productInstanceId = item.RemoteProductInstanceId();

        winrt::guid hostId{};

        if (!TryParseKey(hostKey, hostId))
        {
            co_return;
        }

        item.IsBusy(true);

        co_await winrt::resume_background();

        winrt::hstring message{};

        try
        {
            midi2net::MidiNetworkRemoteClientApprovalConfig config{
                hostId, name, productInstanceId, approve, thisRequestOnly };

            auto const response = co_await midi2net::MidiNetworkTransportManager::ApproveOrDenyRemoteClientConnectRequestAsync(config);

            if (response != nullptr && response.Success())
            {
                if (thisRequestOnly)
                {
                    message = approve ?
                        res::FormatString(L"InvitationAllowedOnceFormat", name) :
                        res::FormatString(L"InvitationDeniedOnceFormat", name);
                }
                else if (!native::NetworkConfigFile::Current().SetRemoteClientDecision(hostKey, name, productInstanceId, approve))
                {
                    message = native::NetworkConfigFile::Current().LastErrorMessage();
                }
                else
                {
                    message = approve ?
                        res::FormatString(L"InvitationAllowedAlwaysFormat", name) :
                        res::FormatString(L"InvitationBlockedFormat", name);
                }
            }
            else
            {
                message = response == nullptr ?
                    res::GetString(L"InvitationAnswerFailedGeneral") :
                    res::FormatString(L"InvitationAnswerFailedFormat", response.ErrorMessage());
            }
        }
        catch (...)
        {
            message = res::GetString(L"InvitationAnswerFailedGeneral");
        }

        if (queue != nullptr)
        {
            queue.TryEnqueue([weak, item, message]()
                {
                    item.IsBusy(false);

                    if (auto strong = weak.get())
                    {
                        strong->SetRemoteStatus(message);
                        strong->SetLocalStatus(message);
                        strong->RequestRefreshAsync();
                    }
                });
        }
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnAllowInvitationOnceClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const&)
    {
        AnswerInvitationAsync(ItemOf<midinetworksetup::PendingInvitationItem>(sender), true, true);

        co_return;
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnAllowInvitationAlwaysClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const&)
    {
        AnswerInvitationAsync(ItemOf<midinetworksetup::PendingInvitationItem>(sender), true, false);

        co_return;
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnDenyInvitationOnceClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const&)
    {
        AnswerInvitationAsync(ItemOf<midinetworksetup::PendingInvitationItem>(sender), false, true);

        co_return;
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnBlockInvitationClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const&)
    {
        AnswerInvitationAsync(ItemOf<midinetworksetup::PendingInvitationItem>(sender), false, false);

        co_return;
    }


    // ------------------------------------------------------------------------------------
    // page 2: hosts on this PC
    // ------------------------------------------------------------------------------------

    _Use_decl_annotations_
    void MainWindow::OnCreateHostFieldChanged(foundation::IInspectable const&, controls::TextChangedEventArgs const&)
    {
        UpdateCreateHostButtonState();
    }

    // The name check briefly listens to the network, so it happens once here rather than while
    // the customer is typing. Canceling the click keeps the dialog open with the field filled
    // in, so creating the host is one more click rather than a round trip through an error.
    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnCreateHostPrimaryButtonClick(
        controls::ContentDialog const& sender,
        controls::ContentDialogButtonClickEventArgs const& args)
    {
        auto const typed = TextOf(HostServiceInstanceNameTextBox());

        if (typed.empty())
        {
            co_return;
        }

        auto deferral = args.GetDeferral();

        // Captured before leaving the UI thread, so the continuation lands back on it.
        winrt::apartment_context uiThread;

        sender.IsPrimaryButtonEnabled(false);
        CreateHostStatusText().Text(res::GetString(L"HostCheckingServiceInstanceName"));

        co_await winrt::resume_background();

        bool available{ true };
        winrt::hstring suggestion{ };

        try
        {
            available = midi2net::MidiNetworkHostCreationConfig::IsServiceInstanceNameAvailable(typed);

            if (!available)
            {
                suggestion = midi2net::MidiNetworkHostCreationConfig::MakeUniqueServiceInstanceName(typed);
            }
        }
        catch (...)
        {
            // A check which could not run must not block the customer. The service still
            // refuses a duplicate, so this fails open.
            available = true;
        }

        co_await uiThread;

        if (!available && !suggestion.empty())
        {
            args.Cancel(true);

            HostServiceInstanceNameTextBox().Text(suggestion);

            CreateHostStatusText().Text(
                res::FormatString(L"HostServiceInstanceNameInUseFormat", suggestion));
        }
        else
        {
            CreateHostStatusText().Text(L"");
        }

        sender.IsPrimaryButtonEnabled(true);

        deferral.Complete();
    }

    _Use_decl_annotations_
    void MainWindow::OnCreateHostPortModeChanged(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        // XAML raises Checked while it is still applying the markup, when the rest of the
        // dialog's fields do not exist yet
        if (!m_loaded)
        {
            return;
        }

        try
        {
            auto const automatic = IsCheckBoxChecked(HostAutomaticPortCheckBox());

            HostPortNumberBox().IsEnabled(!automatic);
            HostAllowPortFallbackCheckBox().IsEnabled(!automatic);
        }
        catch (...)
        {
        }
    }

    void MainWindow::UpdateCreateHostButtonState() noexcept
    {
        if (!m_loaded)
        {
            return;
        }

        try
        {
            auto const name = TextOf(HostNameTextBox());
            auto const serviceInstanceName = TextOf(HostServiceInstanceNameTextBox());
            auto const productInstanceId = TextOf(HostProductInstanceIdTextBox());

            CreateHostDialog().IsPrimaryButtonEnabled(
                !name.empty() && !serviceInstanceName.empty() && !productInstanceId.empty());

            CreateHostStatusText().Text(
                productInstanceId.size() > 42 ? res::GetString(L"HostProductInstanceIdTooLong") : winrt::hstring{});
        }
        catch (...)
        {
        }
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnCreateHostClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        if (m_openDialog != nullptr)
        {
            co_return;
        }

        auto weak = get_weak();
        auto queue = DispatcherQueue();

        midi2net::MidiNetworkHostCreationConfig config{ nullptr };

        try
        {
            config = midi2net::MidiNetworkHostCreationConfig::CreateDefault();
        }
        catch (...)
        {
        }

        if (config == nullptr)
        {
            SetLocalStatus(res::GetString(L"CreateHostFailedGeneral"));

            co_return;
        }

        // the defaults are a good starting point, so they are what the customer sees
        HostNameTextBox().Text(config.Name());
        HostServiceInstanceNameTextBox().Text(config.ServiceInstanceName());
        HostProductInstanceIdTextBox().Text(config.ProductInstanceId());
        HostAdvertiseCheckBox().IsChecked(config.Advertise());
        HostCreateMidi1PortsCheckBox().IsChecked(!config.CreateOnlyUmpEndpoints());
        HostAutomaticPortCheckBox().IsChecked(config.UseAutomaticPortAllocation());
        HostPortNumberBox().IsEnabled(!config.UseAutomaticPortAllocation());
        HostAllowPortFallbackCheckBox().IsChecked(config.AllowPortFallback());
        HostAllowPortFallbackCheckBox().IsEnabled(!config.UseAutomaticPortAllocation());

        // CreateDefault has already generated a free port, so the customer sees the number they
        // are about to keep rather than an arbitrary placeholder.
        if (!config.UseAutomaticPortAllocation() && !config.ManuallyAssignedPort().empty())
        {
            try
            {
                HostPortNumberBox().Value(std::stod(std::wstring{ config.ManuallyAssignedPort() }));
            }
            CATCH_LOG();
        }
        HostPolicyAskRadio().IsChecked(true);
        CreateHostStatusText().Text(L"");

        UpdateCreateHostButtonState();

        CreateHostDialog().XamlRoot(Content().XamlRoot());

        m_openDialog = CreateHostDialog();

        auto const result = co_await CreateHostDialog().ShowAsync();

        m_openDialog = nullptr;

        if (result != controls::ContentDialogResult::Primary)
        {
            co_return;
        }

        try
        {
            config.Name(TextOf(HostNameTextBox()));
            config.ServiceInstanceName(
                midi2net::MidiNetworkHostCreationConfig::EnsureCompliantServiceInstanceName(TextOf(HostServiceInstanceNameTextBox())));
            config.ProductInstanceId(TextOf(HostProductInstanceIdTextBox()));
            config.Advertise(IsCheckBoxChecked(HostAdvertiseCheckBox()));
            config.CreateOnlyUmpEndpoints(!IsCheckBoxChecked(HostCreateMidi1PortsCheckBox()));

            auto const automaticPort = IsCheckBoxChecked(HostAutomaticPortCheckBox());

            config.UseAutomaticPortAllocation(automaticPort);

            if (!automaticPort)
            {
                if (auto const port = PortFrom(HostPortNumberBox()); port != 0)
                {
                    config.ManuallyAssignedPort(winrt::hstring{ std::format(L"{}", port) });
                }

                config.AllowPortFallback(IsCheckBoxChecked(HostAllowPortFallbackCheckBox()));
            }

            auto const askFirst = HostPolicyAskRadio().IsChecked();

            config.RemoteClientPolicy(
                askFirst != nullptr && askFirst.Value() ?
                midi2net::MidiNetworkRemoteClientPolicy::RequireApproval :
                midi2net::MidiNetworkRemoteClientPolicy::AllowAny);
        }
        catch (...)
        {
            SetLocalStatus(res::GetString(L"CreateHostFailedGeneral"));

            co_return;
        }

        auto const hostName = config.Name();

        SetLocalStatus(res::FormatString(L"CreatingHostFormat", hostName));

        co_await winrt::resume_background();

        winrt::hstring message{};

        try
        {
            auto const response = co_await midi2net::MidiNetworkTransportManager::CreateNetworkHostAsync(config);

            if (response != nullptr && response.Success())
            {
                if (!native::NetworkConfigFile::Current().MergeSection(config.ConfigJson()))
                {
                    message = native::NetworkConfigFile::Current().LastErrorMessage();
                }
                else
                {
                    message = res::FormatString(L"HostCreatedFormat", hostName);
                }
            }
            else
            {
                message = response == nullptr ?
                    res::GetString(L"CreateHostFailedGeneral") :
                    res::FormatString(L"CreateHostFailedFormat", response.ErrorMessage());
            }
        }
        catch (...)
        {
            message = res::GetString(L"CreateHostFailedGeneral");
        }

        if (queue != nullptr)
        {
            queue.TryEnqueue([weak, message]()
                {
                    if (auto strong = weak.get())
                    {
                        strong->SetLocalStatus(message);
                        strong->RequestRefreshAsync();
                    }
                });
        }
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnStartStopHostClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const&)
    {
        auto item = ItemOf<midinetworksetup::LocalHostItem>(sender);

        if (item == nullptr)
        {
            co_return;
        }

        winrt::guid hostId{};

        if (!TryParseKey(item.HostId(), hostId))
        {
            co_return;
        }

        auto weak = get_weak();
        auto queue = DispatcherQueue();

        auto const start = !item.HasStarted();
        auto const displayName = item.DisplayName();

        item.IsBusy(true);

        co_await winrt::resume_background();

        winrt::hstring message{};

        try
        {
            auto const response = start ?
                co_await midi2net::MidiNetworkTransportManager::StartNetworkHostAsync(hostId) :
                co_await midi2net::MidiNetworkTransportManager::StopNetworkHostAsync(hostId);

            if (response != nullptr && response.Success())
            {
                message = start ?
                    res::FormatString(L"HostStartedMessageFormat", displayName) :
                    res::FormatString(L"HostStoppedMessageFormat", displayName);
            }
            else
            {
                message = response == nullptr ?
                    res::GetString(L"HostChangeFailedGeneral") :
                    res::FormatString(L"HostChangeFailedFormat", response.ErrorMessage());
            }
        }
        catch (...)
        {
            message = res::GetString(L"HostChangeFailedGeneral");
        }

        if (queue != nullptr)
        {
            queue.TryEnqueue([weak, item, message]()
                {
                    item.IsBusy(false);

                    if (auto strong = weak.get())
                    {
                        strong->SetLocalStatus(message);
                        strong->RequestRefreshAsync();
                    }
                });
        }
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnDeleteHostClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const&)
    {
        auto item = ItemOf<midinetworksetup::LocalHostItem>(sender);

        if (item == nullptr)
        {
            co_return;
        }

        winrt::guid hostId{};

        auto const hostKey = item.HostId();

        if (!TryParseKey(hostKey, hostId))
        {
            co_return;
        }

        auto const displayName = item.DisplayName();

        if (!co_await ConfirmAsync(
            res::GetString(L"DeleteHostConfirmTitle"),
            res::FormatString(L"DeleteHostConfirmMessageFormat", displayName)))
        {
            co_return;
        }

        auto weak = get_weak();
        auto queue = DispatcherQueue();

        item.IsBusy(true);

        co_await winrt::resume_background();

        winrt::hstring message{};

        try
        {
            midi2net::MidiNetworkHostRemovalConfig config{};
            config.HostId(hostId);

            auto const response = co_await midi2net::MidiNetworkTransportManager::RemoveNetworkHostAsync(config);

            if (response != nullptr && response.Success())
            {
                if (!native::NetworkConfigFile::Current().RemoveHost(hostKey))
                {
                    message = native::NetworkConfigFile::Current().LastErrorMessage();
                }
                else
                {
                    message = res::FormatString(L"HostDeletedFormat", displayName);
                }
            }
            else
            {
                message = response == nullptr ?
                    res::GetString(L"HostChangeFailedGeneral") :
                    res::FormatString(L"HostChangeFailedFormat", response.ErrorMessage());
            }
        }
        catch (...)
        {
            message = res::GetString(L"HostChangeFailedGeneral");
        }

        if (queue != nullptr)
        {
            queue.TryEnqueue([weak, item, message]()
                {
                    item.IsBusy(false);

                    if (auto strong = weak.get())
                    {
                        strong->SetLocalStatus(message);
                        strong->RequestRefreshAsync();
                    }
                });
        }
    }


    // ------------------------------------------------------------------------------------
    // remote clients connected to a host on this PC
    // ------------------------------------------------------------------------------------

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::AnswerRemoteClientAsync(
        midinetworksetup::HostConnectionItem const item,
        bool const approve,
        bool const thisRequestOnly)
    {
        if (item == nullptr)
        {
            co_return;
        }

        auto weak = get_weak();
        auto queue = DispatcherQueue();

        auto const hostKey = item.HostId();
        auto const name = item.DisplayName();
        auto const productInstanceId = item.ProductInstanceId();

        winrt::guid hostId{};

        if (!TryParseKey(hostKey, hostId))
        {
            co_return;
        }

        item.IsBusy(true);

        co_await winrt::resume_background();

        winrt::hstring message{};

        try
        {
            // the service keys a remote client on the name it announced, which is what the
            // connection row carries
            midi2net::MidiNetworkRemoteClientApprovalConfig config{
                hostId, name, productInstanceId, approve, thisRequestOnly };

            auto const response = co_await midi2net::MidiNetworkTransportManager::ApproveOrDenyRemoteClientConnectRequestAsync(config);

            if (response != nullptr && response.Success())
            {
                if (thisRequestOnly)
                {
                    message = res::FormatString(L"RemoteClientDisconnectedFormat", name);
                }
                else if (!native::NetworkConfigFile::Current().SetRemoteClientDecision(hostKey, name, productInstanceId, approve))
                {
                    message = native::NetworkConfigFile::Current().LastErrorMessage();
                }
                else
                {
                    message = res::FormatString(L"RemoteClientBlockedFormat", name);
                }
            }
            else
            {
                message = response == nullptr ?
                    res::GetString(L"InvitationAnswerFailedGeneral") :
                    res::FormatString(L"InvitationAnswerFailedFormat", response.ErrorMessage());
            }
        }
        catch (...)
        {
            message = res::GetString(L"InvitationAnswerFailedGeneral");
        }

        if (queue != nullptr)
        {
            queue.TryEnqueue([weak, item, message]()
                {
                    item.IsBusy(false);

                    if (auto strong = weak.get())
                    {
                        strong->SetLocalStatus(message);
                        strong->RequestRefreshAsync();
                    }
                });
        }
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::DisconnectRemoteClientAsync(midinetworksetup::HostConnectionItem const item)
    {
        if (item == nullptr)
        {
            co_return;
        }

        auto weak = get_weak();
        auto queue = DispatcherQueue();

        auto const name = item.DisplayName();
        auto const productInstanceId = item.ProductInstanceId();

        winrt::guid hostId{};

        if (!TryParseKey(item.HostId(), hostId))
        {
            co_return;
        }

        item.IsBusy(true);

        co_await winrt::resume_background();

        winrt::hstring message{};

        try
        {
            // Ends the session and records nothing, so the device can connect again. Denying it
            // is a different answer and belongs to the Block button.
            midi2net::MidiNetworkRemoteClientDisconnectConfig config{ hostId, name, productInstanceId };

            auto const response = co_await midi2net::MidiNetworkTransportManager::DisconnectRemoteClientAsync(config);

            message = (response != nullptr && response.Success()) ?
                res::FormatString(L"RemoteClientDisconnectedFormat", name) :
                (response == nullptr ?
                    res::GetString(L"InvitationAnswerFailedGeneral") :
                    res::FormatString(L"InvitationAnswerFailedFormat", response.ErrorMessage()));
        }
        catch (...)
        {
            message = res::GetString(L"InvitationAnswerFailedGeneral");
        }

        if (queue != nullptr)
        {
            queue.TryEnqueue([weak, item, message]()
                {
                    item.IsBusy(false);

                    if (auto strong = weak.get())
                    {
                        strong->SetLocalStatus(message);
                        strong->RequestRefreshAsync();
                    }
                });
        }
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnDisconnectRemoteClientClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const&)
    {
        auto item = ItemOf<midinetworksetup::HostConnectionItem>(sender);

        if (item == nullptr)
        {
            co_return;
        }

        if (!co_await ConfirmAsync(
            res::GetString(L"DisconnectRemoteClientConfirmTitle"),
            res::FormatString(L"DisconnectRemoteClientConfirmMessageFormat", item.DisplayName())))
        {
            co_return;
        }

        DisconnectRemoteClientAsync(item);
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnBlockRemoteClientClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const&)
    {
        auto item = ItemOf<midinetworksetup::HostConnectionItem>(sender);

        if (item == nullptr)
        {
            co_return;
        }

        if (!co_await ConfirmAsync(
            res::GetString(L"BlockRemoteClientConfirmTitle"),
            res::FormatString(L"BlockRemoteClientConfirmMessageFormat", item.DisplayName())))
        {
            co_return;
        }

        AnswerRemoteClientAsync(item, false, false);
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnForgetKnownClientClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const&)
    {
        auto item = ItemOf<midinetworksetup::KnownClientItem>(sender);

        if (item == nullptr)
        {
            co_return;
        }

        auto weak = get_weak();
        auto queue = DispatcherQueue();

        auto const hostKey = item.HostId();
        auto const name = item.DisplayName();
        auto const productInstanceId = item.ProductInstanceId();

        co_await winrt::resume_background();

        winrt::hstring message{};

        try
        {
            if (native::NetworkConfigFile::Current().ForgetRemoteClient(hostKey, name, productInstanceId))
            {
                message = res::FormatString(L"KnownClientForgottenFormat", name);
            }
            else
            {
                message = native::NetworkConfigFile::Current().LastErrorMessage();
            }
        }
        catch (...)
        {
            message = res::GetString(L"KnownClientForgetFailedGeneral");
        }

        if (queue != nullptr)
        {
            queue.TryEnqueue([weak, message]()
                {
                    if (auto strong = weak.get())
                    {
                        strong->SetLocalStatus(message);
                        strong->RequestRefreshAsync();
                    }
                });
        }
    }
}
