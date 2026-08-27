// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MainWindow.xaml.h"

#include "StringResources.h"
#include "AppearanceFlyout.h"

namespace native = ::midibluetoothsetup;
namespace res = ::midibluetoothsetup::resources;

namespace winrt::midibluetoothsetup::implementation
{
    namespace
    {
        template <typename TItem>
        TItem ItemFromSender(_In_ foundation::IInspectable const& sender) noexcept
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

        void CopyToClipboard(_In_ winrt::hstring const& text) noexcept
        {
            try
            {
                if (text.empty())
                {
                    return;
                }

                winrt::Windows::ApplicationModel::DataTransfer::DataPackage package{};

                package.SetText(text);

                winrt::Windows::ApplicationModel::DataTransfer::Clipboard::SetContent(package);
            }
            catch (...)
            {
            }
        }

        winrt::hstring TrimmedText(_In_ winrt::hstring const& value) noexcept
        {
            try
            {
                std::wstring copy{ value };

                auto const first = copy.find_first_not_of(L" \t\r\n");

                if (first == std::wstring::npos)
                {
                    return {};
                }

                auto const last = copy.find_last_not_of(L" \t\r\n");

                return winrt::hstring{ copy.substr(first, last - first + 1) };
            }
            catch (...)
            {
                return value;
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
    }


    _Use_decl_annotations_
    void MainWindow::OnAlwaysOnTopToggled(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            auto const isChecked = AlwaysOnTopToggle().IsChecked();

            native::AppSettings::Current().AlwaysOnTop(isChecked != nullptr && isChecked.Value());

            m_chrome.ApplyAlwaysOnTop();
        }
        MIDI_BTSETUP_CATCH_AND_LOG(L"Unable to change the always on top setting.")
    }

    _Use_decl_annotations_
    void MainWindow::OnAppearanceButtonClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            midiapp::AppearanceStrings strings{};

            strings.Title = res::GetString(L"SettingsTitle");
            strings.ThemeLabel = res::GetString(L"SettingsThemeLabel");
            strings.ThemeSystem = res::GetString(L"SettingsThemeSystem");
            strings.ThemeLight = res::GetString(L"SettingsThemeLight");
            strings.ThemeDark = res::GetString(L"SettingsThemeDark");
            strings.BackdropLabel = res::GetString(L"SettingsBackdropLabel");
            strings.BackdropSolid = res::GetString(L"SettingsBackdropSolid");
            strings.BackdropMica = res::GetString(L"SettingsBackdropMica");
            strings.BackdropAcrylic = res::GetString(L"SettingsBackdropAcrylic");
            strings.CustomColorCheckBox = res::GetString(L"SettingsCustomColorCheckBox");
            strings.ColorPickerName = res::GetString(L"SettingsColorPickerName");

            // the refresh interval belongs with the other settings rather than in a panel of
            // its own, so it goes into the shared flyout's extra content slot
            controls::StackPanel extra{};
            extra.Spacing(4);
            extra.Margin(xaml::Thickness{ 0, 12, 0, 0 });

            controls::TextBlock label{};
            label.Text(res::GetString(L"SettingsRefreshIntervalLabel"));

            controls::NumberBox box{};
            box.Minimum(static_cast<double>(native::AppSettings::MinimumRefreshIntervalSeconds));
            box.Maximum(static_cast<double>(native::AppSettings::MaximumRefreshIntervalSeconds));
            box.Value(static_cast<double>(native::AppSettings::Current().RefreshIntervalSeconds()));
            box.SpinButtonPlacementMode(controls::NumberBoxSpinButtonPlacementMode::Compact);
            box.HorizontalAlignment(xaml::HorizontalAlignment::Left);
            box.Width(160);
            xaml::Automation::AutomationProperties::SetName(box, res::GetString(L"SettingsRefreshIntervalLabel"));

            box.ValueChanged([weak = get_weak()](controls::NumberBox const& sender, controls::NumberBoxValueChangedEventArgs const&)
                {
                    auto strong = weak.get();

                    if (strong == nullptr)
                    {
                        return;
                    }

                    auto const value = sender.Value();

                    // an empty NumberBox reports NaN
                    if (value != value)
                    {
                        return;
                    }

                    native::AppSettings::Current().RefreshIntervalSeconds(static_cast<uint32_t>(value));

                    strong->StartRefreshTimer();
                });

            controls::TextBlock help{};
            help.Text(res::GetString(L"SettingsRefreshIntervalHelp"));
            help.TextWrapping(xaml::TextWrapping::Wrap);
            help.FontSize(12);

            extra.Children().Append(label);
            extra.Children().Append(box);
            extra.Children().Append(help);

            midiapp::ShowAppearanceFlyout(
                AppearanceButton(),
                native::AppSettings::Current(),
                strings,
                [weak = get_weak()]()
                {
                    if (auto strong = weak.get())
                    {
                        strong->m_chrome.ApplyTheme();
                    }
                },
                extra);
        }
        MIDI_BTSETUP_CATCH_AND_LOG(L"Unable to open the settings.")
    }


    foundation::IAsyncOperation<bool> MainWindow::ConfirmAsync(winrt::hstring const& title, winrt::hstring const& message)
    {
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


    // ================================= Device actions =================================

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnConnectDeviceClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const&)
    {
        auto strongThis = get_strong();

        auto const item = ItemFromSender<midibluetoothsetup::BluetoothDeviceItem>(sender);

        if (item == nullptr || item.IsBusy())
        {
            co_return;
        }

        auto const deviceId = item.BluetoothDeviceId();
        auto const displayName = item.DisplayName();

        item.IsBusy(true);

        auto const dispatcher = DispatcherQueue();

        co_await winrt::resume_background();

        midi2bt::MidiBluetoothDeviceConnectResponse response{ nullptr };
        bool saved{ false };
        winrt::hstring saveError{};

        try
        {
            midi2bt::MidiBluetoothDeviceConnectConfig config{ deviceId };

            response = co_await midi2bt::MidiBluetoothTransportManager::ConnectDeviceAsync(config);

            // Connecting and remembering are separate steps, so a connection the service
            // rejected is never written to the configuration file.
            if (response != nullptr && response.Success())
            {
                auto const saveResponse = midi2svc::MidiServiceTransportPluginConfigManager::SaveUpdate(config);

                saved = saveResponse != nullptr && saveResponse.Success();

                if (!saved && saveResponse != nullptr)
                {
                    saveError = saveResponse.ErrorMessage();
                }
            }
        }
        catch (...)
        {
        }

        if (dispatcher == nullptr)
        {
            co_return;
        }

        dispatcher.TryEnqueue([this, strongThis, item, response, saved, saveError, displayName]()
            {
                try
                {
                    item.IsBusy(false);

                    if (response == nullptr)
                    {
                        SetDevicesStatus(res::GetString(L"StatusServiceDidNotRespond"));
                    }
                    else if (!response.Success())
                    {
                        SetDevicesStatus(res::FormatString(
                            L"StatusConnectFailedFormat",
                            displayName,
                            response.ErrorMessage()));
                    }
                    else if (!saved)
                    {
                        SetDevicesStatus(saveError.empty() ?
                            res::FormatString(L"StatusConnectedNotSavedFormat", displayName) :
                            res::FormatString(L"StatusConnectedSaveFailedFormat", displayName, saveError));
                    }
                    else if (response.IsKnown())
                    {
                        SetDevicesStatus(res::FormatString(L"StatusConnectingFormat", displayName));
                    }
                    else
                    {
                        // the request is remembered rather than performed, so saying "connecting"
                        // for a device which is not there would be a lie
                        SetDevicesStatus(res::FormatString(L"StatusRememberedFormat", displayName));
                    }

                    RequestRefreshAsync();
                }
                MIDI_BTSETUP_CATCH_AND_LOG(L"Unable to report the result of connecting a device.")
            });
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnDisconnectDeviceClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const&)
    {
        auto strongThis = get_strong();

        auto const item = ItemFromSender<midibluetoothsetup::BluetoothDeviceItem>(sender);

        if (item == nullptr || item.IsBusy())
        {
            co_return;
        }

        auto const deviceId = item.BluetoothDeviceId();
        auto const displayName = item.DisplayName();

        item.IsBusy(true);

        auto const dispatcher = DispatcherQueue();

        co_await winrt::resume_background();

        midi2bt::MidiBluetoothDeviceDisconnectResponse response{ nullptr };

        try
        {
            // Disconnecting alone lasts only for this session, so the entry is left in the file
            // but disabled. Forget is what takes it out.
            midi2bt::MidiBluetoothDeviceDisconnectConfig config{ deviceId, false };

            response = co_await midi2bt::MidiBluetoothTransportManager::DisconnectDeviceAsync(config);

            if (response != nullptr && response.Success())
            {
                midi2svc::MidiServiceTransportPluginConfigManager::SaveUpdate(config);
            }
        }
        catch (...)
        {
        }

        if (dispatcher == nullptr)
        {
            co_return;
        }

        dispatcher.TryEnqueue([this, strongThis, item, response, displayName]()
            {
                try
                {
                    item.IsBusy(false);

                    if (response == nullptr)
                    {
                        SetDevicesStatus(res::GetString(L"StatusServiceDidNotRespond"));
                    }
                    else if (!response.Success())
                    {
                        SetDevicesStatus(res::FormatString(
                            L"StatusDisconnectFailedFormat",
                            displayName,
                            response.ErrorMessage()));
                    }
                    else
                    {
                        SetDevicesStatus(res::FormatString(L"StatusDisconnectedFormat", displayName));
                    }

                    RequestRefreshAsync();
                }
                MIDI_BTSETUP_CATCH_AND_LOG(L"Unable to report the result of disconnecting a device.")
            });
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnForgetDeviceClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const&)
    {
        auto strongThis = get_strong();

        auto const item = ItemFromSender<midibluetoothsetup::BluetoothDeviceItem>(sender);

        if (item == nullptr || item.IsBusy())
        {
            co_return;
        }

        auto const deviceId = item.BluetoothDeviceId();
        auto const displayName = item.DisplayName();

        if (!co_await ConfirmAsync(
            res::GetString(L"ConfirmForgetTitle"),
            res::FormatString(L"ConfirmForgetMessageFormat", displayName)))
        {
            co_return;
        }

        item.IsBusy(true);

        auto const dispatcher = DispatcherQueue();

        co_await winrt::resume_background();

        bool succeeded{ false };
        winrt::hstring errorMessage{};

        try
        {
            midi2bt::MidiBluetoothDeviceDisconnectConfig config{ deviceId, true };

            auto const response = co_await midi2bt::MidiBluetoothTransportManager::DisconnectDeviceAsync(config);

            // Forgetting a device which is not connected is still meaningful, and that is
            // exactly when someone is most likely to want it.
            auto const disconnectHandled = response == nullptr ||
                response.Success() ||
                response.ErrorCode() == midi2bt::MidiBluetoothDeviceDisconnectErrorCode::NotConnected ||
                response.ErrorCode() == midi2bt::MidiBluetoothDeviceDisconnectErrorCode::DeviceNotDiscovered;

            if (disconnectHandled)
            {
                auto const saveResponse = midi2svc::MidiServiceTransportPluginConfigManager::SaveUpdate(config);

                succeeded = saveResponse != nullptr && saveResponse.Success();

                if (!succeeded && saveResponse != nullptr)
                {
                    errorMessage = saveResponse.ErrorMessage();
                }
            }
            else if (response != nullptr)
            {
                errorMessage = response.ErrorMessage();
            }
        }
        catch (...)
        {
        }

        if (dispatcher == nullptr)
        {
            co_return;
        }

        dispatcher.TryEnqueue([this, strongThis, item, succeeded, errorMessage, displayName]()
            {
                try
                {
                    item.IsBusy(false);

                    SetDevicesStatus(succeeded ?
                        res::FormatString(L"StatusForgottenFormat", displayName) :
                        res::FormatString(L"StatusForgetFailedFormat", displayName, errorMessage));

                    RequestRefreshAsync();
                }
                MIDI_BTSETUP_CATCH_AND_LOG(L"Unable to report the result of forgetting a device.")
            });
    }

    _Use_decl_annotations_
    void MainWindow::OnCopyEndpointDeviceIdClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const&)
    {
        try
        {
            auto const item = ItemFromSender<midibluetoothsetup::BluetoothDeviceItem>(sender);

            if (item == nullptr)
            {
                return;
            }

            CopyToClipboard(item.EndpointDeviceId());

            SetDevicesStatus(res::GetString(L"StatusEndpointIdCopied"));
        }
        MIDI_BTSETUP_CATCH_AND_LOG(L"Unable to copy the endpoint id.")
    }


    // ============================== Customization ==============================

    foundation::IAsyncOperation<bool> MainWindow::ShowCustomizeDialogAsync(
        winrt::hstring const& endpointDeviceInstanceId,
        winrt::hstring const& transportSuppliedName,
        winrt::hstring const& currentName,
        winrt::hstring const& currentDescription,
        winrt::hstring const& currentImage)
    {
        if (m_openDialog != nullptr || endpointDeviceInstanceId.empty())
        {
            co_return false;
        }

        auto strongThis = get_strong();

        controls::ContentDialogResult result{ controls::ContentDialogResult::None };

        try
        {
            CustomizeTransportNameText().Text(transportSuppliedName.empty() ?
                winrt::hstring{} :
                res::FormatString(L"CustomizeTransportNameFormat", transportSuppliedName));

            CustomizeNameBox().Text(currentName);
            CustomizeDescriptionBox().Text(currentDescription);
            CustomizeImageBox().Text(currentImage);

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

        auto const name = reset ? winrt::hstring{} : TrimmedText(CustomizeNameBox().Text());
        auto const description = reset ? winrt::hstring{} : TrimmedText(CustomizeDescriptionBox().Text());
        auto const image = reset ? winrt::hstring{} : TrimmedText(CustomizeImageBox().Text());

        if (!image.empty() && !IsBareFileName(image))
        {
            SetDevicesStatus(res::GetString(L"StatusImageMustBeFileName"));

            co_return false;
        }

        auto const dispatcher = DispatcherQueue();

        co_await winrt::resume_background();

        bool succeeded{ false };
        winrt::hstring errorMessage{};

        try
        {
            midi2svc::MidiServiceConfigEndpointMatchCriteria match{};
            match.DeviceInstanceId(endpointDeviceInstanceId);

            midi2svc::MidiServiceEndpointCustomizationConfig config{
                midi2bt::MidiBluetoothTransportManager::TransportId() };

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
                        midi2bt::MidiBluetoothTransportManager::TransportId(), match };

                    auto const saveResponse = midi2svc::MidiServiceTransportPluginConfigManager::SaveUpdate(removal);

                    succeeded = saveResponse != nullptr && saveResponse.Success();

                    if (!succeeded && saveResponse != nullptr)
                    {
                        errorMessage = saveResponse.ErrorMessage();
                    }
                }
                else
                {
                    auto const saveResponse = midi2svc::MidiServiceTransportPluginConfigManager::SaveUpdate(config);

                    succeeded = saveResponse != nullptr && saveResponse.Success();

                    if (!succeeded && saveResponse != nullptr)
                    {
                        errorMessage = saveResponse.ErrorMessage();
                    }
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

        // awaiting a WinRT async object restores the caller's apartment context, so the caller
        // is back on the UI thread without this having to marshal
        co_return succeeded;
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnCustomizeDeviceClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const&)
    {
        auto strongThis = get_strong();

        auto const item = ItemFromSender<midibluetoothsetup::BluetoothDeviceItem>(sender);

        if (item == nullptr || item.IsBusy())
        {
            co_return;
        }

        auto const succeeded = co_await ShowCustomizeDialogAsync(
            item.EndpointDeviceInstanceId(),
            item.DisplayName(),
            winrt::hstring{},
            winrt::hstring{},
            winrt::hstring{});

        try
        {
            SetDevicesStatus(succeeded ?
                res::GetString(L"StatusCustomizationSaved") :
                res::GetString(L"StatusCustomizationNotSaved"));

            RequestRefreshAsync();
        }
        MIDI_BTSETUP_CATCH_AND_LOG(L"Unable to report the result of customizing a device.")
    }


    // ================================ Peripheral actions ================================

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnStartPeripheralClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        auto strongThis = get_strong();

        auto const useMidi2 = PeripheralProtocolCombo().SelectedIndex() == 1;

        auto const dispatcher = DispatcherQueue();

        co_await winrt::resume_background();

        midi2bt::MidiBluetoothPeripheralResponse response{ nullptr };

        try
        {
            midi2bt::MidiBluetoothPeripheralConfig config{
                useMidi2 ?
                    midi2bt::MidiBluetoothProtocol::BluetoothLowEnergyMidi2Ump :
                    midi2bt::MidiBluetoothProtocol::BluetoothLowEnergyMidi1 };

            response = co_await midi2bt::MidiBluetoothTransportManager::StartPeripheralAsync(config);

            if (response != nullptr && response.Success())
            {
                midi2svc::MidiServiceTransportPluginConfigManager::SaveUpdate(config);
            }
        }
        catch (...)
        {
        }

        if (dispatcher == nullptr)
        {
            co_return;
        }

        dispatcher.TryEnqueue([this, strongThis, response, useMidi2]()
            {
                try
                {
                    if (response == nullptr)
                    {
                        SetPeripheralStatus(res::GetString(L"StatusServiceDidNotRespond"));
                    }
                    else if (!response.Success())
                    {
                        SetPeripheralStatus(res::FormatString(
                            L"StatusPeripheralStartFailedFormat",
                            response.ErrorMessage()));
                    }
                    else if (useMidi2)
                    {
                        SetPeripheralStatus(res::GetString(L"StatusPeripheralMidi2Warning"));
                    }
                    else
                    {
                        SetPeripheralStatus(res::GetString(L"StatusPeripheralStarted"));
                    }

                    RequestRefreshAsync();
                }
                MIDI_BTSETUP_CATCH_AND_LOG(L"Unable to report the result of publishing this PC.")
            });
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnStopPeripheralClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        auto strongThis = get_strong();

        if (!co_await ConfirmAsync(
            res::GetString(L"ConfirmStopPeripheralTitle"),
            res::GetString(L"ConfirmStopPeripheralMessage")))
        {
            co_return;
        }

        auto const dispatcher = DispatcherQueue();

        co_await winrt::resume_background();

        midi2bt::MidiBluetoothPeripheralResponse response{ nullptr };

        try
        {
            response = co_await midi2bt::MidiBluetoothTransportManager::StopPeripheralAsync();

            if (response != nullptr && response.Success())
            {
                // stopping only lasts for this session unless the configuration file says so too
                midi2bt::MidiBluetoothPeripheralConfig config{};
                config.IsEnabled(false);

                midi2svc::MidiServiceTransportPluginConfigManager::SaveUpdate(config);
            }
        }
        catch (...)
        {
        }

        if (dispatcher == nullptr)
        {
            co_return;
        }

        dispatcher.TryEnqueue([this, strongThis, response]()
            {
                try
                {
                    if (response == nullptr)
                    {
                        SetPeripheralStatus(res::GetString(L"StatusServiceDidNotRespond"));
                    }
                    else if (!response.Success())
                    {
                        SetPeripheralStatus(res::FormatString(
                            L"StatusPeripheralStopFailedFormat",
                            response.ErrorMessage()));
                    }
                    else
                    {
                        SetPeripheralStatus(res::GetString(L"StatusPeripheralStopped"));
                    }

                    RequestRefreshAsync();
                }
                MIDI_BTSETUP_CATCH_AND_LOG(L"Unable to report the result of stopping the peripheral.")
            });
    }

    // Approving has to create the endpoint and denying has to make sure one is not left behind,
    // so both go through here and then refresh.
    _Use_decl_annotations_
    foundation::IAsyncOperation<bool> MainWindow::DecideClientAsync(
        foundation::IInspectable const& sender,
        bool const approve,
        midi2bt::MidiBluetoothApprovalScope const scope) noexcept
    {
        auto strongThis = get_strong();

        auto const item = ItemFromSender<midibluetoothsetup::PendingClientItem>(sender);

        if (item == nullptr || item.IsBusy())
        {
            co_return false;
        }

        item.IsBusy(true);

        auto const address = item.BluetoothAddress();
        auto const dispatcher = DispatcherQueue();

        co_await winrt::resume_background();

        midi2bt::MidiBluetoothPeripheralClientDecisionResponse response{ nullptr };
        bool persisted{ false };

        try
        {
            response = approve ?
                co_await midi2bt::MidiBluetoothTransportManager::ApprovePeripheralClientAsync(address, scope) :
                co_await midi2bt::MidiBluetoothTransportManager::DenyPeripheralClientAsync(address, scope);

            // The service applies the decision immediately but never writes the configuration
            // file, so an "always" only survives a restart if it is recorded here. The lists are
            // stored whole, so the complete set is read back from the service and written out.
            if (response != nullptr && response.Success() && response.PersistRequired())
            {
                auto const status = midi2bt::MidiBluetoothTransportManager::GetPeripheralStatus();

                if (status != nullptr)
                {
                    midi2bt::MidiBluetoothPeripheralClientListConfig config{ status };

                    auto const saveResponse = midi2svc::MidiServiceTransportPluginConfigManager::SaveUpdate(config);

                    persisted = saveResponse != nullptr && saveResponse.Success();
                }
            }
        }
        catch (...)
        {
        }

        if (dispatcher == nullptr)
        {
            co_return false;
        }

        dispatcher.TryEnqueue([this, strongThis, item, response, approve, persisted]()
            {
                try
                {
                    item.IsBusy(false);

                    if (response == nullptr)
                    {
                        SetPeripheralStatus(res::GetString(L"StatusServiceDidNotRespond"));
                    }
                    else if (!response.Success())
                    {
                        SetPeripheralStatus(res::FormatString(
                            L"StatusClientDecisionFailedFormat",
                            response.ErrorMessage()));
                    }
                    else if (response.PersistRequired() && !persisted)
                    {
                        SetPeripheralStatus(res::FormatString(
                            L"StatusClientDecisionNotSavedFormat",
                            response.Name().empty() ? response.BluetoothAddress() : response.Name()));
                    }
                    else
                    {
                        SetPeripheralStatus(res::FormatString(
                            approve ? L"StatusClientApprovedFormat" : L"StatusClientDeniedFormat",
                            response.Name().empty() ? response.BluetoothAddress() : response.Name()));
                    }

                    RequestRefreshAsync();
                }
                MIDI_BTSETUP_CATCH_AND_LOG(L"Unable to report the result of the decision.")
            });

        co_return response != nullptr && response.Success();
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnAllowClientOnceClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const&)
    {
        co_await DecideClientAsync(sender, true, midi2bt::MidiBluetoothApprovalScope::Once);
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnAllowClientAlwaysClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const&)
    {
        co_await DecideClientAsync(sender, true, midi2bt::MidiBluetoothApprovalScope::Always);
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnDenyClientOnceClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const&)
    {
        co_await DecideClientAsync(sender, false, midi2bt::MidiBluetoothApprovalScope::Once);
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnBlockClientClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const&)
    {
        co_await DecideClientAsync(sender, false, midi2bt::MidiBluetoothApprovalScope::Always);
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnCustomizePeripheralClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const&)
    {
        auto strongThis = get_strong();

        auto const item = ItemFromSender<midibluetoothsetup::PeripheralClientItem>(sender);

        if (item == nullptr || item.IsBusy())
        {
            co_return;
        }

        auto const succeeded = co_await ShowCustomizeDialogAsync(
            item.EndpointDeviceInstanceId(),
            item.DisplayName(),
            winrt::hstring{},
            winrt::hstring{},
            winrt::hstring{});

        try
        {
            SetPeripheralStatus(succeeded ?
                res::GetString(L"StatusCustomizationSaved") :
                res::GetString(L"StatusCustomizationNotSaved"));

            RequestRefreshAsync();
        }
        MIDI_BTSETUP_CATCH_AND_LOG(L"Unable to report the result of customizing the connected device.")
    }

    _Use_decl_annotations_
    void MainWindow::OnCopyPeripheralEndpointDeviceIdClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const&)
    {
        try
        {
            auto const item = ItemFromSender<midibluetoothsetup::PeripheralClientItem>(sender);

            if (item == nullptr)
            {
                return;
            }

            CopyToClipboard(item.EndpointDeviceId());

            SetPeripheralStatus(res::GetString(L"StatusEndpointIdCopied"));
        }
        MIDI_BTSETUP_CATCH_AND_LOG(L"Unable to copy the endpoint id.")
    }
}
