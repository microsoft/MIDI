// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MainWindow.xaml.h"

#include "BackgroundWork.h"
#include "MidiConfig.h"
#include "SettingsItems.h"
#include "StringResources.h"

namespace native = ::midisettings;
namespace res = ::midisettings::resources;

namespace winrt::midisettings::implementation
{
    namespace
    {
        // What the port is called right now, which is what the customer recognizes in their DAW.
        // Mirrors MidiEndpointNameTable::GetPreferredName on the service side: a name of the
        // customer's own always wins, and otherwise the selected style decides.
        winrt::hstring ResolveCurrentName(
            _In_ midi2enum::Midi1PortNameTableEntry const& entry,
            _In_ midi2enum::Midi1PortNamingApproach const effectiveApproach) noexcept
        {
            try
            {
                if (!entry.CustomName().empty())
                {
                    return entry.CustomName();
                }

                if (effectiveApproach == midi2enum::Midi1PortNamingApproach::UseNewStyle)
                {
                    return entry.NewStyleName();
                }

                return entry.LegacyCompatibleName();
            }
            catch (...)
            {
                return {};
            }
        }

        midi2enum::Midi1PortNamingApproach ApproachFromComboIndex(_In_ int32_t const index) noexcept
        {
            switch (index)
            {
            case 1:  return midi2enum::Midi1PortNamingApproach::UseClassicCompatible;
            case 2:  return midi2enum::Midi1PortNamingApproach::UseNewStyle;
            default: return midi2enum::Midi1PortNamingApproach::Default;
            }
        }

        int32_t ComboIndexFromApproach(_In_ midi2enum::Midi1PortNamingApproach const approach) noexcept
        {
            switch (approach)
            {
            case midi2enum::Midi1PortNamingApproach::UseClassicCompatible: return 1;
            case midi2enum::Midi1PortNamingApproach::UseNewStyle:          return 2;
            default:                                                      return 0;
            }
        }
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnCustomizePortNamesClick(
        foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        auto lifetime = get_strong();

        try
        {
            // Only one content dialog can be open at a time, so customize steps aside and the
            // caller brings it back when the port names dialog closes.
            m_portNamesRequested = true;

            m_customizeEditsPending = true;
            m_pendingCustomizeName = CustomizeNameTextBox().Text();
            m_pendingCustomizeDescription = CustomizeDescriptionTextBox().Text();

            CustomizeDialog().Hide();
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to open the port names dialog.")

        co_return;
    }

    _Use_decl_annotations_
    void MainWindow::OnMidi1PortNamesApproachChanged(
        foundation::IInspectable const&, xaml::Controls::SelectionChangedEventArgs const&)
    {
        UpdateMidi1PortNamesApproachCaption();
    }

    // The "current name" column and the caption both depend on the selected style, so they are
    // recalculated as the customer changes it rather than only when the dialog opens.
    void MainWindow::UpdateMidi1PortNamesApproachCaption() noexcept
    {
        try
        {
            auto const selected = ApproachFromComboIndex(Midi1PortNamesApproachCombo().SelectedIndex());

            auto effective = selected;

            if (effective == midi2enum::Midi1PortNamingApproach::Default)
            {
                auto const globalDefault = native::config::DefaultMidi1PortNaming();

                effective = globalDefault == native::Midi1PortNaming::NewStyle ?
                    midi2enum::Midi1PortNamingApproach::UseNewStyle :
                    midi2enum::Midi1PortNamingApproach::UseClassicCompatible;

                Midi1PortNamesApproachCaption().Text(
                    effective == midi2enum::Midi1PortNamingApproach::UseNewStyle ?
                        res::GetString(L"Midi1PortNamesDefaultIsNewStyle") :
                        res::GetString(L"Midi1PortNamesDefaultIsClassic"));
            }
            else
            {
                Midi1PortNamesApproachCaption().Text({});
            }

            auto const refresh = [effective](collections::IObservableVector<midisettings::Midi1PortNameItem> const& rows,
                std::vector<midi2enum::Midi1PortNameTableEntry> const& entries)
                {
                    for (uint32_t i = 0; i < rows.Size() && i < entries.size(); i++)
                    {
                        auto const item = rows.GetAt(i).as<implementation::Midi1PortNameItem>();

                        // keep whatever the customer has typed; only the resolved name moves
                        item->Update(
                            item->GroupIndex(),
                            ResolveCurrentName(entries[i], effective),
                            entries[i].LegacyCompatibleName(),
                            entries[i].NewStyleName(),
                            item->CustomName());
                    }
                };

            refresh(m_midi1PortNameSources, m_midi1PortNameSourceEntries);
            refresh(m_midi1PortNameDestinations, m_midi1PortNameDestinationEntries);
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to update the port naming caption.")
    }

    _Use_decl_annotations_
    void MainWindow::PopulateMidi1PortNames(midi2enum::MidiEndpointDeviceInformation const& endpoint) noexcept
    {
        try
        {
            m_midi1PortNameSourceEntries.clear();
            m_midi1PortNameDestinationEntries.clear();

            m_midi1PortNameSources.Clear();
            m_midi1PortNameDestinations.Clear();

            auto const table = endpoint.GetNameTable();

            if (table != nullptr)
            {
                for (auto const& entry : table)
                {
                    if (entry.Flow() == midi2enum::Midi1PortFlow::MidiMessageSource)
                    {
                        m_midi1PortNameSourceEntries.push_back(entry);
                    }
                    else
                    {
                        m_midi1PortNameDestinationEntries.push_back(entry);
                    }
                }
            }

            // The service hands these back in whatever order the blocks were walked. A customer
            // reading a 16-port device down the page expects them in group order.
            auto const byGroup = [](midi2enum::Midi1PortNameTableEntry const& a,
                midi2enum::Midi1PortNameTableEntry const& b)
                {
                    return a.Group().Index() < b.Group().Index();
                };

            std::sort(m_midi1PortNameSourceEntries.begin(), m_midi1PortNameSourceEntries.end(), byGroup);
            std::sort(m_midi1PortNameDestinationEntries.begin(), m_midi1PortNameDestinationEntries.end(), byGroup);

            auto const fill = [](collections::IObservableVector<midisettings::Midi1PortNameItem> const& target,
                std::vector<midi2enum::Midi1PortNameTableEntry> const& entries)
                {
                    for (auto const& entry : entries)
                    {
                        auto item = winrt::make<implementation::Midi1PortNameItem>();

                        item.as<implementation::Midi1PortNameItem>()->Update(
                            entry.Group().Index(),
                            entry.LegacyCompatibleName(),
                            entry.LegacyCompatibleName(),
                            entry.NewStyleName(),
                            entry.CustomName());

                        target.Append(item);
                    }
                };

            fill(m_midi1PortNameSources, m_midi1PortNameSourceEntries);
            fill(m_midi1PortNameDestinations, m_midi1PortNameDestinationEntries);

            Midi1PortNamesNoSourcesText().Visibility(
                m_midi1PortNameSources.Size() == 0 ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);

            Midi1PortNamesNoDestinationsText().Visibility(
                m_midi1PortNameDestinations.Size() == 0 ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to read the MIDI 1.0 port names.")
    }

    _Use_decl_annotations_
    foundation::IAsyncAction MainWindow::ShowMidi1PortNamesDialogAsync(winrt::hstring endpointDeviceId)
    {
        auto lifetime = get_strong();

        try
        {
            auto endpoint = FindEndpoint(endpointDeviceId);

            if (endpoint == nullptr)
            {
                co_return;
            }

            m_portNamesEndpointDeviceId = endpointDeviceId;

            Midi1PortNamesDialog().Title(
                winrt::box_value(res::FormatString(L"Midi1PortNamesTitleFormat", endpoint.Name())));

            Midi1PortNamesSourcesItemsControl().ItemsSource(m_midi1PortNameSources);
            Midi1PortNamesDestinationsItemsControl().ItemsSource(m_midi1PortNameDestinations);

            PopulateMidi1PortNames(endpoint);

            Midi1PortNamesApproachCombo().SelectedIndex(
                ComboIndexFromApproach(endpoint.Midi1PortNamingApproach()));

            // SelectedIndex above may not have changed, so resolve the columns either way
            UpdateMidi1PortNamesApproachCaption();

            Midi1PortNamesStatusText().Text({});

            Midi1PortNamesDialog().XamlRoot(Content().XamlRoot());

            auto const result = co_await Midi1PortNamesDialog().ShowAsync();

            if (result != controls::ContentDialogResult::Primary || m_closing)
            {
                co_return;
            }

            auto const approach = ApproachFromComboIndex(Midi1PortNamesApproachCombo().SelectedIndex());

            struct PortRename
            {
                uint8_t GroupIndex{ 0 };
                winrt::hstring Name{};
            };

            std::vector<PortRename> sources{};
            std::vector<PortRename> destinations{};

            auto const collect = [](collections::IObservableVector<midisettings::Midi1PortNameItem> const& rows,
                std::vector<PortRename>& target)
                {
                    for (uint32_t i = 0; i < rows.Size(); i++)
                    {
                        auto const item = rows.GetAt(i).as<implementation::Midi1PortNameItem>();

                        // An empty box is sent as an empty name, which is how a previously
                        // customized port gets its generated name back.
                        target.push_back({ item->GroupIndex(), item->CustomName() });
                    }
                };

            collect(m_midi1PortNameSources, sources);
            collect(m_midi1PortNameDestinations, destinations);

            winrt::guid transportId{};

            if (auto const transportInfo = endpoint.GetTransportSuppliedInfo())
            {
                transportId = transportInfo.TransportId();
            }

            auto const deviceInstanceId = endpoint.DeviceInstanceId();

            bool sent{ false };
            bool saved{ false };
            winrt::hstring errorMessage{};

            co_await native::RunOnBackgroundAsync(
                [&sent, &saved, &errorMessage, transportId, endpointDeviceId, deviceInstanceId,
                 approach, sources, destinations]()
                {
                    midi2config::MidiServiceEndpointCustomizationConfig config{ transportId };

                    config.MatchCriteria().EndpointDeviceId(endpointDeviceId);
                    config.MatchCriteria().DeviceInstanceId(deviceInstanceId);

                    config.Midi1PortNamingApproach(approach);

                    for (auto const& source : sources)
                    {
                        config.AddMidi1SourcePortCustomName(midi2::MidiGroup{ source.GroupIndex }, source.Name);
                    }

                    for (auto const& destination : destinations)
                    {
                        config.AddMidi1DestinationPortCustomName(midi2::MidiGroup{ destination.GroupIndex }, destination.Name);
                    }

                    auto const response = midi2config::MidiServiceTransportPluginConfigManager::SendUpdate(config);

                    sent = response != nullptr &&
                        response.Status() == midi2config::MidiServiceConfigResponseStatus::Success;

                    if (!sent)
                    {
                        errorMessage = response == nullptr ? winrt::hstring{} : response.ServiceErrorMessage();
                        return;
                    }

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
                    res::GetString(L"Midi1PortNamesFailed") :
                    res::FormatString(L"Midi1PortNamesFailedFormat", errorMessage));
            }
            else if (!saved)
            {
                StatusText().Text(errorMessage.empty() ?
                    res::GetString(L"Midi1PortNamesNotSaved") :
                    res::FormatString(L"Midi1PortNamesNotSavedFormat", errorMessage));
            }
            else
            {
                StatusText().Text(res::GetString(L"Midi1PortNamesSaved"));
            }
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to show the MIDI 1.0 port names dialog.")
    }
}
