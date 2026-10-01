// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "EndpointCatalog.h"
#include "MidiEndpointHelpers.h"

#include <cwctype>
#include <map>

namespace midiapp
{
    namespace mdm2 = winrt::Windows::Devices::Midi2;
    namespace mdm2enum = winrt::Windows::Devices::Midi2::Enumeration;
    namespace mdm2legacy = winrt::Windows::Devices::Midi2::Enumeration::Legacy;
    namespace mdm2loop = winrt::Windows::Devices::Midi2::Transports::Loopback;

    namespace
    {
        constexpr wchar_t TransportCodeLoopback[] = L"LOOP";
        constexpr wchar_t TransportCodeBasicLoopback[] = L"BLOOP";

        bool EqualsIgnoringCase(_In_ std::wstring const& left, _In_ std::wstring const& right) noexcept
        {
            if (left.empty() || right.empty())
            {
                return false;
            }

            return ::CompareStringOrdinal(left.c_str(), -1, right.c_str(), -1, TRUE) == CSTR_EQUAL;
        }

        std::wstring SafeString(_In_ winrt::hstring const& value) noexcept
        {
            return SanitizeStoredString(std::wstring{ value });
        }

        // The way the SDK compares endpoint ids, so a port finds the endpoint it belongs to.
        std::wstring NormalizedEndpointId(_In_ winrt::hstring const& value)
        {
            std::wstring id{ value };

            std::transform(id.begin(), id.end(), id.begin(),
                [](wchar_t ch) { return static_cast<wchar_t>(::towlower(ch)); });

            return id;
        }

        using GroupPortNames = std::array<std::wstring, MaximumGroupCount>;

        // A port's name goes in the slot of the group it carries.
        void ClaimPortName(
            _In_ mdm2legacy::MidiLegacyPortDeviceInformation const& port,
            _Inout_ GroupPortNames& names)
        {
            if (port == nullptr || port.Group() == nullptr)
            {
                return;
            }

            auto const index = static_cast<int32_t>(port.Group().Index());

            if (index >= 0 && index < MaximumGroupCount)
            {
                names[static_cast<size_t>(index)] = SafeString(port.Name());
            }
        }

        using PortNames = std::map<std::wstring, GroupPortNames>;

        // Every MIDI 1.0 port of one flow, by the endpoint it belongs to. Asked for once rather
        // than once per endpoint, which on a PC with many devices took seconds.
        PortNames CollectPortNames(_In_ mdm2enum::Midi1PortFlow flow)
        {
            PortNames names{};

            auto const ports = mdm2legacy::MidiLegacyPortDeviceInformation::FindAll(flow);

            if (ports == nullptr)
            {
                return names;
            }

            for (auto const& port : ports)
            {
                if (port == nullptr)
                {
                    continue;
                }

                if (auto const endpointId = NormalizedEndpointId(port.AssociatedEndpointDeviceId()); !endpointId.empty())
                {
                    ClaimPortName(port, names[endpointId]);
                }
            }

            return names;
        }

        // Groups that already have a name keep it, so whichever block was offered first wins.
        void ClaimGroupNames(
            _Inout_ std::array<std::wstring, MaximumGroupCount>& names,
            _In_ mdm2::MidiGroup const& firstGroup,
            _In_ uint8_t groupCount,
            _In_ std::wstring const& name) noexcept
        {
            if (firstGroup == nullptr || name.empty())
            {
                return;
            }

            auto const first = static_cast<size_t>(firstGroup.Index());
            auto const last = std::min(first + groupCount, names.size());

            for (auto i = first; i < last; i++)
            {
                if (names[i].empty())
                {
                    names[i] = name;
                }
            }
        }

        // Block directions are the device's point of view: a block output is a source for us.
        void ReadGroupNames(
            _In_ mdm2enum::MidiEndpointDeviceInformation const& device,
            _Inout_ LiveEndpoint& endpoint) noexcept
        {
            try
            {
                // Active function blocks, then inactive ones, then group terminal blocks for the rest.
                if (auto const functionBlocks = device.GetDeclaredFunctionBlocks())
                {
                    for (auto const active : { true, false })
                    {
                        for (auto const& block : functionBlocks)
                        {
                            if (block == nullptr || block.IsActive() != active)
                            {
                                continue;
                            }

                            auto const direction = block.Direction();
                            auto const firstGroup = block.FirstGroup();
                            auto const name = SafeString(block.Name());

                            if (direction == mdm2enum::MidiFunctionBlockDirection::BlockOutput ||
                                direction == mdm2enum::MidiFunctionBlockDirection::Bidirectional)
                            {
                                ClaimGroupNames(endpoint.SourceGroupNames, firstGroup, block.GroupCount(), name);
                            }

                            if (direction == mdm2enum::MidiFunctionBlockDirection::BlockInput ||
                                direction == mdm2enum::MidiFunctionBlockDirection::Bidirectional)
                            {
                                ClaimGroupNames(endpoint.DestinationGroupNames, firstGroup, block.GroupCount(), name);
                            }
                        }
                    }
                }

                if (auto const terminalBlocks = device.GetGroupTerminalBlocks())
                {
                    for (auto const& block : terminalBlocks)
                    {
                        if (block == nullptr)
                        {
                            continue;
                        }

                        auto const direction = block.Direction();
                        auto const firstGroup = block.FirstGroup();
                        auto const name = SafeString(block.Name());

                        if (direction == mdm2enum::MidiGroupTerminalBlockDirection::BlockOutput ||
                            direction == mdm2enum::MidiGroupTerminalBlockDirection::Bidirectional)
                        {
                            ClaimGroupNames(endpoint.SourceGroupNames, firstGroup, block.GroupCount(), name);
                        }

                        if (direction == mdm2enum::MidiGroupTerminalBlockDirection::BlockInput ||
                            direction == mdm2enum::MidiGroupTerminalBlockDirection::Bidirectional)
                        {
                            ClaimGroupNames(endpoint.DestinationGroupNames, firstGroup, block.GroupCount(), name);
                        }
                    }
                }
            }
            catch (...)
            {
                ReportEndpointError(L"Unable to read the group names of an endpoint.");
            }
        }

        using EndpointList = winrt::Windows::Foundation::Collections::IIterable<mdm2enum::MidiEndpointDeviceInformation>;

        // Everything about one endpoint except its MIDI 1.0 port names, which come from the port
        // watcher or from a direct query depending on how the list is being built.
        std::optional<LiveEndpoint> DescribeEndpoint(
            _In_ mdm2enum::MidiEndpointDeviceInformation const& device,
            _In_ EndpointList const& all)
        {
            if (device == nullptr)
            {
                return std::nullopt;
            }

            LiveEndpoint endpoint{};

            endpoint.EndpointDeviceId = SafeString(device.EndpointDeviceId());
            endpoint.Name = SafeString(device.Name());
            endpoint.DeviceInstanceId = SafeString(device.DeviceInstanceId());

            if (endpoint.EndpointDeviceId.empty())
            {
                return std::nullopt;
            }

            auto const transportInfo = device.GetTransportSuppliedInfo();

            endpoint.TransportCode = SafeString(transportInfo.TransportCode());
            endpoint.TransportSuppliedName = SafeString(transportInfo.Name());
            endpoint.ManufacturerName = SafeString(transportInfo.ManufacturerName());
            endpoint.UsbVendorId = transportInfo.VendorId();
            endpoint.UsbProductId = transportInfo.ProductId();
            endpoint.UsbSerialNumber = SafeString(transportInfo.SerialNumber());
            endpoint.Description = SafeString(transportInfo.Description());

            auto const parent = device.GetParentDeviceInformation();

            if (parent != nullptr)
            {
                endpoint.ParentDeviceName = SafeString(parent.Name());
            }

            if (auto const userInfo = device.GetUserSuppliedInfo())
            {
                endpoint.ImagePath = SafeString(ResolveEndpointImagePath(userInfo.ImageFileName()));

                // The customer's own words win, because they are the ones who will be
                // reading them back.
                if (auto const described = SafeString(userInfo.Description()); !described.empty())
                {
                    endpoint.Description = described;
                }
            }

            endpoint.DeclaredGroups = DeclaredGroups(device);

            auto const directions = DeclaredGroupDirections(device);

            endpoint.SourceGroups = directions.Sources;
            endpoint.DestinationGroups = directions.Destinations;

            ReadGroupNames(device, endpoint);

            if (EqualsIgnoringCase(endpoint.TransportCode, TransportCodeBasicLoopback))
            {
                endpoint.IsLoopback = true;
            }
            else if (EqualsIgnoringCase(endpoint.TransportCode, TransportCodeLoopback))
            {
                endpoint.IsLoopback = true;

                // Searched in the list already fetched. Without a list to search, the SDK
                // fetches every endpoint again, once for each loopback.
                auto const partner = mdm2loop::MidiLoopbackManager::GetAssociatedLoopbackEndpoint(device, all);

                if (partner != nullptr)
                {
                    endpoint.LoopbackPartnerEndpointId = SafeString(partner.EndpointDeviceId());
                }
            }

            return endpoint;
        }

        // From what the two watchers already hold, so nothing here searches the system again.
        std::vector<LiveEndpoint> BuildFromWatchers(
            _In_ mdm2enum::MidiEndpointDeviceWatcher const& watcher,
            _In_ mdm2legacy::MidiLegacyPortDeviceWatcher const& portWatcher)
        {
            std::vector<LiveEndpoint> rebuilt{};

            auto const all = winrt::single_threaded_vector(SortedEndpoints(watcher));

            rebuilt.reserve(all.Size());

            for (auto const& device : all)
            {
                auto endpoint = DescribeEndpoint(device, all);

                if (!endpoint.has_value())
                {
                    continue;
                }

                if (portWatcher != nullptr)
                {
                    for (auto const& port : portWatcher.GetEnumeratedPortsForAssociatedEndpoint(
                        device.EndpointDeviceId(), mdm2enum::Midi1PortFlow::MidiMessageSource))
                    {
                        ClaimPortName(port, endpoint->SourcePortNames);
                    }

                    for (auto const& port : portWatcher.GetEnumeratedPortsForAssociatedEndpoint(
                        device.EndpointDeviceId(), mdm2enum::Midi1PortFlow::MidiMessageDestination))
                    {
                        ClaimPortName(port, endpoint->DestinationPortNames);
                    }
                }

                rebuilt.push_back(std::move(*endpoint));
            }

            return rebuilt;
        }

        std::vector<LiveEndpoint> BuildFromQueries()
        {
            std::vector<LiveEndpoint> rebuilt{};

            auto const all = mdm2enum::MidiEndpointDeviceInformation::FindAll(
                mdm2enum::MidiEndpointDeviceInformationSortOrder::Name,
                mdm2enum::MidiEndpointDeviceInformationFilters::AllStandardEndpoints);

            if (all == nullptr)
            {
                return rebuilt;
            }

            rebuilt.reserve(all.Size());

            auto const sourcePortNames = CollectPortNames(mdm2enum::Midi1PortFlow::MidiMessageSource);
            auto const destinationPortNames = CollectPortNames(mdm2enum::Midi1PortFlow::MidiMessageDestination);

            for (auto const& device : all)
            {
                auto endpoint = DescribeEndpoint(device, all);

                if (!endpoint.has_value())
                {
                    continue;
                }

                auto const portKey = NormalizedEndpointId(device.EndpointDeviceId());

                if (auto const found = sourcePortNames.find(portKey); found != sourcePortNames.end())
                {
                    endpoint->SourcePortNames = found->second;
                }

                if (auto const found = destinationPortNames.find(portKey); found != destinationPortNames.end())
                {
                    endpoint->DestinationPortNames = found->second;
                }

                rebuilt.push_back(std::move(*endpoint));
            }

            return rebuilt;
        }

        // DeviceWatcher throws from Stop unless it is running, and the SDK's Stop is noexcept.
        template <typename TWatcher>
        void StopIfRunning(_In_ TWatcher const& watcher)
        {
            auto const status = watcher.Status();

            if (status == winrt::Windows::Devices::Enumeration::DeviceWatcherStatus::Started ||
                status == winrt::Windows::Devices::Enumeration::DeviceWatcherStatus::EnumerationCompleted)
            {
                watcher.Stop();
            }
        }

        constexpr DWORD FirstPassTimeoutMilliseconds{ 30000 };

        // A device arriving reports a dozen changes over half a second or so, and listeners such
        // as routing should hear about it once. A device that never goes quiet still gets passes.
        constexpr std::chrono::milliseconds ChangeSettleTime{ 250 };
        constexpr std::chrono::milliseconds LongestChangeDelay{ 2000 };

        // Returns once the watcher has reported everything already present, or has stopped.
        template <typename TWatcher>
        void StartAndWaitForFirstPass(_In_ TWatcher const& watcher)
        {
            auto const finished = std::make_shared<wil::unique_event>(wil::EventOptions::ManualReset);

            auto const onFinished = [finished](auto&&, auto&&)
                {
                    finished->SetEvent();
                };

            auto const completedToken = watcher.EnumerationCompleted(onFinished);
            auto const stoppedToken = watcher.Stopped(onFinished);

            auto const unsubscribe = wil::scope_exit([&]() noexcept
                {
                    watcher.EnumerationCompleted(completedToken);
                    watcher.Stopped(stoppedToken);
                });

            watcher.Start();

            if (!finished->wait(FirstPassTimeoutMilliseconds))
            {
                ReportEndpointError(L"A device watcher did not finish its first pass in time.");
            }
        }
    }

    _Use_decl_annotations_
    std::wstring const& LiveEndpoint::PortName(int32_t groupIndex, bool isSource) const noexcept
    {
        static std::wstring const empty{};

        if (groupIndex < 0 || groupIndex >= MaximumGroupCount)
        {
            return empty;
        }

        return isSource
            ? SourcePortNames[static_cast<size_t>(groupIndex)]
            : DestinationPortNames[static_cast<size_t>(groupIndex)];
    }

    _Use_decl_annotations_
    std::wstring const& LiveEndpoint::GroupName(int32_t groupIndex, bool isSource) const noexcept
    {
        static std::wstring const empty{};

        if (groupIndex < 0 || groupIndex >= MaximumGroupCount)
        {
            return empty;
        }

        return isSource
            ? SourceGroupNames[static_cast<size_t>(groupIndex)]
            : DestinationGroupNames[static_cast<size_t>(groupIndex)];
    }

    int32_t LiveEndpoint::SourceGroupCount() const noexcept
    {
        return static_cast<int32_t>(std::count(SourceGroups.begin(), SourceGroups.end(), true));
    }

    int32_t LiveEndpoint::DestinationGroupCount() const noexcept
    {
        return static_cast<int32_t>(std::count(DestinationGroups.begin(), DestinationGroups.end(), true));
    }

    EndpointMatch LiveEndpoint::BuildMatch() const noexcept
    {
        EndpointMatch match{};

        match.EndpointDeviceId = EndpointDeviceId;
        match.DeviceInstanceId = DeviceInstanceId;
        match.UsbVendorId = UsbVendorId;
        match.UsbProductId = UsbProductId;
        match.UsbSerialNumber = UsbSerialNumber;
        match.TransportSuppliedEndpointName = TransportSuppliedName;
        match.ParentDeviceName = ParentDeviceName;

        return match;
    }

    EndpointCatalog& EndpointCatalog::Current() noexcept
    {
        static EndpointCatalog instance{};
        return instance;
    }

    _Use_decl_annotations_
    void EndpointCatalog::SetChangedHandler(std::function<void()> handler) noexcept
    {
        std::scoped_lock guard{ m_lock };
        m_changedHandler = std::move(handler);
    }

    _Use_decl_annotations_
    uint64_t EndpointCatalog::AddChangedHandler(std::function<void()> handler) noexcept
    {
        try
        {
            if (!handler)
            {
                return 0;
            }

            std::scoped_lock guard{ m_lock };

            auto const token = m_nextHandlerToken++;

            m_extraHandlers.emplace_back(token, std::move(handler));

            return token;
        }
        catch (...)
        {
            ReportEndpointError(L"Unable to add an endpoint change handler.");
        }

        return 0;
    }

    _Use_decl_annotations_
    void EndpointCatalog::RemoveChangedHandler(uint64_t token) noexcept
    {
        std::scoped_lock guard{ m_lock };

        std::erase_if(m_extraHandlers, [token](auto const& entry) { return entry.first == token; });
    }

    bool EndpointCatalog::Start() noexcept
    {
        try
        {
            if (m_running.exchange(true))
            {
                return true;
            }

            m_serviceAvailable.store(mdm2::MidiApi::EnsureServiceAvailable(), std::memory_order_relaxed);

            auto const watcher = mdm2enum::MidiEndpointDeviceWatcher::Create(
                mdm2enum::MidiEndpointDeviceInformationFilters::AllStandardEndpoints);

            if (watcher == nullptr)
            {
                m_running.store(false);

                // Nothing more is coming, so nobody should be left waiting for it.
                m_enumerated.store(true, std::memory_order_release);
                NotifyChanged();

                return false;
            }

            auto const onChanged = [this](auto&&, auto&&)
                {
                    OnWatcherChanged();
                };

            m_addedToken = watcher.Added(onChanged);
            m_removedToken = watcher.Removed(onChanged);
            m_updatedToken = watcher.Updated(onChanged);

            {
                std::scoped_lock guard{ m_lock };
                m_watcher = watcher;
            }

            // Every endpoint first, then the MIDI 1.0 ports, which the port watcher answers from
            // its own list instead of another search of the system.
            StartAndWaitForFirstPass(watcher);

            if (!m_running.load())
            {
                StopWatchers();
                return false;
            }

            if (auto const portWatcher = mdm2legacy::MidiLegacyPortDeviceWatcher::Create())
            {
                m_portAddedToken = portWatcher.Added(onChanged);
                m_portRemovedToken = portWatcher.Removed(onChanged);
                m_portUpdatedToken = portWatcher.Updated(onChanged);

                {
                    std::scoped_lock guard{ m_lock };
                    m_portWatcher = portWatcher;
                }

                StartAndWaitForFirstPass(portWatcher);
            }

            if (!m_running.load())
            {
                StopWatchers();
                return false;
            }

            // Whatever was reported up to here is already in the watchers' lists.
            m_changedWhileEnumerating.store(false);

            Rebuild(RebuildSource::Watchers);

            m_enumerated.store(true, std::memory_order_release);

            NotifyChanged();

            if (m_changedWhileEnumerating.exchange(false))
            {
                OnWatcherChanged();
            }

            return true;
        }
        catch (...)
        {
            ReportEndpointError(L"Unable to start the endpoint watcher.");
        }

        StopWatchers();

        m_running.store(false);
        m_enumerated.store(true, std::memory_order_release);
        NotifyChanged();

        return false;
    }

    void EndpointCatalog::Stop() noexcept
    {
        if (!m_running.exchange(false))
        {
            return;
        }

        StopWatchers();

        m_enumerated.store(false, std::memory_order_release);
        m_changedWhileEnumerating.store(false);
    }

    void EndpointCatalog::StopWatchers() noexcept
    {
        try
        {
            mdm2enum::MidiEndpointDeviceWatcher watcher{ nullptr };
            mdm2legacy::MidiLegacyPortDeviceWatcher portWatcher{ nullptr };

            {
                std::scoped_lock guard{ m_lock };

                watcher = std::exchange(m_watcher, nullptr);
                portWatcher = std::exchange(m_portWatcher, nullptr);
            }

            if (portWatcher != nullptr)
            {
                portWatcher.Added(m_portAddedToken);
                portWatcher.Removed(m_portRemovedToken);
                portWatcher.Updated(m_portUpdatedToken);

                StopIfRunning(portWatcher);
            }

            if (watcher != nullptr)
            {
                watcher.Added(m_addedToken);
                watcher.Removed(m_removedToken);
                watcher.Updated(m_updatedToken);

                StopIfRunning(watcher);
            }
        }
        catch (...)
        {
            ReportEndpointError(L"Unable to stop the endpoint watcher.");
        }
    }

    void EndpointCatalog::Refresh() noexcept
    {
        Rebuild(RebuildSource::Query);
        NotifyChanged();
    }

    void EndpointCatalog::OnWatcherChanged() noexcept
    {
        // The first pass in Start covers anything that arrives before it is done.
        if (!m_enumerated.load(std::memory_order_acquire))
        {
            m_changedWhileEnumerating.store(true);
            return;
        }

        // Changes arrive one at a time and often in a burst for one device. The rebuild runs off
        // the watcher's thread, so everything arriving while it runs is covered by one more pass.
        if (m_pendingChanges.fetch_add(1) == 0)
        {
            RebuildForChanges();
        }
    }

    winrt::fire_and_forget EndpointCatalog::RebuildForChanges() noexcept
    {
        try
        {
            uint32_t covered{ 0 };

            do
            {
                auto seen = m_pendingChanges.load();

                for (auto waited = std::chrono::milliseconds{ 0 }; waited < LongestChangeDelay; waited += ChangeSettleTime)
                {
                    co_await winrt::resume_after(ChangeSettleTime);

                    auto const now = m_pendingChanges.load();

                    if (now == seen)
                    {
                        break;
                    }

                    seen = now;
                }

                covered = m_pendingChanges.load();

                Rebuild(RebuildSource::Watchers);
                NotifyChanged();
            }
            while (m_pendingChanges.fetch_sub(covered) != covered);
        }
        catch (...)
        {
            // The next change starts over rather than finding a pass that never ends.
            m_pendingChanges.store(0);
            ReportEndpointError(L"Endpoint watcher notification failed.");
        }
    }

    void EndpointCatalog::NotifyChanged() noexcept
    {
        std::vector<std::function<void()>> handlers{};

        try
        {
            std::scoped_lock guard{ m_lock };

            if (m_changedHandler)
            {
                handlers.push_back(m_changedHandler);
            }

            for (auto const& [token, handler] : m_extraHandlers)
            {
                handlers.push_back(handler);
            }
        }
        catch (...)
        {
            ReportEndpointError(L"Unable to collect the endpoint change handlers.");
            return;
        }

        for (auto const& handler : handlers)
        {
            try
            {
                handler();
            }
            catch (...)
            {
                ReportEndpointError(L"An endpoint change handler failed.");
            }
        }
    }

    _Use_decl_annotations_
    void EndpointCatalog::Rebuild(RebuildSource source) noexcept
    {
        auto const generation = m_rebuildsStarted.fetch_add(1) + 1;

        // Every rebuild is driven by a device arriving, leaving or changing, which is exactly
        // when the service may have gone away, so the flag is refreshed here rather than left as
        // whatever it was when the watcher started. Read only: asking with
        // EnsureServiceAvailable would start the service and the answer would always be yes.
        m_serviceAvailable.store(IsMidiServiceRunning(), std::memory_order_relaxed);

        std::vector<LiveEndpoint> rebuilt{};

        try
        {
            if (source == RebuildSource::Query)
            {
                rebuilt = BuildFromQueries();
            }
            else
            {
                mdm2enum::MidiEndpointDeviceWatcher watcher{ nullptr };
                mdm2legacy::MidiLegacyPortDeviceWatcher portWatcher{ nullptr };

                {
                    std::scoped_lock guard{ m_lock };

                    watcher = m_watcher;
                    portWatcher = m_portWatcher;
                }

                rebuilt = BuildFromWatchers(watcher, portWatcher);
            }

            // The order FindAll gives, which the list has always had, whichever way it was built.
            std::sort(rebuilt.begin(), rebuilt.end(),
                [](LiveEndpoint const& left, LiveEndpoint const& right)
                {
                    return left.Name != right.Name
                        ? left.Name < right.Name
                        : left.EndpointDeviceId < right.EndpointDeviceId;
                });

            m_serviceAvailable.store(true, std::memory_order_relaxed);
        }
        catch (...)
        {
            ReportEndpointError(L"Unable to enumerate endpoints.");
            m_serviceAvailable.store(false, std::memory_order_relaxed);
            return;
        }

        std::scoped_lock guard{ m_lock };

        if (generation < m_newestRebuildApplied)
        {
            return;
        }

        m_newestRebuildApplied = generation;
        m_endpoints = std::move(rebuilt);
    }

    std::vector<LiveEndpoint> EndpointCatalog::Snapshot() const noexcept
    {
        std::scoped_lock guard{ m_lock };
        return m_endpoints;
    }

    _Use_decl_annotations_
    std::optional<LiveEndpoint> EndpointCatalog::Find(std::wstring const& endpointDeviceId) const noexcept
    {
        std::scoped_lock guard{ m_lock };

        auto it = std::find_if(m_endpoints.begin(), m_endpoints.end(),
            [&endpointDeviceId](LiveEndpoint const& e)
            { return EqualsIgnoringCase(e.EndpointDeviceId, endpointDeviceId); });

        if (it == m_endpoints.end())
        {
            return std::nullopt;
        }

        return *it;
    }

    _Use_decl_annotations_
    std::optional<LiveEndpoint> EndpointCatalog::Resolve(
        EndpointMatch const& match,
        EndpointMatchMode mode,
        std::wstring const& fallbackName) const noexcept
    {
        std::scoped_lock guard{ m_lock };

        // The device id is always tried first, whatever the mode, because when it is still there
        // it is unambiguous and the broader modes only exist for when it is not.
        if (!match.EndpointDeviceId.empty())
        {
            auto it = std::find_if(m_endpoints.begin(), m_endpoints.end(),
                [&match](LiveEndpoint const& e)
                { return EqualsIgnoringCase(e.EndpointDeviceId, match.EndpointDeviceId); });

            if (it != m_endpoints.end())
            {
                return *it;
            }
        }

        if (mode == EndpointMatchMode::UsbVendorAndProduct && match.HasUsbIdentity())
        {
            auto it = std::find_if(m_endpoints.begin(), m_endpoints.end(),
                [&match](LiveEndpoint const& e)
                {
                    if (e.UsbVendorId != match.UsbVendorId || e.UsbProductId != match.UsbProductId)
                    {
                        return false;
                    }

                    // a serial number makes the match exact, so honor it when both sides have one
                    if (!match.UsbSerialNumber.empty() && !e.UsbSerialNumber.empty())
                    {
                        return EqualsIgnoringCase(e.UsbSerialNumber, match.UsbSerialNumber);
                    }

                    return true;
                });

            if (it != m_endpoints.end())
            {
                return *it;
            }
        }

        if (mode == EndpointMatchMode::EndpointName)
        {
            auto const& wanted = match.TransportSuppliedEndpointName.empty()
                ? fallbackName
                : match.TransportSuppliedEndpointName;

            auto it = std::find_if(m_endpoints.begin(), m_endpoints.end(),
                [&wanted](LiveEndpoint const& e)
                {
                    return EqualsIgnoringCase(e.TransportSuppliedName, wanted) ||
                        EqualsIgnoringCase(e.Name, wanted);
                });

            if (it != m_endpoints.end())
            {
                return *it;
            }
        }

        return std::nullopt;
    }

    _Use_decl_annotations_
    std::optional<LiveEndpoint> EndpointCatalog::SuggestReplacement(
        EndpointMatch const& match,
        EndpointMatchMode mode,
        std::wstring const& fallbackName) const noexcept
    {
        if (Resolve(match, mode, fallbackName).has_value())
        {
            return std::nullopt;
        }

        std::scoped_lock guard{ m_lock };

        // USB identity first: the same model in a different port is by far the most common
        // reason a device id stops resolving.
        if (match.HasUsbIdentity())
        {
            auto it = std::find_if(m_endpoints.begin(), m_endpoints.end(),
                [&match](LiveEndpoint const& e)
                {
                    return e.UsbVendorId == match.UsbVendorId &&
                        e.UsbProductId == match.UsbProductId;
                });

            if (it != m_endpoints.end())
            {
                return *it;
            }
        }

        auto const& wanted = match.TransportSuppliedEndpointName.empty()
            ? fallbackName
            : match.TransportSuppliedEndpointName;

        auto it = std::find_if(m_endpoints.begin(), m_endpoints.end(),
            [&wanted](LiveEndpoint const& e)
            {
                return EqualsIgnoringCase(e.TransportSuppliedName, wanted) ||
                    EqualsIgnoringCase(e.Name, wanted);
            });

        if (it != m_endpoints.end())
        {
            return *it;
        }

        return std::nullopt;
    }
}
