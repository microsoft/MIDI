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

#include "..\..\Transport\UdpNetworkMidi2Transport\network_json_defs.h"

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

        // Same focus problem as PortFrom, so Text is preferred here too. Anything unreadable or
        // out of range becomes the default rather than zero, which would mean no ports at all.
        uint8_t FallbackMidi1PortCountFrom(_In_ controls::NumberBox const& box) noexcept
        {
            try
            {
                std::wstring const text{ box.Text() };

                if (!text.empty() &&
                    std::all_of(text.begin(), text.end(), [](wchar_t const c) { return c >= L'0' && c <= L'9'; }))
                {
                    auto const parsed = std::stoul(text);

                    if (parsed >= MIDI_NETWORK_MIDI_FALLBACK_MIDI1_PORT_COUNT_MINIMUM &&
                        parsed <= MIDI_NETWORK_MIDI_FALLBACK_MIDI1_PORT_COUNT_MAXIMUM)
                    {
                        return static_cast<uint8_t>(parsed);
                    }
                }

                auto const value = box.Value();

                // NaN when the box is empty
                if (value == value &&
                    value >= MIDI_NETWORK_MIDI_FALLBACK_MIDI1_PORT_COUNT_MINIMUM &&
                    value <= MIDI_NETWORK_MIDI_FALLBACK_MIDI1_PORT_COUNT_MAXIMUM)
                {
                    return static_cast<uint8_t>(value);
                }

                return MIDI_NETWORK_MIDI_FALLBACK_MIDI1_PORT_COUNT_DEFAULT;
            }
            catch (...)
            {
                return MIDI_NETWORK_MIDI_FALLBACK_MIDI1_PORT_COUNT_DEFAULT;
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

        // RTP-MIDI names travel as DNS-SD labels, which hold 63 bytes of UTF-8
        constexpr int RtpNameMaxUtf8Bytes{ 63 };

        bool IsRtpNameTooLong(_In_ winrt::hstring const& value) noexcept
        {
            return !value.empty() &&
                ::WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr) > RtpNameMaxUtf8Bytes;
        }

        // an advertised RTP-MIDI name is a single DNS label
        winrt::hstring WithoutPeriods(_In_ winrt::hstring const& value) noexcept
        {
            try
            {
                std::wstring copy{ value };

                std::erase(copy, L'.');

                return winrt::hstring{ copy };
            }
            catch (...)
            {
                return value;
            }
        }

        // for a change the service has already made, but the configuration did not take
        winrt::hstring NotSavedMessage(_In_ midi2svc::MidiServiceConfigSaveResponse const& response) noexcept
        {
            return res::FormatString(
                L"ChangeNotSavedFormat",
                response == nullptr ? winrt::hstring{} : response.ErrorMessage());
        }

        // The saved Network MIDI 2.0 client with this entry identifier, or nullptr when there is none
        midi2net::MidiNetworkSavedClient FindSavedClient(_In_ winrt::hstring const& clientKey) noexcept
        {
            winrt::guid clientId{};

            if (!TryParseKey(clientKey, clientId))
            {
                return nullptr;
            }

            try
            {
                for (auto const& saved : midi2net::MidiNetworkTransportManager::GetSavedClients())
                {
                    if (saved != nullptr && saved.ClientId() == clientId)
                    {
                        return saved;
                    }
                }
            }
            catch (...)
            {
            }

            return nullptr;
        }

        // The service keys a remote client on its name and product instance id together, compared
        // without case, so the saved lists are matched the same way.
        bool IsSameRemoteClient(
            _In_ midi2net::MidiNetworkKnownRemoteClient const& known,
            _In_ winrt::hstring const& name,
            _In_ winrt::hstring const& productInstanceId) noexcept
        {
            auto const same = [](winrt::hstring const& left, winrt::hstring const& right) noexcept
                {
                    auto const trimmed = [](std::wstring_view const value) noexcept
                        {
                            auto const first = value.find_first_not_of(L" \t\r\n");

                            return first == std::wstring_view::npos ?
                                std::wstring_view{} :
                                value.substr(first, value.find_last_not_of(L" \t\r\n") - first + 1);
                        };

                    auto const a = trimmed(left);
                    auto const b = trimmed(right);

                    return a.size() == b.size() &&
                        (a.empty() || ::CompareStringOrdinal(
                            a.data(), static_cast<int>(a.size()),
                            b.data(), static_cast<int>(b.size()),
                            TRUE) == CSTR_EQUAL);
                };

            return
                same(known.RemoteClientName(), name) &&
                same(known.RemoteClientProductInstanceId(), productInstanceId);
        }

        // Saves the allow or deny decision about one remote client of a Network MIDI 2.0 host, or
        // forgets it when there is no decision. A save replaces the host's lists whole, so they are
        // rebuilt from what is saved with only this client changed. On failure, errorMessage is
        // the reason the save gave, which can be empty.
        bool SaveNetworkRemoteClientDecision(
            _In_ winrt::guid const& hostId,
            _In_ winrt::hstring const& name,
            _In_ winrt::hstring const& productInstanceId,
            _In_ std::optional<bool> const allowed,
            _Out_ winrt::hstring& errorMessage) noexcept
        {
            errorMessage = winrt::hstring{};

            try
            {
                midi2net::MidiNetworkSavedHost savedHost{ nullptr };

                for (auto const& host : midi2net::MidiNetworkTransportManager::GetSavedHosts())
                {
                    if (host != nullptr && host.HostId() == hostId)
                    {
                        savedHost = host;
                        break;
                    }
                }

                // A host which is not saved has no decisions to forget, and nowhere to keep a new
                // one. This is also what stops a host which could not be read from being saved
                // with every other decision missing.
                if (savedHost == nullptr)
                {
                    return !allowed.has_value();
                }

                midi2net::MidiNetworkHostKnownClientsConfig config{ hostId };

                bool found{ false };

                if (auto const known = savedHost.KnownRemoteClients())
                {
                    for (auto const& client : known)
                    {
                        if (client == nullptr)
                        {
                            continue;
                        }

                        // a client is on one list or the other, so its old entry is dropped
                        if (IsSameRemoteClient(client, name, productInstanceId))
                        {
                            found = true;
                            continue;
                        }

                        config.KnownClients().Append(client);
                    }
                }

                if (allowed.has_value())
                {
                    config.KnownClients().Append(
                        midi2net::MidiNetworkKnownRemoteClient{ name, productInstanceId, *allowed });
                }
                else if (!found)
                {
                    // nothing is saved about this client, so there is nothing to forget
                    return true;
                }

                auto const response = midi2svc::MidiServiceTransportPluginConfigManager::SaveUpdate(config);

                if (response != nullptr && response.Success())
                {
                    return true;
                }

                if (response != nullptr)
                {
                    errorMessage = response.ErrorMessage();
                }
            }
            catch (...)
            {
            }

            return false;
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
        try
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
        MIDI_NETSETUP_CATCH_AND_LOG(L"Unable to start connecting to a remote device.")
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
        try
        {
            ConnectRemoteHostAsync(ItemOf<midinetworksetup::RemoteHostItem>(sender), true, winrt::hstring{});
        }
        MIDI_NETSETUP_CATCH_AND_LOG(L"Unable to retry a remote device.")

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
        auto const savedClient = FindSavedClient(item.ClientId());
        auto const savedCustomName = savedClient != nullptr ? savedClient.CustomEndpointName() : winrt::hstring{};

        auto target = std::make_shared<midinetworksetup::RemoteHostItem>(nullptr);

        if (!co_await PromptForReassociateTargetAsync(entryName, target) || *target == nullptr)
        {
            co_return;
        }

        auto weak = get_weak();
        auto queue = DispatcherQueue();

        item.IsBusy(true);

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
                // saved, the same config removes the saved entry
                auto const saved = midi2svc::MidiServiceTransportPluginConfigManager::SaveUpdate(config);

                removed = saved != nullptr && saved.Success();
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
                auto const saved = midi2svc::MidiServiceTransportPluginConfigManager::SaveUpdate(config);

                message = saved != nullptr && saved.Success() ?
                    res::FormatString(L"ConnectRequestedFormat", displayName) :
                    NotSavedMessage(saved);
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
        winrt::hstring const endpointDeviceId,
        winrt::hstring const clientKey,
        bool const isRtpMidi,
        std::shared_ptr<winrt::hstring> errorMessage)
    {
        if (m_openDialog != nullptr || endpointDeviceId.empty())
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

        // Only the saved configuration records these two, so they are read before the endpoint
        // lookup rather than from the endpoint's properties.
        auto const savedClient = isRtpMidi ? midi2net::MidiNetworkSavedClient{ nullptr } : FindSavedClient(clientKey);

        bool const currentCreateMidi1Ports = savedClient == nullptr || !savedClient.CreateOnlyUmpEndpoints();

        auto const currentFallbackMidi1PortCount = savedClient == nullptr ?
            static_cast<uint8_t>(MIDI_NETWORK_MIDI_FALLBACK_MIDI1_PORT_COUNT_DEFAULT) :
            savedClient.FallbackMidi1PortCount();

        try
        {
            auto const info = midi2enum::MidiEndpointDeviceInformation::CreateFromEndpointDeviceId(
                endpointDeviceId);

            if (info == nullptr)
            {
                co_return false;
            }

            deviceInstanceId = info.DeviceInstanceId();

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
            CustomizeFallbackMidi1PortCountBox().Value(static_cast<double>(currentFallbackMidi1PortCount));
            CustomizeMidi1PortsPanel().Visibility(isRtpMidi ? xaml::Visibility::Collapsed : xaml::Visibility::Visible);

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
            if (isRtpMidi)
            {
                SetRtpRemoteStatus(res::GetString(L"StatusImageMustBeFileName"));
            }
            else
            {
                SetRemoteStatus(res::GetString(L"StatusImageMustBeFileName"));
            }

            co_return false;
        }

        auto const checkBoxState = CustomizeCreateMidi1PortsCheckBox().IsChecked();

        // Reset clears the display customization only. Taking the ports away as well would be a
        // destructive surprise from a button pressed to clear a name, and would not be undone by
        // pressing it again.
        auto const createMidi1Ports = reset ?
            currentCreateMidi1Ports :
            (checkBoxState != nullptr && checkBoxState.Value());

        auto const fallbackMidi1PortCount = reset ?
            currentFallbackMidi1PortCount :
            FallbackMidi1PortCountFrom(CustomizeFallbackMidi1PortCountBox());

        auto const transportId = isRtpMidi ?
            midi2rtp::MidiRtpTransportManager::TransportId() :
            midi2net::MidiNetworkTransportManager::TransportId();

        co_await winrt::resume_background();

        bool succeeded{ false };
        winrt::hstring failure{};

        try
        {
            midi2svc::MidiServiceConfigEndpointMatchCriteria match{};
            match.DeviceInstanceId(deviceInstanceId);

            midi2svc::MidiServiceEndpointCustomizationConfig config{ transportId };

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
                    midi2svc::MidiServiceEndpointCustomizationRemovalConfig removal{ transportId, match };

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

            // The port count reaches the running endpoint; the create flag is recorded for the
            // next connection, because whether an endpoint has MIDI 1.0 ports at all is settled
            // when the endpoint is built.
            if (succeeded && !isRtpMidi && !clientKey.empty() &&
                (createMidi1Ports != currentCreateMidi1Ports ||
                 fallbackMidi1PortCount != currentFallbackMidi1PortCount))
            {
                winrt::guid clientEntryId{};

                if (TryParseKey(clientKey, clientEntryId))
                {
                    midi2net::MidiNetworkClientUpdateConfig update{};

                    update.ClientId(clientEntryId);
                    update.CreateMidi1Ports(createMidi1Ports);
                    update.FallbackMidi1PortCount(fallbackMidi1PortCount);

                    auto const updateResponse = midi2svc::MidiServiceTransportPluginConfigManager::SendUpdate(update);

                    if (updateResponse != nullptr &&
                        updateResponse.Status() == midi2svc::MidiServiceConfigResponseStatus::Success)
                    {
                        auto const saveResponse = midi2svc::MidiServiceTransportPluginConfigManager::SaveUpdate(update);

                        succeeded = saveResponse != nullptr && saveResponse.Success();

                        if (!succeeded && saveResponse != nullptr)
                        {
                            failure = saveResponse.ErrorMessage();
                        }
                    }
                    else
                    {
                        succeeded = false;

                        if (updateResponse != nullptr)
                        {
                            failure = updateResponse.ServiceErrorMessage();
                        }
                    }
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

        auto const succeeded = co_await ShowCustomizeDialogAsync(item.EndpointDeviceId(), item.ClientId(), false, errorMessage);

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
            // An unpackaged app has no implicit window to parent a picker to.
            HWND handle{ nullptr };

            if (auto const native = try_as<::IWindowNative>())
            {
                LOG_IF_FAILED(native->get_WindowHandle(&handle));
            }

            if (handle == nullptr)
            {
                co_return;
            }

            // Runs its own modal loop and returns the answer directly. See ShowPicker for why
            // this is not the WinRT picker: this button lives inside an open ContentDialog.
            auto const chosen = midiapp::EndpointImageAssets::ShowPicker(handle);

            if (chosen.empty())
            {
                co_return;
            }

            auto const sourcePath = winrt::hstring{ chosen };

            // Copied rather than referenced, so the stored value is always a name inside the
            // shared folder and cannot break when the customer moves the original.
            auto const importedName = co_await ImportImageAsync(sourcePath);

            if (importedName.empty())
            {
                // the dialog belongs to whichever page is showing
                if (RtpRemoteHostsPanel().Visibility() == xaml::Visibility::Visible)
                {
                    SetRtpRemoteStatus(res::GetString(L"StatusImageCopyFailed"));
                }
                else
                {
                    SetRemoteStatus(res::GetString(L"StatusImageCopyFailed"));
                }

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
                // saved, the same config removes the saved entry, so the service does not
                // connect it again when it starts
                auto const saved = midi2svc::MidiServiceTransportPluginConfigManager::SaveUpdate(config);

                if (saved == nullptr || !saved.Success())
                {
                    message = NotSavedMessage(saved);
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
                auto const saved = midi2svc::MidiServiceTransportPluginConfigManager::SaveUpdate(config);

                message = saved != nullptr && saved.Success() ?
                    res::FormatString(L"ConnectRequestedFormat", name) :
                    NotSavedMessage(saved);
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
        auto const isRtpMidi = item.IsRtpMidi();

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
            if (isRtpMidi)
            {
                midi2rtp::MidiRtpRemoteClientApprovalConfig config{ hostId, name, approve, thisRequestOnly };

                auto const response = co_await midi2rtp::MidiRtpTransportManager::ApproveOrDenyRemoteClientConnectRequestAsync(config);

                winrt::hstring saveError{};

                if (response == nullptr || !response.Success())
                {
                    message = response == nullptr ?
                        res::GetString(L"InvitationAnswerFailedGeneral") :
                        res::FormatString(L"InvitationAnswerFailedFormat", response.ErrorMessage());
                }
                else if (thisRequestOnly)
                {
                    message = approve ?
                        res::FormatString(L"InvitationAllowedOnceFormat", name) :
                        res::FormatString(L"InvitationDeniedOnceFormat", name);
                }
                else if (!SaveRtpKnownClients(hostId, saveError))
                {
                    message = res::FormatString(L"DecisionNotSavedFormat", name, saveError);
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
                midi2net::MidiNetworkRemoteClientApprovalConfig config{
                    hostId, name, productInstanceId, approve, thisRequestOnly };

                auto const response = co_await midi2net::MidiNetworkTransportManager::ApproveOrDenyRemoteClientConnectRequestAsync(config);

                winrt::hstring saveError{};

                if (response != nullptr && response.Success())
                {
                    if (thisRequestOnly)
                    {
                        message = approve ?
                            res::FormatString(L"InvitationAllowedOnceFormat", name) :
                            res::FormatString(L"InvitationDeniedOnceFormat", name);
                    }
                    else if (!SaveNetworkRemoteClientDecision(hostId, name, productInstanceId, approve, saveError))
                    {
                        message = res::FormatString(L"DecisionNotSavedFormat", name, saveError);
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
                        // the bar sits above every page, so the answer is reported on each
                        strong->SetRemoteStatus(message);
                        strong->SetLocalStatus(message);
                        strong->SetRtpRemoteStatus(message);
                        strong->SetRtpLocalStatus(message);
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
        controls::ContentDialog sender,
        controls::ContentDialogButtonClickEventArgs args)
    {
        auto const typed = TextOf(HostServiceInstanceNameTextBox());

        if (typed.empty())
        {
            co_return;
        }

        auto strongThis = get_strong();
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
        HostFallbackMidi1PortCountBox().Value(static_cast<double>(config.FallbackMidi1PortCount()));
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
            config.FallbackMidi1PortCount(FallbackMidi1PortCountFrom(HostFallbackMidi1PortCountBox()));

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
                auto const saved = midi2svc::MidiServiceTransportPluginConfigManager::SaveUpdate(config);

                message = saved != nullptr && saved.Success() ?
                    res::FormatString(L"HostCreatedFormat", hostName) :
                    NotSavedMessage(saved);
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
                auto const saved = midi2svc::MidiServiceTransportPluginConfigManager::SaveUpdate(config);

                message = saved != nullptr && saved.Success() ?
                    res::FormatString(L"HostDeletedFormat", displayName) :
                    NotSavedMessage(saved);
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

            winrt::hstring saveError{};

            if (response != nullptr && response.Success())
            {
                if (thisRequestOnly)
                {
                    message = res::FormatString(L"RemoteClientDisconnectedFormat", name);
                }
                else if (!SaveNetworkRemoteClientDecision(hostId, name, productInstanceId, approve, saveError))
                {
                    message = res::FormatString(L"DecisionNotSavedFormat", name, saveError);
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
        try
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
        MIDI_NETSETUP_CATCH_AND_LOG(L"Unable to disconnect a remote client.")
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnBlockRemoteClientClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const&)
    {
        try
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
        MIDI_NETSETUP_CATCH_AND_LOG(L"Unable to block a remote client.")
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

        winrt::guid hostId{};

        if (!TryParseKey(hostKey, hostId))
        {
            co_return;
        }

        co_await winrt::resume_background();

        winrt::hstring message{};

        try
        {
            winrt::hstring saveError{};

            if (!SaveNetworkRemoteClientDecision(hostId, name, productInstanceId, std::nullopt, saveError))
            {
                message = saveError.empty() ? res::GetString(L"KnownClientForgetFailedGeneral") : saveError;
            }
            else
            {
                // Removing it from the saved lists only decides what the next service start reads.
                // The running service holds its own copy of the lists, so it has to be told as well.
                midi2net::MidiNetworkRemoteClientForgetConfig config{ hostId, name, productInstanceId };

                auto const response = co_await midi2net::MidiNetworkTransportManager::ForgetRemoteClientAsync(config);

                message = response != nullptr && response.Success() ?
                    res::FormatString(L"KnownClientForgottenFormat", name) :
                    res::FormatString(L"KnownClientForgottenUntilRestartFormat", name);
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


    // ------------------------------------------------------------------------------------
    // RTP-MIDI: connecting to remote devices
    // ------------------------------------------------------------------------------------

    _Use_decl_annotations_
    foundation::IAsyncOperation<bool> MainWindow::ConfirmRtpAlongsideNetworkMidi2Async(winrt::hstring const deviceName)
    {
        co_return co_await ConfirmAsync(
            res::GetString(L"RtpConnectAnywayTitle"),
            res::FormatString(L"RtpConnectAnywayMessageFormat", deviceName));
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnConnectRtpRemoteHostClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const&)
    {
        auto strongThis = get_strong();

        try
        {
            auto item = ItemOf<midinetworksetup::RtpRemoteHostItem>(sender);

            if (item == nullptr || item.ServiceInstanceName().empty())
            {
                co_return;
            }

            if (item.AlsoOffersNetworkMidi2() && !co_await ConfirmRtpAlongsideNetworkMidi2Async(item.DisplayName()))
            {
                co_return;
            }

            auto customName = std::make_shared<winrt::hstring>();

            if (!co_await PromptForConnectNameAsync(item.DisplayName(), customName))
            {
                co_return;
            }

            ConnectRtpRemoteHostAsync(item, *customName);
        }
        MIDI_NETSETUP_CATCH_AND_LOG(L"Unable to start connecting to an RTP-MIDI device.")
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::ConnectRtpRemoteHostAsync(
        midinetworksetup::RtpRemoteHostItem const item,
        winrt::hstring const customEndpointName)
    {
        if (item == nullptr)
        {
            co_return;
        }

        auto weak = get_weak();
        auto queue = DispatcherQueue();

        auto const displayName = item.DisplayName();
        auto const serviceInstanceName = item.ServiceInstanceName();

        item.IsBusy(true);
        SetRtpRemoteStatus(res::FormatString(L"ConnectingToDeviceFormat", displayName));

        co_await winrt::resume_background();

        winrt::hstring message{};

        try
        {
            // matched on the advertised name, so the service finds the device again at a new address
            midi2rtp::MidiRtpClientMatchCriteria criteria{};
            criteria.ServiceInstanceName(serviceInstanceName);

            midi2rtp::MidiRtpClientConnectConfig config{};
            config.CustomEndpointName(customEndpointName);
            config.MatchCriteria(criteria);

            auto const response = co_await midi2rtp::MidiRtpTransportManager::ConnectRtpClientAsync(config);

            if (response != nullptr && response.Success())
            {
                auto const saved = midi2svc::MidiServiceTransportPluginConfigManager::SaveUpdate(config);

                message = saved != nullptr && saved.Success() ?
                    res::FormatString(L"ConnectRequestedFormat", displayName) :
                    NotSavedMessage(saved);
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
                        strong->SetRtpRemoteStatus(message);
                        strong->RequestRefreshAsync();
                    }
                });
        }
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnRetryRtpRemoteHostClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const&)
    {
        auto item = ItemOf<midinetworksetup::RtpRemoteHostItem>(sender);

        winrt::guid clientId{};

        if (item == nullptr || !TryParseKey(item.ClientId(), clientId))
        {
            co_return;
        }

        auto weak = get_weak();
        auto queue = DispatcherQueue();

        auto const displayName = item.DisplayName();

        item.IsBusy(true);
        SetRtpRemoteStatus(res::FormatString(L"ConnectingToDeviceFormat", displayName));

        co_await winrt::resume_background();

        winrt::hstring message{};

        try
        {
            auto const response = co_await midi2rtp::MidiRtpTransportManager::ReconnectRtpClientAsync(clientId);

            if (response != nullptr && response.Success())
            {
                message = res::FormatString(L"ConnectRequestedFormat", displayName);
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
                        strong->SetRtpRemoteStatus(message);
                        strong->RequestRefreshAsync();
                    }
                });
        }
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnDisconnectRtpRemoteHostClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const&)
    {
        auto item = ItemOf<midinetworksetup::RtpRemoteHostItem>(sender);

        winrt::guid clientId{};

        if (item == nullptr || !TryParseKey(item.ClientId(), clientId))
        {
            co_return;
        }

        auto weak = get_weak();
        auto queue = DispatcherQueue();

        auto const displayName = item.DisplayName();
        auto const wasConnected = item.IsConnected();

        if (!co_await ConfirmAsync(
            wasConnected ?
                res::GetString(L"DisconnectConfirmTitle") :
                res::GetString(L"ForgetConfirmTitle"),
            wasConnected ?
                res::FormatString(L"DisconnectConfirmMessageFormat", displayName) :
                res::FormatString(L"ForgetConfirmMessageFormat", displayName)))
        {
            co_return;
        }

        item.IsBusy(true);

        co_await winrt::resume_background();

        winrt::hstring message{};

        try
        {
            midi2rtp::MidiRtpClientDisconnectConfig config{ clientId };

            auto const response = co_await midi2rtp::MidiRtpTransportManager::DisconnectRtpClientAsync(config);

            if (response != nullptr && response.Success())
            {
                auto const saved = midi2svc::MidiServiceTransportPluginConfigManager::SaveUpdate(config);

                if (saved != nullptr && saved.Success())
                {
                    message = wasConnected ?
                        res::FormatString(L"DisconnectedFormat", displayName) :
                        res::FormatString(L"ForgottenFormat", displayName);
                }
                else
                {
                    message = NotSavedMessage(saved);
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
            queue.TryEnqueue([weak, item, message]()
                {
                    item.IsBusy(false);

                    if (auto strong = weak.get())
                    {
                        strong->SetRtpRemoteStatus(message);
                        strong->RequestRefreshAsync();
                    }
                });
        }
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnCustomizeRtpRemoteHostClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const&)
    {
        auto item = ItemOf<midinetworksetup::RtpRemoteHostItem>(sender);

        if (item == nullptr)
        {
            co_return;
        }

        auto strongThis = get_strong();

        auto errorMessage = std::make_shared<winrt::hstring>();

        auto const succeeded = co_await ShowCustomizeDialogAsync(item.EndpointDeviceId(), item.ClientId(), true, errorMessage);

        try
        {
            if (succeeded)
            {
                SetRtpRemoteStatus(res::GetString(L"StatusCustomizationSaved"));
            }
            else
            {
                SetRtpRemoteStatus(errorMessage->empty() ?
                    res::GetString(L"StatusCustomizationNotSaved") :
                    *errorMessage);
            }

            RequestRefreshAsync();
        }
        MIDI_NETSETUP_CATCH_AND_LOG(L"Unable to report the result of customizing an RTP-MIDI device.")
    }

    _Use_decl_annotations_
    void MainWindow::OnCopyRtpEndpointDeviceIdClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const&)
    {
        auto item = ItemOf<midinetworksetup::RtpRemoteHostItem>(sender);

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

            SetRtpRemoteStatus(res::GetString(L"EndpointDeviceIdCopied"));
        }
        catch (...)
        {
            SetRtpRemoteStatus(res::GetString(L"EndpointDeviceIdCopyFailed"));
        }
    }

    _Use_decl_annotations_
    void MainWindow::OnMonitorRtpRemoteHostClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const&)
    {
        try
        {
            auto item = ItemOf<midinetworksetup::RtpRemoteHostItem>(sender);

            if (item == nullptr || item.EndpointDeviceId().empty())
            {
                return;
            }

            if (!midiapp::LaunchMonitorForEndpoint(item.EndpointDeviceId()))
            {
                SetRtpRemoteStatus(res::GetString(L"StatusMonitorNotAvailable"));
            }
        }
        MIDI_NETSETUP_CATCH_AND_LOG(L"Unable to open the MIDI monitor for an RTP-MIDI device.")
    }

    _Use_decl_annotations_
    void MainWindow::OnMonitorRtpHostConnectionClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const&)
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
                SetRtpLocalStatus(res::GetString(L"StatusMonitorNotAvailable"));
            }
        }
        MIDI_NETSETUP_CATCH_AND_LOG(L"Unable to open the MIDI monitor for a connected RTP-MIDI device.")
    }

    _Use_decl_annotations_
    void MainWindow::OnRtpManualConnectFieldChanged(foundation::IInspectable const&, controls::TextChangedEventArgs const&)
    {
        UpdateRtpManualConnectButton();
    }

    _Use_decl_annotations_
    void MainWindow::OnRtpManualConnectPortChanged(controls::NumberBox const&, controls::NumberBoxValueChangedEventArgs const&)
    {
        UpdateRtpManualConnectButton();
    }

    void MainWindow::UpdateRtpManualConnectButton() noexcept
    {
        try
        {
            if (!m_loaded)
            {
                return;
            }

            RtpManualConnectButton().IsEnabled(
                !TextOf(RtpManualAddressTextBox()).empty() && PortFrom(RtpManualPortNumberBox()) != 0);
        }
        catch (...)
        {
        }
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnRtpManualConnectClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        auto strongThis = get_strong();
        auto weak = get_weak();
        auto queue = DispatcherQueue();

        auto const address = TextOf(RtpManualAddressTextBox());
        auto const port = PortFrom(RtpManualPortNumberBox());

        if (address.empty() || port == 0)
        {
            co_return;
        }

        // empty means this PC's name
        auto const name = TextOf(RtpManualNameTextBox());
        auto const customName = TextOf(RtpManualCustomNameTextBox());

        if (IsNetworkMidi2Machine(address) && !co_await ConfirmRtpAlongsideNetworkMidi2Async(address))
        {
            co_return;
        }

        RtpManualConnectButton().IsEnabled(false);
        SetRtpRemoteStatus(res::FormatString(L"ConnectingToDeviceFormat", address));

        co_await winrt::resume_background();

        winrt::hstring message{};

        try
        {
            midi2rtp::MidiRtpClientMatchCriteria criteria{};
            criteria.DirectHostNameOrIPAddress(address);
            criteria.DirectPort(port);

            midi2rtp::MidiRtpClientConnectConfig config{};
            config.Name(name);
            config.CustomEndpointName(customName);
            config.MatchCriteria(criteria);

            auto const response = co_await midi2rtp::MidiRtpTransportManager::ConnectRtpClientAsync(config);

            if (response != nullptr && response.Success())
            {
                auto const saved = midi2svc::MidiServiceTransportPluginConfigManager::SaveUpdate(config);

                message = saved != nullptr && saved.Success() ?
                    res::FormatString(L"ConnectRequestedFormat", address) :
                    NotSavedMessage(saved);
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
                        strong->SetRtpRemoteStatus(message);
                        strong->UpdateRtpManualConnectButton();
                        strong->RequestRefreshAsync();
                    }
                });
        }
    }


    // ------------------------------------------------------------------------------------
    // RTP-MIDI: hosts on this PC
    // ------------------------------------------------------------------------------------

    _Use_decl_annotations_
    void MainWindow::OnCreateRtpHostFieldChanged(foundation::IInspectable const&, controls::TextChangedEventArgs const&)
    {
        UpdateCreateRtpHostButtonState();
    }

    _Use_decl_annotations_
    void MainWindow::OnCreateRtpHostPortModeChanged(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        // XAML raises Checked while it is still applying the markup, when the rest of the
        // dialog's fields do not exist yet
        if (!m_loaded)
        {
            return;
        }

        try
        {
            auto const automatic = IsCheckBoxChecked(RtpHostAutomaticPortCheckBox());

            RtpHostPortNumberBox().IsEnabled(!automatic);
            RtpHostAllowPortFallbackCheckBox().IsEnabled(!automatic);
        }
        catch (...)
        {
        }
    }

    // The service refuses a long name outright, and 63 bytes is fewer characters than it looks
    // outside of ASCII, so this says so while there is still something to fix.
    void MainWindow::UpdateCreateRtpHostButtonState() noexcept
    {
        if (!m_loaded)
        {
            return;
        }

        try
        {
            auto const tooLong =
                IsRtpNameTooLong(TextOf(RtpHostNameTextBox())) ||
                IsRtpNameTooLong(TextOf(RtpHostServiceInstanceNameTextBox()));

            CreateRtpHostDialog().IsPrimaryButtonEnabled(!tooLong);

            RtpCreateHostStatusText().Text(tooLong ? res::GetString(L"RtpHostNameTooLong") : winrt::hstring{});
        }
        catch (...)
        {
        }
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnCreateRtpHostClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        if (m_openDialog != nullptr)
        {
            co_return;
        }

        auto strongThis = get_strong();
        auto weak = get_weak();
        auto queue = DispatcherQueue();

        try
        {
            RtpHostNameTextBox().Text(L"");
            RtpHostServiceInstanceNameTextBox().Text(L"");
            RtpHostPolicyAskRadio().IsChecked(true);
            RtpHostAdvertiseCheckBox().IsChecked(true);
            RtpHostAutomaticPortCheckBox().IsChecked(true);
            RtpHostPortNumberBox().Value(static_cast<double>(midi2rtp::MidiRtpTransportManager::DefaultHostPort()));
            RtpHostPortNumberBox().IsEnabled(false);
            RtpHostAllowPortFallbackCheckBox().IsChecked(true);
            RtpHostAllowPortFallbackCheckBox().IsEnabled(false);
            RtpHostSendRecoveryJournalCheckBox().IsChecked(true);
            RtpCreateHostStatusText().Text(L"");

            UpdateCreateRtpHostButtonState();

            CreateRtpHostDialog().XamlRoot(Content().XamlRoot());
        }
        catch (...)
        {
            SetRtpLocalStatus(res::GetString(L"CreateHostFailedGeneral"));

            co_return;
        }

        m_openDialog = CreateRtpHostDialog();

        auto const result = co_await CreateRtpHostDialog().ShowAsync();

        m_openDialog = nullptr;

        if (result != controls::ContentDialogResult::Primary)
        {
            co_return;
        }

        midi2rtp::MidiRtpHostCreationConfig config{};
        winrt::hstring hostName{};

        try
        {
            auto const name = TextOf(RtpHostNameTextBox());
            auto serviceInstanceName = TextOf(RtpHostServiceInstanceNameTextBox());

            // the name is advertised when there is no separate one, so it has to follow the same rule
            if (serviceInstanceName.empty() && std::wstring_view{ name }.find(L'.') != std::wstring_view::npos)
            {
                serviceInstanceName = name;
            }

            config.Name(name);
            config.ServiceInstanceName(WithoutPeriods(serviceInstanceName));
            config.Advertise(IsCheckBoxChecked(RtpHostAdvertiseCheckBox()));

            auto const automaticPort = IsCheckBoxChecked(RtpHostAutomaticPortCheckBox());

            config.UseAutomaticPortAllocation(automaticPort);

            if (!automaticPort)
            {
                if (auto const port = PortFrom(RtpHostPortNumberBox()); port != 0)
                {
                    config.ManuallyAssignedPort(port);
                }

                config.AllowPortFallback(IsCheckBoxChecked(RtpHostAllowPortFallbackCheckBox()));
            }

            config.SendRecoveryJournal(IsCheckBoxChecked(RtpHostSendRecoveryJournalCheckBox()));

            auto const askFirst = RtpHostPolicyAskRadio().IsChecked();

            config.RemoteClientPolicy(
                askFirst != nullptr && askFirst.Value() ?
                midi2rtp::MidiRtpRemoteClientPolicy::RequireApproval :
                midi2rtp::MidiRtpRemoteClientPolicy::AllowAny);

            hostName = !name.empty() ? name :
                (!config.ServiceInstanceName().empty() ? config.ServiceInstanceName() : res::GetString(L"RtpHostDefaultName"));
        }
        catch (...)
        {
            SetRtpLocalStatus(res::GetString(L"CreateHostFailedGeneral"));

            co_return;
        }

        SetRtpLocalStatus(res::FormatString(L"CreatingHostFormat", hostName));

        co_await winrt::resume_background();

        winrt::hstring message{};

        try
        {
            auto const response = co_await midi2rtp::MidiRtpTransportManager::CreateRtpHostAsync(config);

            // A host which is slow to start is still created, and the service keeps trying, so it
            // is saved like any other and its row says why it has not started.
            auto const created = response != nullptr &&
                (response.Success() ||
                 response.ErrorCode() == midi2rtp::MidiRtpHostCreationErrorCode::TimedOutWaitingForHostToStart);

            if (created)
            {
                auto const saved = midi2svc::MidiServiceTransportPluginConfigManager::SaveUpdate(config);

                if (saved == nullptr || !saved.Success())
                {
                    message = NotSavedMessage(saved);
                }
                else
                {
                    message = response.Success() ?
                        res::FormatString(L"HostCreatedFormat", hostName) :
                        res::FormatString(L"RtpHostCreatedNotStartedFormat", hostName);
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
                        strong->SetRtpLocalStatus(message);
                        strong->RequestRefreshAsync();
                    }
                });
        }
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnStartStopRtpHostClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const&)
    {
        auto item = ItemOf<midinetworksetup::LocalHostItem>(sender);

        winrt::guid hostId{};

        if (item == nullptr || !TryParseKey(item.HostId(), hostId))
        {
            co_return;
        }

        auto weak = get_weak();
        auto queue = DispatcherQueue();

        // an RTP-MIDI row reports whether the host is switched on here, not whether it is running
        auto const start = !item.HasStarted();
        auto const displayName = item.DisplayName();

        item.IsBusy(true);

        co_await winrt::resume_background();

        winrt::hstring message{};

        try
        {
            auto const response = start ?
                co_await midi2rtp::MidiRtpTransportManager::StartRtpHostAsync(hostId) :
                co_await midi2rtp::MidiRtpTransportManager::StopRtpHostAsync(hostId);

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
                        strong->SetRtpLocalStatus(message);
                        strong->RequestRefreshAsync();
                    }
                });
        }
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnDeleteRtpHostClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const&)
    {
        auto item = ItemOf<midinetworksetup::LocalHostItem>(sender);

        winrt::guid hostId{};

        if (item == nullptr || !TryParseKey(item.HostId(), hostId))
        {
            co_return;
        }

        auto weak = get_weak();
        auto queue = DispatcherQueue();

        auto const displayName = item.DisplayName();

        if (!co_await ConfirmAsync(
            res::GetString(L"DeleteHostConfirmTitle"),
            res::FormatString(L"DeleteHostConfirmMessageFormat", displayName)))
        {
            co_return;
        }

        item.IsBusy(true);

        co_await winrt::resume_background();

        winrt::hstring message{};

        try
        {
            midi2rtp::MidiRtpHostRemovalConfig config{ hostId };

            auto const response = co_await midi2rtp::MidiRtpTransportManager::RemoveRtpHostAsync(config);

            if (response != nullptr && response.Success())
            {
                // this takes the host's remembered decisions out of the file as well
                auto const saved = midi2svc::MidiServiceTransportPluginConfigManager::SaveUpdate(config);

                message = saved != nullptr && saved.Success() ?
                    res::FormatString(L"HostDeletedFormat", displayName) :
                    NotSavedMessage(saved);
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
                        strong->SetRtpLocalStatus(message);
                        strong->RequestRefreshAsync();
                    }
                });
        }
    }


    // ------------------------------------------------------------------------------------
    // RTP-MIDI: remotes connected to a host on this PC
    // ------------------------------------------------------------------------------------

    _Use_decl_annotations_
    bool MainWindow::SaveRtpKnownClients(winrt::guid const& hostId, winrt::hstring& errorMessage) noexcept
    {
        errorMessage = winrt::hstring{};

        try
        {
            midi2rtp::MidiRtpHostKnownClientsConfig config{ hostId };

            auto const hosts = midi2rtp::MidiRtpTransportManager::GetConfiguredHosts();

            bool found{ false };

            if (hosts != nullptr)
            {
                for (auto const& host : hosts)
                {
                    if (host == nullptr || host.HostId() != hostId)
                    {
                        continue;
                    }

                    found = true;

                    if (auto const known = host.KnownRemoteClients())
                    {
                        for (auto const& client : known)
                        {
                            if (client != nullptr)
                            {
                                config.KnownClients().Append(client);
                            }
                        }
                    }

                    break;
                }
            }

            // The save replaces the host's lists outright, so a host which could not be read must
            // not be written as one with no decisions at all.
            if (!found)
            {
                return false;
            }

            auto const response = midi2svc::MidiServiceTransportPluginConfigManager::SaveUpdate(config);

            if (response != nullptr && response.Success())
            {
                return true;
            }

            if (response != nullptr)
            {
                errorMessage = response.ErrorMessage();
            }

            return false;
        }
        catch (...)
        {
            return false;
        }
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnDisconnectRtpRemoteClientClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const&)
    {
        auto item = ItemOf<midinetworksetup::HostConnectionItem>(sender);

        winrt::guid hostId{};

        if (item == nullptr || item.ConnectionId() == 0 || !TryParseKey(item.HostId(), hostId))
        {
            co_return;
        }

        auto weak = get_weak();
        auto queue = DispatcherQueue();

        auto const displayName = item.DisplayName();
        auto const connectionId = item.ConnectionId();

        if (!co_await ConfirmAsync(
            res::GetString(L"DisconnectRemoteClientConfirmTitle"),
            res::FormatString(L"DisconnectRemoteClientConfirmMessageFormat", displayName)))
        {
            co_return;
        }

        item.IsBusy(true);

        co_await winrt::resume_background();

        winrt::hstring message{};

        try
        {
            // Ends the session and records nothing, so the device can connect again. Refusing it
            // is a different answer and belongs to the Block button.
            midi2rtp::MidiRtpRemoteClientDisconnectConfig config{ hostId, connectionId };

            auto const response = co_await midi2rtp::MidiRtpTransportManager::DisconnectRemoteClientAsync(config);

            if (response != nullptr && response.Success())
            {
                message = res::FormatString(L"RemoteClientDisconnectedFormat", displayName);
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
            queue.TryEnqueue([weak, item, message]()
                {
                    item.IsBusy(false);

                    if (auto strong = weak.get())
                    {
                        strong->SetRtpLocalStatus(message);
                        strong->RequestRefreshAsync();
                    }
                });
        }
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnBlockRtpRemoteClientClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const&)
    {
        auto item = ItemOf<midinetworksetup::HostConnectionItem>(sender);

        winrt::guid hostId{};

        if (item == nullptr || !TryParseKey(item.HostId(), hostId))
        {
            co_return;
        }

        auto strongThis = get_strong();
        auto weak = get_weak();
        auto queue = DispatcherQueue();

        // a remote is known by the name it sends, so one which sent none cannot be told apart
        auto const remoteName = winrt::get_self<HostConnectionItem>(item)->RemoteName();

        if (remoteName.empty())
        {
            SetRtpLocalStatus(res::GetString(L"RtpBlockUnnamedNotPossible"));

            co_return;
        }

        if (!co_await ConfirmAsync(
            res::GetString(L"BlockRemoteClientConfirmTitle"),
            res::FormatString(L"BlockRemoteClientConfirmMessageFormat", remoteName)))
        {
            co_return;
        }

        item.IsBusy(true);

        co_await winrt::resume_background();

        winrt::hstring message{};

        try
        {
            // Refusing a remote also ends the connection it has now
            midi2rtp::MidiRtpRemoteClientApprovalConfig config{ hostId, remoteName, false, false };

            auto const response = co_await midi2rtp::MidiRtpTransportManager::ApproveOrDenyRemoteClientConnectRequestAsync(config);

            winrt::hstring saveError{};

            if (response == nullptr || !response.Success())
            {
                message = response == nullptr ?
                    res::GetString(L"InvitationAnswerFailedGeneral") :
                    res::FormatString(L"InvitationAnswerFailedFormat", response.ErrorMessage());
            }
            else if (!SaveRtpKnownClients(hostId, saveError))
            {
                message = res::FormatString(L"DecisionNotSavedFormat", remoteName, saveError);
            }
            else
            {
                message = res::FormatString(L"RemoteClientBlockedFormat", remoteName);
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
                        strong->SetRtpLocalStatus(message);
                        strong->RequestRefreshAsync();
                    }
                });
        }
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnForgetRtpKnownClientClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const&)
    {
        auto item = ItemOf<midinetworksetup::KnownClientItem>(sender);

        winrt::guid hostId{};

        if (item == nullptr || !TryParseKey(item.HostId(), hostId))
        {
            co_return;
        }

        auto weak = get_weak();
        auto queue = DispatcherQueue();

        auto const name = item.DisplayName();

        co_await winrt::resume_background();

        winrt::hstring message{};

        try
        {
            midi2rtp::MidiRtpRemoteClientForgetConfig config{ hostId, name };

            auto const response = co_await midi2rtp::MidiRtpTransportManager::ForgetRemoteClientAsync(config);

            winrt::hstring saveError{};

            if (response == nullptr || !response.Success())
            {
                message = res::GetString(L"KnownClientForgetFailedGeneral");
            }
            else if (!SaveRtpKnownClients(hostId, saveError))
            {
                message = res::FormatString(L"ChangeNotSavedFormat", saveError);
            }
            else
            {
                message = res::FormatString(L"KnownClientForgottenFormat", name);
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
                        strong->SetRtpLocalStatus(message);
                        strong->RequestRefreshAsync();
                    }
                });
        }
    }
}
