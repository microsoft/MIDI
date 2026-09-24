// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "EndpointCatalog.h"
#include "MidiEndpointHelpers.h"

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

    bool EndpointCatalog::Start() noexcept
    {
        try
        {
            if (m_running.exchange(true))
            {
                return true;
            }

            m_serviceAvailable.store(mdm2::MidiApi::EnsureServiceAvailable(), std::memory_order_relaxed);

            m_watcher = mdm2enum::MidiEndpointDeviceWatcher::Create(
                mdm2enum::MidiEndpointDeviceInformationFilters::AllStandardEndpoints);

            if (m_watcher == nullptr)
            {
                m_running.store(false);
                return false;
            }

            auto const onChanged = [this](auto&&, auto&&)
                {
                    try
                    {
                        Rebuild();
                        NotifyChanged();
                    }
                    catch (...)
                    {
                        ReportEndpointError(L"Endpoint watcher notification failed.");
                    }
                };

            m_addedToken = m_watcher.Added(onChanged);
            m_removedToken = m_watcher.Removed(onChanged);
            m_updatedToken = m_watcher.Updated(onChanged);

            m_watcher.Start();

            Rebuild();

            return true;
        }
        catch (...)
        {
            ReportEndpointError(L"Unable to start the endpoint watcher.");
        }

        m_running.store(false);
        return false;
    }

    void EndpointCatalog::Stop() noexcept
    {
        try
        {
            if (!m_running.exchange(false))
            {
                return;
            }

            if (m_watcher != nullptr)
            {
                m_watcher.Added(m_addedToken);
                m_watcher.Removed(m_removedToken);
                m_watcher.Updated(m_updatedToken);

                m_watcher.Stop();
                m_watcher = nullptr;
            }
        }
        catch (...)
        {
            ReportEndpointError(L"Unable to stop the endpoint watcher.");
        }
    }

    void EndpointCatalog::Refresh() noexcept
    {
        Rebuild();
        NotifyChanged();
    }

    void EndpointCatalog::NotifyChanged() noexcept
    {
        std::function<void()> handler{};

        {
            std::scoped_lock guard{ m_lock };
            handler = m_changedHandler;
        }

        if (handler)
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

    void EndpointCatalog::Rebuild() noexcept
    {
        // Every rebuild is driven by a device arriving, leaving or changing, which is exactly
        // when the service may have gone away, so the flag is refreshed here rather than left as
        // whatever it was when the watcher started. Read only: asking with
        // EnsureServiceAvailable would start the service and the answer would always be yes.
        m_serviceAvailable.store(IsMidiServiceRunning(), std::memory_order_relaxed);

        std::vector<LiveEndpoint> rebuilt{};

        try
        {
            auto const all = mdm2enum::MidiEndpointDeviceInformation::FindAll(
                mdm2enum::MidiEndpointDeviceInformationSortOrder::Name,
                mdm2enum::MidiEndpointDeviceInformationFilters::AllStandardEndpoints);

            if (all != nullptr)
            {
                rebuilt.reserve(all.Size());

                for (auto const& device : all)
                {
                    if (device == nullptr)
                    {
                        continue;
                    }

                    LiveEndpoint endpoint{};

                    endpoint.EndpointDeviceId = SafeString(device.EndpointDeviceId());
                    endpoint.Name = SafeString(device.Name());
                    endpoint.DeviceInstanceId = SafeString(device.DeviceInstanceId());

                    if (endpoint.EndpointDeviceId.empty())
                    {
                        continue;
                    }

                    auto const transportInfo = device.GetTransportSuppliedInfo();

                    endpoint.TransportCode = SafeString(transportInfo.TransportCode());
                    endpoint.TransportSuppliedName = SafeString(transportInfo.Name());
                    endpoint.ManufacturerName = SafeString(transportInfo.ManufacturerName());
                    endpoint.UsbVendorId = transportInfo.VendorId();
                    endpoint.UsbProductId = transportInfo.ProductId();
                    endpoint.UsbSerialNumber = SafeString(transportInfo.SerialNumber());

                    auto const parent = device.GetParentDeviceInformation();

                    if (parent != nullptr)
                    {
                        endpoint.ParentDeviceName = SafeString(parent.Name());
                    }

                    if (auto const userInfo = device.GetUserSuppliedInfo())
                    {
                        endpoint.ImagePath = SafeString(ResolveEndpointImagePath(userInfo.ImageFileName()));
                    }

                    endpoint.DeclaredGroups = DeclaredGroups(device);

                    // The MIDI 1.0 port names are what the customer already sees everywhere
                    // else, so they are the labels on the connection points.
                    auto const sourcePorts = mdm2legacy::MidiLegacyPortDeviceInformation::FindAllForAssociatedEndpoint(
                        winrt::hstring{ endpoint.EndpointDeviceId },
                        mdm2enum::Midi1PortFlow::MidiMessageSource);

                    if (sourcePorts != nullptr)
                    {
                        for (auto const& port : sourcePorts)
                        {
                            if (port == nullptr || port.Group() == nullptr)
                            {
                                continue;
                            }

                            auto const index = static_cast<int32_t>(port.Group().Index());

                            if (index >= 0 && index < MaximumGroupCount)
                            {
                                endpoint.SourcePortNames[static_cast<size_t>(index)] = SafeString(port.Name());
                            }
                        }
                    }

                    auto const destinationPorts = mdm2legacy::MidiLegacyPortDeviceInformation::FindAllForAssociatedEndpoint(
                        winrt::hstring{ endpoint.EndpointDeviceId },
                        mdm2enum::Midi1PortFlow::MidiMessageDestination);

                    if (destinationPorts != nullptr)
                    {
                        for (auto const& port : destinationPorts)
                        {
                            if (port == nullptr || port.Group() == nullptr)
                            {
                                continue;
                            }

                            auto const index = static_cast<int32_t>(port.Group().Index());

                            if (index >= 0 && index < MaximumGroupCount)
                            {
                                endpoint.DestinationPortNames[static_cast<size_t>(index)] = SafeString(port.Name());
                            }
                        }
                    }

                    if (EqualsIgnoringCase(endpoint.TransportCode, TransportCodeBasicLoopback))
                    {
                        endpoint.IsLoopback = true;
                    }
                    else if (EqualsIgnoringCase(endpoint.TransportCode, TransportCodeLoopback))
                    {
                        endpoint.IsLoopback = true;

                        auto const partner = mdm2loop::MidiLoopbackManager::GetAssociatedLoopbackEndpoint(device);

                        if (partner != nullptr)
                        {
                            endpoint.LoopbackPartnerEndpointId = SafeString(partner.EndpointDeviceId());
                        }
                    }

                    rebuilt.push_back(std::move(endpoint));
                }
            }

            m_serviceAvailable.store(true, std::memory_order_relaxed);
        }
        catch (...)
        {
            ReportEndpointError(L"Unable to enumerate endpoints.");
            m_serviceAvailable.store(false, std::memory_order_relaxed);
            return;
        }

        std::scoped_lock guard{ m_lock };
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
