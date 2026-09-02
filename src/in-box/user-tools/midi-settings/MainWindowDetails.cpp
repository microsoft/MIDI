// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MainWindow.xaml.h"

#include "BackgroundWork.h"
#include "MidiPanic.h"
#include "StringResources.h"

namespace native = ::midisettings;
namespace res = ::midisettings::resources;

namespace winrt::midisettings::implementation
{
    namespace
    {
        winrt::hstring TrimmedOrEmpty(winrt::hstring const& value) noexcept
        {
            std::wstring text{ value };

            while (!text.empty() && ::iswspace(text.front()))
            {
                text.erase(text.begin());
            }

            while (!text.empty() && ::iswspace(text.back()))
            {
                text.pop_back();
            }

            return winrt::hstring{ text };
        }

        winrt::hstring DescribeGroupAndNumber(midi2legacy::MidiLegacyPortDeviceInformation const& port) noexcept
        {
            try
            {
                // Group is zero based internally and one based everywhere a musician sees it.
                return res::FormatString(
                    L"Midi1PortDetailFormat",
                    static_cast<int32_t>(port.Group() == nullptr ? 0 : port.Group().Index() + 1),
                    static_cast<int32_t>(port.Number()));
            }
            catch (...)
            {
                return {};
            }
        }
    }

    // ------------------------------------------------------------------------------------
    // Row actions
    // ------------------------------------------------------------------------------------

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnEndpointClick(
        foundation::IInspectable const&,
        controls::ItemClickEventArgs const& args)
    {
        auto lifetime = get_strong();

        try
        {
            auto const item = args.ClickedItem().try_as<midisettings::EndpointItem>();

            if (item == nullptr)
            {
                co_return;
            }

            co_await ShowEndpointDetailAsync(item.EndpointDeviceId());
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to show the endpoint details.")
    }

    _Use_decl_annotations_
    void MainWindow::OnEndpointMonitorClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const&)
    {
        try
        {
            auto const button = sender.try_as<controls::Button>();

            if (button == nullptr)
            {
                return;
            }

            auto const endpointDeviceId = winrt::unbox_value_or<winrt::hstring>(button.Tag(), winrt::hstring{});

            if (!native::LaunchMonitorForEndpoint(endpointDeviceId))
            {
                StatusText().Text(res::GetString(L"MonitorLaunchFailed"));
            }
            else
            {
                StatusText().Text({});
            }
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to start the monitor.")
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnEndpointPanicClick(
        foundation::IInspectable const& sender,
        xaml::RoutedEventArgs const&)
    {
        auto lifetime = get_strong();

        try
        {
            auto const button = sender.try_as<controls::Button>();

            if (button == nullptr)
            {
                co_return;
            }

            SendPanicAsync(winrt::unbox_value_or<winrt::hstring>(button.Tag(), winrt::hstring{}));
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to send MIDI panic.")

        co_return;
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::SendPanicAsync(winrt::hstring endpointDeviceId) noexcept
    {
        auto lifetime = get_strong();

        try
        {
            if (endpointDeviceId.empty())
            {
                co_return;
            }

            auto const endpoint = FindEndpoint(endpointDeviceId);

            if (endpoint == nullptr)
            {
                co_return;
            }

            auto const name = endpoint.Name();

            // Only the groups the endpoint declares. Blasting all sixteen would send messages
            // to groups the device does not have, and is what pushed the list past the size
            // the service accepts in one send.
            std::vector<uint8_t> groups{};

            auto const declared = midiapp::DeclaredGroups(endpoint);

            for (uint8_t i = 0; i < declared.size(); i++)
            {
                if (declared[i])
                {
                    groups.push_back(i);
                }
            }

            std::wstring errorMessage{};
            bool succeeded{ false };

            co_await native::RunOnBackgroundAsync([&succeeded, &errorMessage, endpointDeviceId, groups]()
                {
                    succeeded = native::SendMidiPanic(endpointDeviceId, groups, errorMessage);
                });

            if (m_closing)
            {
                co_return;
            }

            auto const message = succeeded ?
                res::FormatString(L"PanicSentFormat", name) :
                res::FormatString(L"PanicFailedFormat", winrt::hstring{ errorMessage });

            StatusText().Text(message);

            if (m_detailDialogOpen)
            {
                DetailStatusText().Text(message);
            }
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to send MIDI panic.")
    }

    // ------------------------------------------------------------------------------------
    // Endpoint details
    // ------------------------------------------------------------------------------------

    _Use_decl_annotations_
    foundation::IAsyncAction MainWindow::ShowEndpointDetailAsync(winrt::hstring endpointDeviceId)
    {
        auto lifetime = get_strong();

        try
        {
            auto const endpoint = FindEndpoint(endpointDeviceId);

            if (endpoint == nullptr)
            {
                co_return;
            }

            m_detailEndpointId = endpointDeviceId;
            m_customizeRequested = false;

            PopulateDetail(endpoint);
            RefreshDetailPorts();

            EndpointDetailDialog().XamlRoot(Content().XamlRoot());

            m_detailDialogOpen = true;

            co_await EndpointDetailDialog().ShowAsync();

            m_detailDialogOpen = false;
            m_detailEndpointId = {};

            if (m_closing)
            {
                co_return;
            }

            // The customize button closes this dialog first, because only one content dialog
            // can be open at a time. Coming back afterwards is what makes the edit feel like
            // part of the same task rather than a dead end.
            if (m_customizeRequested)
            {
                m_customizeRequested = false;

                co_await ShowCustomizeDialogAsync(endpointDeviceId);

                if (!m_closing)
                {
                    co_await ShowEndpointDetailAsync(endpointDeviceId);
                }
            }
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to show the endpoint details.")
    }

    _Use_decl_annotations_
    void MainWindow::PopulateDetail(midi2enum::MidiEndpointDeviceInformation const& endpoint) noexcept
    {
        try
        {
            DetailStatusText().Text({});

            EndpointDetailDialog().Title(winrt::box_value(endpoint.Name()));

            winrt::hstring description{};
            winrt::hstring imageFileName{};

            if (auto const userInfo = endpoint.GetUserSuppliedInfo())
            {
                description = TrimmedOrEmpty(userInfo.Description());
                imageFileName = userInfo.ImageFileName();
            }

            winrt::hstring serialNumber{};
            winrt::hstring transportText{};

            if (auto const transportInfo = endpoint.GetTransportSuppliedInfo())
            {
                // What the customer wrote wins; the transport's own words are better than nothing.
                if (description.empty())
                {
                    description = TrimmedOrEmpty(transportInfo.Description());
                }

                serialNumber = TrimmedOrEmpty(transportInfo.SerialNumber());

                transportText = res::FormatString(
                    L"TransportWithCodeFormat",
                    TransportDisplayName(transportInfo.TransportCode()),
                    transportInfo.TransportCode());

                DetailNativeFormatText().Text(
                    transportInfo.NativeDataFormat() == midi2enum::MidiEndpointNativeDataFormat::UniversalMidiPacketFormat ?
                    res::GetString(L"NativeFormatUmp") :
                    transportInfo.NativeDataFormat() == midi2enum::MidiEndpointNativeDataFormat::Midi1ByteFormat ?
                    res::GetString(L"NativeFormatBytes") :
                    res::GetString(L"NativeFormatUnknown"));
            }

            DetailDescriptionText().Text(description);
            DetailDescriptionText().Visibility(description.empty() ?
                xaml::Visibility::Collapsed : xaml::Visibility::Visible);

            auto const imagePath = ResolveEndpointImage(endpoint);

            if (imagePath.empty())
            {
                DetailImage().Source(nullptr);
            }
            else if (midiapp::EndpointImageAssets::IsScalableVector(std::wstring{ imagePath }))
            {
                media::Imaging::SvgImageSource source{};

                source.RasterizePixelHeight(176);
                source.UriSource(foundation::Uri{ L"file:///" + imagePath });

                DetailImage().Source(source);
            }
            else
            {
                media::Imaging::BitmapImage bitmap{};

                bitmap.DecodePixelHeight(176);
                bitmap.UriSource(foundation::Uri{ L"file:///" + imagePath });

                DetailImage().Source(bitmap);
            }

            DetailTransportText().Text(transportText);

            winrt::hstring productInstanceId{};

            if (auto const declared = endpoint.GetDeclaredEndpointInfo())
            {
                productInstanceId = TrimmedOrEmpty(declared.ProductInstanceId());
            }

            auto const notSupplied = res::GetString(L"ValueNotSupplied");

            DetailProductInstanceIdText().Text(productInstanceId.empty() ? notSupplied : productInstanceId);
            DetailSerialNumberText().Text(serialNumber.empty() ? notSupplied : serialNumber);

            DetailEndpointIdText().Text(endpoint.EndpointDeviceId());

            // the value is ellipsized when it does not fit, so the whole of it lives here too
            controls::ToolTipService::SetToolTip(
                DetailEndpointIdText(), winrt::box_value(endpoint.EndpointDeviceId()));

            // Without one of these the endpoint id changes when the device is plugged into a
            // different port, and any customization stored against it stops matching.
            auto const hasUniqueId = !productInstanceId.empty() || !serialNumber.empty();

            DetailUniqueIdBar().Severity(hasUniqueId ?
                controls::InfoBarSeverity::Success : controls::InfoBarSeverity::Warning);

            DetailUniqueIdBar().Message(hasUniqueId ?
                res::GetString(L"UniqueIdPresent") : res::GetString(L"UniqueIdMissing"));

            DetailMonitorButton().IsEnabled(native::IsMonitoringAvailable());
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to show the endpoint properties.")
    }

    void MainWindow::RefreshDetailPorts() noexcept
    {
        try
        {
            if (m_closing || m_detailEndpointId.empty())
            {
                return;
            }

            auto const fill = [](collections::IObservableVector<midisettings::Midi1PortItem> const& target,
                collections::IVectorView<midi2legacy::MidiLegacyPortDeviceInformation> const& ports)
                {
                    auto const count = ports == nullptr ? 0u : ports.Size();

                    while (target.Size() > count)
                    {
                        target.RemoveAtEnd();
                    }

                    for (uint32_t i = 0; i < count; i++)
                    {
                        auto const port = ports.GetAt(i);

                        if (i >= target.Size())
                        {
                            target.Append(winrt::make<implementation::Midi1PortItem>());
                        }

                        target.GetAt(i).as<implementation::Midi1PortItem>()->Update(
                            port.PortDeviceId(), port.Name(), DescribeGroupAndNumber(port));
                    }
                };

            collections::IVectorView<midi2legacy::MidiLegacyPortDeviceInformation> sources{ nullptr };
            collections::IVectorView<midi2legacy::MidiLegacyPortDeviceInformation> destinations{ nullptr };

            if (m_portWatcher != nullptr)
            {
                sources = m_portWatcher.GetEnumeratedPortsForAssociatedEndpoint(
                    m_detailEndpointId, midi2enum::Midi1PortFlow::MidiMessageSource);

                destinations = m_portWatcher.GetEnumeratedPortsForAssociatedEndpoint(
                    m_detailEndpointId, midi2enum::Midi1PortFlow::MidiMessageDestination);
            }

            fill(m_sourcePortItems, sources);
            fill(m_destinationPortItems, destinations);

            DetailNoSourcePortsText().Visibility(m_sourcePortItems.Size() == 0 ?
                xaml::Visibility::Visible : xaml::Visibility::Collapsed);

            DetailNoDestinationPortsText().Visibility(m_destinationPortItems.Size() == 0 ?
                xaml::Visibility::Visible : xaml::Visibility::Collapsed);
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to list the MIDI 1.0 ports.")
    }

    _Use_decl_annotations_
    void MainWindow::OnDetailCopyIdClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            winrt::Windows::ApplicationModel::DataTransfer::DataPackage package{};

            package.SetText(DetailEndpointIdText().Text());

            winrt::Windows::ApplicationModel::DataTransfer::Clipboard::SetContent(package);

            DetailStatusText().Text(res::GetString(L"EndpointIdCopied"));
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to copy the endpoint id.")
    }

    _Use_decl_annotations_
    void MainWindow::OnDetailMonitorClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            DetailStatusText().Text(native::LaunchMonitorForEndpoint(m_detailEndpointId) ?
                winrt::hstring{} : res::GetString(L"MonitorLaunchFailed"));
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to start the monitor.")
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnDetailPanicClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        auto lifetime = get_strong();

        try
        {
            SendPanicAsync(m_detailEndpointId);
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to send MIDI panic.")

        co_return;
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnDetailCustomizeClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        auto lifetime = get_strong();

        try
        {
            m_customizeRequested = true;

            EndpointDetailDialog().Hide();
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to open the customization dialog.")

        co_return;
    }

    // ------------------------------------------------------------------------------------
    // Customization
    // ------------------------------------------------------------------------------------

    _Use_decl_annotations_
    foundation::IAsyncAction MainWindow::ShowCustomizeDialogAsync(winrt::hstring endpointDeviceId)
    {
        auto lifetime = get_strong();

        try
        {
            auto endpoint = FindEndpoint(endpointDeviceId);

            if (endpoint == nullptr)
            {
                co_return;
            }

            winrt::hstring name{};
            winrt::hstring description{};

            m_customizeImageFileName = {};

            if (auto const userInfo = endpoint.GetUserSuppliedInfo())
            {
                name = userInfo.Name();
                description = userInfo.Description();
                m_customizeImageFileName = userInfo.ImageFileName();
            }

            // An endpoint with no custom name shows the name it currently has, so the customer
            // edits what they see rather than starting from an empty box.
            CustomizeNameTextBox().Text(name.empty() ? endpoint.Name() : name);
            CustomizeDescriptionTextBox().Text(description);
            CustomizeStatusText().Text({});

            // Coming back from the port names dialog restores what was typed, rather than the
            // stored values, so stepping out to rename ports does not discard an edit in progress.
            if (m_customizeEditsPending)
            {
                m_customizeEditsPending = false;

                CustomizeNameTextBox().Text(m_pendingCustomizeName);
                CustomizeDescriptionTextBox().Text(m_pendingCustomizeDescription);
            }

            UpdateCustomizeImagePreview();

            // KS and KSAggregate are the transports which create a MIDI 1.0 port per group, so
            // they are the only ones with per-port names to edit.
            winrt::hstring transportCode{};
            winrt::guid transportId{};

            if (auto const transportInfo = endpoint.GetTransportSuppliedInfo())
            {
                transportCode = transportInfo.TransportCode();
                transportId = transportInfo.TransportId();
            }

            CustomizePortNamingPanel().Visibility(
                (transportCode == L"KS" || transportCode == L"KSA") && endpoint.IsMidi1PortCreationEnabled() ?
                    xaml::Visibility::Visible : xaml::Visibility::Collapsed);

            m_portNamesRequested = false;

            CustomizeDialog().XamlRoot(Content().XamlRoot());

            auto const result = co_await CustomizeDialog().ShowAsync();

            // Stepping out to the port names dialog and back is not a cancel, so nothing is
            // applied here and the customize dialog is reopened when it closes.
            if (m_portNamesRequested && !m_closing)
            {
                m_portNamesRequested = false;

                co_await ShowMidi1PortNamesDialogAsync(endpointDeviceId);

                if (!m_closing)
                {
                    co_await ShowCustomizeDialogAsync(endpointDeviceId);
                }

                co_return;
            }

            if (result != controls::ContentDialogResult::Primary || m_closing)
            {
                co_return;
            }

            auto const newName = TrimmedOrEmpty(CustomizeNameTextBox().Text());
            auto const newDescription = TrimmedOrEmpty(CustomizeDescriptionTextBox().Text());
            auto const newImage = m_customizeImageFileName;

            auto const deviceInstanceId = endpoint.DeviceInstanceId();

            bool sent{ false };
            bool saved{ false };
            winrt::hstring errorMessage{};

            co_await native::RunOnBackgroundAsync(
                [&sent, &saved, &errorMessage, transportId, endpointDeviceId, deviceInstanceId,
                 newName, newDescription, newImage]()
                {
                    midi2config::MidiServiceEndpointCustomizationConfig config{ transportId };

                    config.Name(newName);
                    config.Description(newDescription);
                    config.ImageFileName(newImage);

                    // This dialog owns the whole set, so an empty box means "remove the stored
                    // value" rather than "leave whatever was there".
                    config.ClearDisplayProperties(true);

                    config.MatchCriteria().EndpointDeviceId(endpointDeviceId);
                    config.MatchCriteria().DeviceInstanceId(deviceInstanceId);

                    auto const response = midi2config::MidiServiceTransportPluginConfigManager::SendUpdate(config);

                    sent = response != nullptr &&
                        response.Status() == midi2config::MidiServiceConfigResponseStatus::Success;

                    if (!sent)
                    {
                        errorMessage = response == nullptr ? winrt::hstring{} : response.ServiceErrorMessage();
                        return;
                    }

                    // Applying it and remembering it are separate steps. Without the save the
                    // change is gone the next time the service starts.
                    auto const saveResponse = midi2config::MidiServiceTransportPluginConfigManager::SaveUpdate(config);

                    saved = saveResponse != nullptr && saveResponse.Success();

                    if (!saved && saveResponse != nullptr)
                    {
                        errorMessage = saveResponse.ErrorMessage();
                    }
                });

            if (m_closing)
            {
                co_return;
            }

            if (!sent)
            {
                StatusText().Text(errorMessage.empty() ?
                    res::GetString(L"CustomizeFailed") :
                    res::FormatString(L"CustomizeFailedFormat", errorMessage));
            }
            else if (!saved)
            {
                StatusText().Text(errorMessage.empty() ?
                    res::GetString(L"CustomizeNotSaved") :
                    res::FormatString(L"CustomizeNotSavedFormat", errorMessage));
            }
            else
            {
                StatusText().Text(res::GetString(L"CustomizeSaved"));
            }

            RefreshEndpointList();
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to customize the endpoint.")
    }

    void MainWindow::UpdateCustomizeImagePreview() noexcept
    {
        try
        {
            auto const path = midiapp::ResolveEndpointImagePath(m_customizeImageFileName);

            if (path.empty())
            {
                CustomizeImagePreview().Source(nullptr);
                return;
            }

            if (midiapp::EndpointImageAssets::IsScalableVector(std::wstring{ path }))
            {
                media::Imaging::SvgImageSource source{};

                source.RasterizePixelHeight(112);
                source.UriSource(foundation::Uri{ L"file:///" + path });

                CustomizeImagePreview().Source(source);
            }
            else
            {
                media::Imaging::BitmapImage bitmap{};

                bitmap.DecodePixelHeight(112);
                bitmap.UriSource(foundation::Uri{ L"file:///" + path });

                CustomizeImagePreview().Source(bitmap);
            }
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to show the picture.")
    }

    _Use_decl_annotations_
    void MainWindow::OnCustomizeBrowseImageClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            // The Win32 dialog rather than the WinRT picker: the picker's completion never
            // resumes when it is raised over an open ContentDialog, which is exactly here.
            auto const chosen = midiapp::EndpointImageAssets::ShowPicker(WindowHandle());

            if (chosen.empty())
            {
                return;
            }

            auto const stored = midiapp::ImportEndpointImage(winrt::hstring{ chosen });

            if (stored.empty())
            {
                CustomizeStatusText().Text(res::GetString(L"ImageCopyFailed"));
                return;
            }

            m_customizeImageFileName = stored;

            CustomizeStatusText().Text({});

            UpdateCustomizeImagePreview();
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to choose a picture.")
    }

    _Use_decl_annotations_
    void MainWindow::OnCustomizeClearImageClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            m_customizeImageFileName = {};

            CustomizeStatusText().Text({});

            UpdateCustomizeImagePreview();
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to clear the picture.")
    }
}
