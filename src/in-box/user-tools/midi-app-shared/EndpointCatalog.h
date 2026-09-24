// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

namespace midiapp
{
    constexpr int32_t MaximumGroupCount = 16;

    // Untrusted input guard. Names reach these tools from files other software can write and from
    // devices that supply whatever they like, so everything stored or displayed is bounded first.
    constexpr size_t MaximumStringLength = 1024;

    // Trims, bounds and strips control characters.
    std::wstring SanitizeStoredString(_In_ std::wstring value) noexcept;

    // Where a swallowed exception goes. Shared code cannot reach any one app's telemetry, so the
    // app supplies the sink it already logs to. Set it before starting the catalog.
    void SetEndpointErrorHandler(_In_ std::function<void(std::wstring_view)> handler) noexcept;
    void ReportEndpointError(_In_ std::wstring_view message) noexcept;

    // How a saved endpoint is matched back to a live one. The default is the exact device id,
    // matching what endpoint customization does; the other two only apply after the customer
    // has confirmed them for that endpoint.
    enum class EndpointMatchMode
    {
        EndpointDeviceId = 0,
        UsbVendorAndProduct = 1,
        EndpointName = 2,
    };

    // The same fields, and the same JSON keys, that the service configuration uses for its
    // "match" object. Serialized through MidiServiceConfigEndpointMatchCriteria so the two can
    // never drift, which is what makes folding this into the configuration later a copy rather
    // than a translation.
    struct EndpointMatch
    {
        std::wstring EndpointDeviceId{};
        std::wstring DeviceInstanceId{};
        uint16_t UsbVendorId{ 0 };
        uint16_t UsbProductId{ 0 };
        std::wstring UsbSerialNumber{};
        std::wstring TransportSuppliedEndpointName{};
        std::wstring ParentDeviceName{};

        bool HasUsbIdentity() const noexcept { return UsbVendorId != 0 || UsbProductId != 0; }
    };

    winrt::Windows::Data::Json::JsonObject MatchToJson(_In_ EndpointMatch const& match) noexcept;
    EndpointMatch MatchFromJson(_In_ winrt::Windows::Data::Json::JsonObject const& value) noexcept;

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

        // Full path of the picture the customer gave this endpoint, if there is one.
        std::wstring ImagePath{};

        uint16_t UsbVendorId{ 0 };
        uint16_t UsbProductId{ 0 };
        std::wstring UsbSerialNumber{};

        std::array<bool, MaximumGroupCount> DeclaredGroups{};

        // The names these groups carry as MIDI 1.0 ports, which is what the customer already
        // sees in every other app. Empty where the endpoint has no MIDI 1.0 port for the group.
        std::array<std::wstring, MaximumGroupCount> SourcePortNames{};
        std::array<std::wstring, MaximumGroupCount> DestinationPortNames{};

        // Loopbacks are the only endpoints an app knows for certain will echo what it sends,
        // which is what makes a loop provable rather than merely possible.
        bool IsLoopback{ false };

        // For a MIDI 2.0 loopback pair, the other side. Empty for a basic loopback, which echoes
        // to itself.
        std::wstring LoopbackPartnerEndpointId{};

        EndpointMatch BuildMatch() const noexcept;

        std::wstring const& PortName(_In_ int32_t groupIndex, _In_ bool isSource) const noexcept;
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

        bool Start() noexcept;
        void Stop() noexcept;

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

        // Forces a rebuild, used after creating a loopback so the new endpoint shows up without
        // waiting for the watcher.
        void Refresh() noexcept;

    private:
        EndpointCatalog() noexcept = default;

        void Rebuild() noexcept;
        void NotifyChanged() noexcept;

        mutable std::mutex m_lock{};
        std::vector<LiveEndpoint> m_endpoints{};

        std::function<void()> m_changedHandler{};

        winrt::Windows::Devices::Midi2::Enumeration::MidiEndpointDeviceWatcher m_watcher{ nullptr };

        winrt::event_token m_addedToken{};
        winrt::event_token m_removedToken{};
        winrt::event_token m_updatedToken{};

        std::atomic<bool> m_serviceAvailable{ false };
        std::atomic<bool> m_running{ false };
    };
}
