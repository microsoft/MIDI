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
    winrt::fire_and_forget MainWindow::ConnectRemoteHostAsync(
        midinetworksetup::RemoteHostItem const item,
        bool const reuseExistingEntry,
        winrt::hstring const customEndpointName)
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

        if (!reuseExistingEntry || !TryParseKey(item.ClientId(), clientId))
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
            res::GetString(L"DisconnectConfirmTitle"),
            res::FormatString(L"DisconnectConfirmMessageFormat", displayName)))
        {
            co_return;
        }

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
                    message = res::FormatString(L"DisconnectedFormat", displayName);
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
            HostPortNumberBox().IsEnabled(!IsCheckBoxChecked(HostAutomaticPortCheckBox()));
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
