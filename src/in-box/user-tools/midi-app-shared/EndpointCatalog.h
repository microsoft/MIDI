// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "EndpointMatch.h"
#include "MidiServiceStatus.h"

namespace midiapp
{
    // A snapshot of one endpoint that is present right now. Everything a canvas, a graph checker
    // or a routing engine needs, copied out of the watcher so nothing on the UI thread iterates a
    // collection the watcher is rewriting.
    struct LiveEndpoint
    {
        std::wstring EndpointDeviceId{};
        std::wstring Name{};
        std::wstring TransportCode{};
        std::wstring ManufacturerName{};
        std::wstring DeviceInstanceId{};
        std::wstring ParentDeviceName{};
        std::wstring TransportSuppliedName{};

        // What the customer or the transport says this thing is, for a picker that has room for
        // a second line. A transport code tells nobody anything.
        std::wstring Description{};

        // Full path of the picture the customer gave this endpoint, if there is one.
        std::wstring ImagePath{};

        uint16_t UsbVendorId{ 0 };
        uint16_t UsbProductId{ 0 };
        std::wstring UsbSerialNumber{};

        std::array<bool, MaximumGroupCount> DeclaredGroups{};

        // The same, split by which way the messages go, so a picker can say "1 source group,
        // 3 destination groups" rather than a count that does not say what it is for.
        std::array<bool, MaximumGroupCount> SourceGroups{};
        std::array<bool, MaximumGroupCount> DestinationGroups{};

        int32_t SourceGroupCount() const noexcept;
        int32_t DestinationGroupCount() const noexcept;

        // The names these groups carry as MIDI 1.0 ports, which is what the customer already
        // sees in every other app. Empty where the endpoint has no MIDI 1.0 port for the group.
        std::array<std::wstring, MaximumGroupCount> SourcePortNames{};
        std::array<std::wstring, MaximumGroupCount> DestinationPortNames{};

        // Function block name per group, falling back to the group terminal block name.
        std::array<std::wstring, MaximumGroupCount> SourceGroupNames{};
        std::array<std::wstring, MaximumGroupCount> DestinationGroupNames{};

        // Read from what the watcher already holds, so nobody has to query the device for it.
        bool SupportsMidi2Protocol{ false };

        // Loopbacks are the only endpoints an app knows for certain will echo what it sends,
        // which is what makes a loop provable rather than merely possible.
        bool IsLoopback{ false };

        // For a MIDI 2.0 loopback pair, the other side. Empty for a basic loopback, which echoes
        // to itself.
        std::wstring LoopbackPartnerEndpointId{};

        EndpointMatch BuildMatch() const noexcept;

        std::wstring const& PortName(_In_ int32_t groupIndex, _In_ bool isSource) const noexcept;

        std::wstring const& GroupName(_In_ int32_t groupIndex, _In_ bool isSource) const noexcept;
    };

    // Watches the live endpoints and answers "which live endpoint does this saved one mean".
    //
    // The watcher callbacks arrive on their own threads, so the snapshot is rebuilt off the UI
    // thread and the change notification is marshalled by the caller.
    class EndpointCatalog
    {
    public:
        static EndpointCatalog& Current() noexcept;

        // Raised after the snapshot has been replaced. Called on a background thread.
        void SetChangedHandler(_In_ std::function<void()> handler) noexcept;

        // The same, for something that comes and goes, such as a dialog, while another part of
        // the app owns the handler above. Returns 0 when it could not be added.
        uint64_t AddChangedHandler(_In_ std::function<void()> handler) noexcept;
        void RemoveChangedHandler(_In_ uint64_t token) noexcept;

        bool Start() noexcept;
        void Stop() noexcept;

        // False until the first full look at the endpoints is done, which takes a while on a PC
        // with many devices. Until then an empty snapshot means "not looked yet", not "nothing".
        bool HasEnumerated() const noexcept { return m_enumerated.load(std::memory_order_acquire); }

        // A copy, deliberately: callers hold it while they walk a patch.
        std::vector<LiveEndpoint> Snapshot() const noexcept;

        std::optional<LiveEndpoint> Find(_In_ std::wstring const& endpointDeviceId) const noexcept;

        // The endpoint a saved match resolves to under its mode, or nothing. The fallback name is
        // used for a name match when the match itself carries no transport supplied name, which is
        // how a caller offers the display name it last saw.
        std::optional<LiveEndpoint> Resolve(
            _In_ EndpointMatch const& match,
            _In_ EndpointMatchMode mode,
            _In_ std::wstring const& fallbackName = {}) const noexcept;

        // A live endpoint that is probably the saved one but does not match under the current
        // mode, so it can be offered rather than bound silently. Only returns something when
        // the endpoint is not already resolved.
        std::optional<LiveEndpoint> SuggestReplacement(
            _In_ EndpointMatch const& match,
            _In_ EndpointMatchMode mode,
            _In_ std::wstring const& fallbackName = {}) const noexcept;

        bool IsServiceAvailable() const noexcept { return m_serviceAvailable.load(std::memory_order_relaxed); }

        // Asks Windows directly instead of reading the watchers' lists, used after creating a
        // loopback so the new endpoint shows up without waiting for the watcher. Slower.
        void Refresh() noexcept;

    private:
        EndpointCatalog() noexcept = default;

        enum class RebuildSource
        {
            Watchers,
            Query,
        };

        void Rebuild(_In_ RebuildSource source) noexcept;
        void NotifyChanged() noexcept;
        void OnWatcherChanged() noexcept;
        void StopWatchers() noexcept;
        winrt::fire_and_forget RebuildForChanges() noexcept;

        mutable std::mutex m_lock{};
        std::vector<LiveEndpoint> m_endpoints{};

        std::function<void()> m_changedHandler{};
        std::vector<std::pair<uint64_t, std::function<void()>>> m_extraHandlers{};
        uint64_t m_nextHandlerToken{ 1 };

        // Rebuilds can overlap, so each takes a number, and a slow one that started first never
        // puts back an older list. The newest applied is guarded by m_lock.
        std::atomic<uint64_t> m_rebuildsStarted{ 0 };
        uint64_t m_newestRebuildApplied{ 0 };

        // Watcher changes not yet covered by a rebuild. Whoever takes it from zero rebuilds.
        std::atomic<uint32_t> m_pendingChanges{ 0 };

        std::atomic<bool> m_enumerated{ false };
        std::atomic<bool> m_changedWhileEnumerating{ false };

        // Guarded by m_lock, because a rebuild on another thread reads them.
        winrt::Windows::Devices::Midi2::Enumeration::MidiEndpointDeviceWatcher m_watcher{ nullptr };
        winrt::Windows::Devices::Midi2::Enumeration::Legacy::MidiLegacyPortDeviceWatcher m_portWatcher{ nullptr };

        winrt::event_token m_addedToken{};
        winrt::event_token m_removedToken{};
        winrt::event_token m_updatedToken{};

        winrt::event_token m_portAddedToken{};
        winrt::event_token m_portRemovedToken{};
        winrt::event_token m_portUpdatedToken{};

        std::atomic<bool> m_serviceAvailable{ false };
        std::atomic<bool> m_running{ false };
    };
}
