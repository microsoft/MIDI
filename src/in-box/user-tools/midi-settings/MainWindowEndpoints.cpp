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
        constexpr uint32_t HealthCheckIntervalSeconds = 5;

        winrt::hstring DescribeNativeDataFormat(midi2enum::MidiEndpointNativeDataFormat const format) noexcept
        {
            switch (format)
            {
            case midi2enum::MidiEndpointNativeDataFormat::UniversalMidiPacketFormat:
                return res::GetString(L"NativeFormatUmp");

            case midi2enum::MidiEndpointNativeDataFormat::Midi1ByteFormat:
                return res::GetString(L"NativeFormatBytes");

            default:
                return res::GetString(L"NativeFormatUnknown");
            }
        }
    }

    // ------------------------------------------------------------------------------------
    // Watchers
    // ------------------------------------------------------------------------------------

    winrt::fire_and_forget MainWindow::StartWatchersAsync() noexcept
    {
        auto lifetime = get_strong();

        try
        {
            if (m_closing || m_endpointWatcher != nullptr)
            {
                co_return;
            }

            bool available{ false };

            std::vector<midi2rept::MidiServiceTransportPluginInfo> transports{};

            // Every one of these blocks on the service over RPC, so none of it may happen on
            // the XAML thread. Awaiting the helper puts us back on it afterwards.
            co_await native::RunOnBackgroundAsync([&available, &transports]()
                {
                    available = midi2::MidiApi::EnsureServiceAvailable();

                    if (!available)
                    {
                        return;
                    }

                    if (auto const installed = midi2rept::MidiReporting::GetInstalledTransportPlugins())
                    {
                        for (auto const& transport : installed)
                        {
                            transports.push_back(transport);
                        }
                    }
                });

            if (m_closing)
            {
                co_return;
            }

            m_serviceAvailable = available;

            ServiceBar().IsOpen(!available);

            if (!available)
            {
                m_endpointItems.Clear();

                ApplyViewMode();

                EndpointCountText().Text({});

                co_return;
            }

            m_transportNames.clear();

            for (auto const& transport : transports)
            {
                if (!transport.TransportCode().empty())
                {
                    m_transportNames.insert_or_assign(
                        std::wstring{ transport.TransportCode() },
                        std::wstring{ transport.Name() });
                }
            }

            m_endpointWatcher = midi2enum::MidiEndpointDeviceWatcher::Create(
                midi2enum::MidiEndpointDeviceInformationFilters::AllStandardEndpoints);

            if (m_endpointWatcher != nullptr)
            {
                auto const refresh = [weak = get_weak(), queue = m_dispatcherQueue]()
                    {
                        if (queue == nullptr)
                        {
                            return;
                        }

                        queue.TryEnqueue([weak]()
                            {
                                if (auto strong = weak.get())
                                {
                                    strong->RefreshEndpointList();
                                }
                            });
                    };

                m_endpointAddedToken = m_endpointWatcher.Added([refresh](auto&&, auto&&) { refresh(); });
                m_endpointRemovedToken = m_endpointWatcher.Removed([refresh](auto&&, auto&&) { refresh(); });
                m_endpointUpdatedToken = m_endpointWatcher.Updated([refresh](auto&&, auto&&) { refresh(); });

                m_endpointWatcher.Start();
            }

            // The MIDI 1.0 ports shown in the detail dialog get their own watcher so the list
            // stays right while the dialog is open, rather than being a snapshot taken once.
            m_portWatcher = midi2legacy::MidiLegacyPortDeviceWatcher::Create();

            if (m_portWatcher != nullptr)
            {
                auto const refreshPorts = [weak = get_weak(), queue = m_dispatcherQueue]()
                    {
                        if (queue == nullptr)
                        {
                            return;
                        }

                        queue.TryEnqueue([weak]()
                            {
                                if (auto strong = weak.get())
                                {
                                    strong->RefreshDetailPorts();
                                }
                            });
                    };

                m_portAddedToken = m_portWatcher.Added([refreshPorts](auto&&, auto&&) { refreshPorts(); });
                m_portRemovedToken = m_portWatcher.Removed([refreshPorts](auto&&, auto&&) { refreshPorts(); });
                m_portUpdatedToken = m_portWatcher.Updated([refreshPorts](auto&&, auto&&) { refreshPorts(); });

                m_portWatcher.Start();
            }

            RefreshTransportChoices();
            RefreshEndpointList();
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to start the device watchers.")
    }

    void MainWindow::StopWatchers() noexcept
    {
        try
        {
            if (m_endpointWatcher != nullptr)
            {
                m_endpointWatcher.Added(m_endpointAddedToken);
                m_endpointWatcher.Removed(m_endpointRemovedToken);
                m_endpointWatcher.Updated(m_endpointUpdatedToken);

                m_endpointWatcher.Stop();
                m_endpointWatcher = nullptr;
            }

            if (m_portWatcher != nullptr)
            {
                m_portWatcher.Added(m_portAddedToken);
                m_portWatcher.Removed(m_portRemovedToken);
                m_portWatcher.Updated(m_portUpdatedToken);

                m_portWatcher.Stop();
                m_portWatcher = nullptr;
            }
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to stop the device watchers.")
    }

    // ------------------------------------------------------------------------------------
    // Service health
    // ------------------------------------------------------------------------------------

    void MainWindow::StartHealthTimer() noexcept
    {
        try
        {
            if (m_dispatcherQueue == nullptr || m_healthTimer != nullptr)
            {
                return;
            }

            m_healthTimer = m_dispatcherQueue.CreateTimer();

            m_healthTimer.Interval(std::chrono::seconds{ HealthCheckIntervalSeconds });

            m_healthTimer.Tick([weak = get_weak()](auto&&, auto&&)
                {
                    if (auto strong = weak.get())
                    {
                        strong->CheckServiceHealthAsync();
                    }
                });

            m_healthTimer.Start();
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to start the service health timer.")
    }

    void MainWindow::StopHealthTimer() noexcept
    {
        try
        {
            if (m_healthTimer != nullptr)
            {
                m_healthTimer.Stop();
                m_healthTimer = nullptr;
            }
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to stop the service health timer.")
    }

    // A watcher created against a service that has since restarted keeps running but never
    // reports anything again, so polling is what lets the app recover on its own rather than
    // needing to be closed and reopened.
    winrt::fire_and_forget MainWindow::CheckServiceHealthAsync() noexcept
    {
        auto lifetime = get_strong();

        try
        {
            if (m_closing || m_healthCheckInFlight)
            {
                co_return;
            }

            m_healthCheckInFlight = true;

            bool available{ false };

            co_await native::RunOnBackgroundAsync([&available]()
                {
                    available = midi2::MidiApi::EnsureServiceAvailable();
                });

            m_healthCheckInFlight = false;

            if (m_closing)
            {
                co_return;
            }

            if (available == m_serviceAvailable)
            {
                // The service is in the same state, but the watcher can die on its own: a
                // service restart quick enough to fall between two ticks leaves it alive but
                // permanently silent, and the first attempt to create it may have failed.
                if (available && m_endpointWatcher == nullptr)
                {
                    StartWatchersAsync();
                }
                else if (available)
                {
                    auto const status = m_endpointWatcher.Status();

                    if (status == winrt::Windows::Devices::Enumeration::DeviceWatcherStatus::Stopped ||
                        status == winrt::Windows::Devices::Enumeration::DeviceWatcherStatus::Aborted)
                    {
                        StopWatchers();
                        StartWatchersAsync();
                    }
                }

                co_return;
            }

            m_serviceAvailable = available;

            // Both transitions are handled the same way: throw the old watchers away and, when
            // the service is back, build new ones against the new instance.
            StopWatchers();

            ServiceBar().IsOpen(!available);

            if (!available)
            {
                // The cached session belongs to the service instance that just went away.
                ::midisettings::ShutDownPanicSession();

                m_endpointItems.Clear();

                ApplyViewMode();

                EndpointCountText().Text({});

                co_return;
            }

            StartWatchersAsync();
        }
        catch (...)
        {
            m_healthCheckInFlight = false;

            MIDI_SETTINGS_LOG_GENERAL_EXCEPTION(L"Unable to check the MIDI service.");
        }
    }

    // ------------------------------------------------------------------------------------
    // Endpoint list
    // ------------------------------------------------------------------------------------

    _Use_decl_annotations_
    winrt::hstring MainWindow::TransportDisplayName(winrt::hstring const& transportCode) noexcept
    {
        try
        {
            auto const found = m_transportNames.find(std::wstring{ transportCode });

            if (found != m_transportNames.end() && !found->second.empty())
            {
                return winrt::hstring{ found->second };
            }
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to resolve a transport name.")

        return transportCode;
    }

    void MainWindow::RefreshTransportChoices() noexcept
    {
        try
        {
            m_suppressFilterHandling = true;

            auto const previous = native::AppSettings::Current().TransportFilter();

            m_transportChoices.Clear();

            m_transportChoices.Append(
                winrt::make<implementation::TransportChoice>(res::GetString(L"TransportFilterAll"), winrt::hstring{}));

            int32_t selectedIndex{ 0 };
            int32_t index{ 1 };

            for (auto const& [code, name] : m_transportNames)
            {
                m_transportChoices.Append(winrt::make<implementation::TransportChoice>(
                    winrt::hstring{ name }, winrt::hstring{ code }));

                if (::_wcsicmp(code.c_str(), previous.c_str()) == 0)
                {
                    selectedIndex = index;
                }

                index++;
            }

            TransportFilterComboBox().SelectedIndex(selectedIndex);

            m_suppressFilterHandling = false;
        }
        catch (...)
        {
            m_suppressFilterHandling = false;

            MIDI_SETTINGS_LOG_GENERAL_EXCEPTION(L"Unable to build the transport filter.");
        }
    }

    _Use_decl_annotations_
    void MainWindow::OnTransportFilterChanged(
        foundation::IInspectable const&,
        controls::SelectionChangedEventArgs const&)
    {
        try
        {
            if (m_suppressFilterHandling || !m_loaded)
            {
                return;
            }

            auto const selected = TransportFilterComboBox().SelectedItem()
                .try_as<midisettings::TransportChoice>();

            native::AppSettings::Current().TransportFilter(
                selected == nullptr ? std::wstring{} : std::wstring{ selected.TransportCode() });

            RefreshEndpointList();
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to apply the transport filter.")
    }

    _Use_decl_annotations_
    winrt::hstring MainWindow::ResolveEndpointImage(midi2enum::MidiEndpointDeviceInformation const& endpoint) noexcept
    {
        try
        {
            if (auto const userInfo = endpoint.GetUserSuppliedInfo())
            {
                auto const custom = midiapp::ResolveEndpointImagePath(userInfo.ImageFileName());

                if (!custom.empty())
                {
                    return custom;
                }
            }

            // The installer puts one default per transport in the same folder, named after the
            // transport code, with a plain default alongside them for anything unrecognized.
            winrt::hstring transportCode{};

            if (auto const transportInfo = endpoint.GetTransportSuppliedInfo())
            {
                transportCode = transportInfo.TransportCode();
            }

            if (!transportCode.empty())
            {
                std::wstring lowered{ transportCode };

                std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                    [](wchar_t const ch) { return static_cast<wchar_t>(::towlower(ch)); });

                auto const byTransport = midiapp::ResolveEndpointImagePath(
                    winrt::hstring{ L"default-" + lowered + L"-small.svg" });

                if (!byTransport.empty())
                {
                    return byTransport;
                }
            }

            return midiapp::ResolveEndpointImagePath(L"default-small.svg");
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to resolve an endpoint picture.")

        return {};
    }

    _Use_decl_annotations_
    winrt::hstring MainWindow::BuildEndpointDetail(midi2enum::MidiEndpointDeviceInformation const& endpoint) noexcept
    {
        try
        {
            auto const transportInfo = endpoint.GetTransportSuppliedInfo();

            if (transportInfo == nullptr)
            {
                return {};
            }

            std::wstring detail{ TransportDisplayName(transportInfo.TransportCode()) };

            detail += L" \x2022 ";
            detail += DescribeNativeDataFormat(transportInfo.NativeDataFormat());

            if (transportInfo.SupportsMultiClient())
            {
                detail += L" \x2022 ";
                detail += res::GetString(L"MultiClient");
            }

            return winrt::hstring{ detail };
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to describe an endpoint.")

        return {};
    }

    void MainWindow::RefreshEndpointList() noexcept
    {
        try
        {
            if (m_closing || m_endpointWatcher == nullptr)
            {
                return;
            }

            auto const filter = native::AppSettings::Current().TransportFilter();
            auto const canMonitor = native::IsMonitoringAvailable();

            auto const devices = midiapp::SortedEndpoints(m_endpointWatcher);

            std::vector<midi2enum::MidiEndpointDeviceInformation> visible{};

            for (auto const& device : devices)
            {
                if (!filter.empty())
                {
                    auto const transportInfo = device.GetTransportSuppliedInfo();

                    if (transportInfo == nullptr ||
                        ::_wcsicmp(std::wstring{ transportInfo.TransportCode() }.c_str(), filter.c_str()) != 0)
                    {
                        continue;
                    }
                }

                visible.push_back(device);
            }

            // Rows are updated in place. Rebuilding the collection would reset the scroll
            // position and flash every picture each time a device announced itself, and the
            // watcher does that repeatedly during discovery.
            while (m_endpointItems.Size() > visible.size())
            {
                m_endpointItems.RemoveAtEnd();
            }

            for (uint32_t i = 0; i < visible.size(); i++)
            {
                auto const& device = visible[i];

                winrt::hstring description{};

                if (auto const userInfo = device.GetUserSuppliedInfo())
                {
                    description = userInfo.Description();
                }

                winrt::hstring transportCode{};

                if (auto const transportInfo = device.GetTransportSuppliedInfo())
                {
                    transportCode = transportInfo.TransportCode();
                }

                if (i >= m_endpointItems.Size())
                {
                    m_endpointItems.Append(winrt::make<implementation::EndpointItem>());
                }

                auto item = m_endpointItems.GetAt(i).as<implementation::EndpointItem>();

                item->Update(
                    device.EndpointDeviceId(),
                    device.Name(),
                    description,
                    transportCode,
                    BuildEndpointDetail(device),
                    ResolveEndpointImage(device),
                    canMonitor);
            }

            EndpointCountText().Text(res::FormatString(
                L"EndpointCountFormat", static_cast<int32_t>(visible.size())));

            ApplyViewMode();
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to refresh the endpoint list.")
    }

    _Use_decl_annotations_
    midi2enum::MidiEndpointDeviceInformation MainWindow::FindEndpoint(winrt::hstring const& endpointDeviceId) noexcept
    {
        try
        {
            if (m_endpointWatcher == nullptr || endpointDeviceId.empty())
            {
                return nullptr;
            }

            for (auto const& device : midiapp::SortedEndpoints(m_endpointWatcher))
            {
                if (midiapp::EndpointIdsMatch(device.EndpointDeviceId(), endpointDeviceId))
                {
                    return device;
                }
            }
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to find the endpoint.")

        return nullptr;
    }
}
