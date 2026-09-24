// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "DeviceCatalog.h"

namespace glass
{
    namespace
    {
        // Display only, and only so the editor can show what a MIDI 1.0 device receives. What
        // leaves the app is the same either way: the service owns the downscale.
        DestinationProtocol ProtocolOf(_In_ std::wstring const& endpointDeviceId) noexcept
        {
            try
            {
                auto const information =
                    midi2enum::MidiEndpointDeviceInformation::CreateFromEndpointDeviceId(
                        winrt::hstring{ endpointDeviceId });

                if (information != nullptr &&
                    information.GetDeclaredEndpointInfo().SupportsMidi20Protocol())
                {
                    return DestinationProtocol::Midi2;
                }
            }
            catch (...)
            {
            }

            return DestinationProtocol::Midi1;
        }

        // The shared watcher takes exactly one changed handler, so it gets one, and that one
        // walks everybody who is interested. Keeping this here rather than in the shared catalog
        // is deliberate: one consumer wanting several subscribers is not yet a reason to change
        // code MIDI Patchbay also depends on.
        std::mutex& RegistryLock() noexcept
        {
            static std::mutex lock{};
            return lock;
        }

        std::vector<DeviceCatalog*>& Registry() noexcept
        {
            static std::vector<DeviceCatalog*> registry{};
            return registry;
        }

        void NotifyEveryCatalog() noexcept
        {
            std::vector<DeviceCatalog*> catalogs{};

            {
                std::scoped_lock guard{ RegistryLock() };
                catalogs = Registry();
            }

            for (auto* const catalog : catalogs)
            {
                catalog->Refresh();
            }
        }
    }

    DeviceCatalog::~DeviceCatalog() noexcept
    {
        Stop();
    }

    bool DeviceCatalog::Start() noexcept
    {
        bool installHandler{ false };

        {
            std::scoped_lock guard{ m_lock };

            if (m_started)
            {
                return true;
            }

            m_started = true;
        }

        {
            std::scoped_lock guard{ RegistryLock() };

            installHandler = Registry().empty();

            Registry().push_back(this);
        }

        if (installHandler)
        {
            midiapp::EndpointCatalog::Current().SetChangedHandler([]() { NotifyEveryCatalog(); });
        }

        auto const started = midiapp::EndpointCatalog::Current().Start();

        Refresh();

        return started;
    }

    void DeviceCatalog::Stop() noexcept
    {
        {
            std::scoped_lock guard{ m_lock };

            if (!m_started)
            {
                return;
            }

            m_started = false;
            m_changedHandler = nullptr;
        }

        std::scoped_lock guard{ RegistryLock() };

        auto& registry = Registry();

        registry.erase(std::remove(registry.begin(), registry.end(), this), registry.end());
    }

    _Use_decl_annotations_
    void DeviceCatalog::SetChangedHandler(std::function<void()> handler) noexcept
    {
        std::scoped_lock guard{ m_lock };
        m_changedHandler = std::move(handler);
    }

    _Use_decl_annotations_
    void DeviceCatalog::SetDocument(LayoutDocument const& document) noexcept
    {
        {
            std::scoped_lock guard{ m_lock };

            m_entries = document.Devices;
            m_groupMasks = CollectGroupMasks(document);
        }

        Refresh();
    }

    void DeviceCatalog::Refresh() noexcept
    {
        Resolve();

        std::function<void()> handler{};

        {
            std::scoped_lock guard{ m_lock };
            handler = m_changedHandler;
        }

        if (handler)
        {
            handler();
        }
    }

    void DeviceCatalog::Resolve() noexcept
    {
        std::vector<DeviceEntry> entries{};
        std::vector<uint16_t> masks{};

        {
            std::scoped_lock guard{ m_lock };
            entries = m_entries;
            masks = m_groupMasks;
        }

        std::vector<ResolvedDevice> resolved{};
        resolved.reserve(entries.size());

        auto const& catalog = midiapp::EndpointCatalog::Current();

        for (size_t i = 0; i < entries.size(); ++i)
        {
            auto const& entry = entries[i];

            ResolvedDevice device{};
            device.Name = entry.Name;
            device.GroupMask = i < masks.size() ? masks[i] : uint16_t{ 0 };

            auto const live = catalog.Resolve(entry.Match, entry.MatchMode, entry.Name);

            if (live.has_value())
            {
                device.EndpointDeviceId = live->EndpointDeviceId;
                device.ResolvedName = live->Name;
                device.Protocol = ProtocolOf(live->EndpointDeviceId);
                device.IsAvailable = true;
            }
            else
            {
                auto const suggestion = catalog.SuggestReplacement(entry.Match, entry.MatchMode, entry.Name);

                if (suggestion.has_value())
                {
                    device.SuggestedEndpointDeviceId = suggestion->EndpointDeviceId;
                    device.SuggestedName = suggestion->Name;
                }
            }

            resolved.push_back(std::move(device));
        }

        std::scoped_lock guard{ m_lock };
        m_resolved = std::move(resolved);
    }

    std::vector<ResolvedDevice> DeviceCatalog::Devices() const noexcept
    {
        std::scoped_lock guard{ m_lock };
        return m_resolved;
    }

    std::vector<PreparedDestination> DeviceCatalog::BuildDestinations() const noexcept
    {
        std::scoped_lock guard{ m_lock };

        std::vector<PreparedDestination> destinations{};
        destinations.reserve(m_resolved.size());

        for (auto const& device : m_resolved)
        {
            PreparedDestination destination{};

            destination.Name = device.Name;
            destination.Protocol = device.Protocol;
            destination.IsAvailable = device.IsAvailable;

            destinations.push_back(std::move(destination));
        }

        return destinations;
    }

    _Use_decl_annotations_
    void DeviceCatalog::BuildOutputRequests(
        std::vector<std::wstring>& endpointDeviceIds,
        std::vector<uint16_t>& groupMasks) const noexcept
    {
        std::scoped_lock guard{ m_lock };

        endpointDeviceIds.clear();
        groupMasks.clear();

        endpointDeviceIds.reserve(m_resolved.size());
        groupMasks.reserve(m_resolved.size());

        for (auto const& device : m_resolved)
        {
            endpointDeviceIds.push_back(device.IsAvailable ? device.EndpointDeviceId : std::wstring{});
            groupMasks.push_back(device.GroupMask);
        }
    }

    size_t DeviceCatalog::AvailableCount() const noexcept
    {
        std::scoped_lock guard{ m_lock };

        return static_cast<size_t>(std::count_if(m_resolved.begin(), m_resolved.end(),
            [](ResolvedDevice const& device) { return device.IsAvailable; }));
    }

    size_t DeviceCatalog::MissingCount() const noexcept
    {
        std::scoped_lock guard{ m_lock };

        return static_cast<size_t>(std::count_if(m_resolved.begin(), m_resolved.end(),
            [](ResolvedDevice const& device) { return !device.IsAvailable; }));
    }
}
