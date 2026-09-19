// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

namespace midipatchbay
{
    // A connection point that carries every group untouched. Stored as -1 so a group index and
    // "all groups" can share one field, the way the group index does elsewhere in this app.
    constexpr int32_t AllGroups = -1;

    constexpr int32_t MaximumGroupCount = 16;

    // Untrusted input guards. These files live in the customer's Documents folder, which other
    // software can write to, so everything read back is bounded before it reaches the UI.
    constexpr size_t MaximumPatchFileBytes = 4 * 1024 * 1024;
    constexpr size_t MaximumStringLength = 1024;
    constexpr size_t MaximumEndpointsPerPatch = 64;
    constexpr size_t MaximumConnectionsPerPatch = 512;
    constexpr size_t MaximumPatchCount = 256;

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

    // An endpoint placed on the canvas.
    struct PatchEndpoint
    {
        std::wstring Id{};                  // stable within the patch, referenced by connections
        std::wstring DisplayName{};         // last known name, so an absent device still reads right
        std::wstring TransportCode{};
        EndpointMatch Match{};
        EndpointMatchMode MatchMode{ EndpointMatchMode::EndpointDeviceId };

        double CanvasX{ 0 };
        double CanvasY{ 0 };

        // Devices that declare nothing get one group; this opts a node into showing all sixteen.
        bool ShowAllGroups{ false };
    };

    // One hop: everything arriving on the source group is sent to the destination group.
    struct PatchConnection
    {
        std::wstring Id{};
        std::wstring SourceEndpointId{};
        int32_t SourceGroupIndex{ AllGroups };
        std::wstring DestinationEndpointId{};
        int32_t DestinationGroupIndex{ AllGroups };
        bool Muted{ false };
    };

    // A patch is a file. Nothing in here touches WinRT UI types, so this whole layer is what a
    // future API would be built over.
    struct PatchDocument
    {
        std::wstring Name{};
        std::wstring Description{};

        // Empty while the patch has never been written, which is also what makes it temporary.
        std::wstring FilePath{};

        bool IsTemporary{ false };
        bool ActivateAtStartup{ true };

        // Seconds since 1970. Kept small enough to survive a JSON number exactly.
        int64_t CreatedTimestamp{ 0 };
        int64_t ModifiedTimestamp{ 0 };

        std::vector<PatchEndpoint> Endpoints{};
        std::vector<PatchConnection> Connections{};

        PatchEndpoint* FindEndpoint(_In_ std::wstring const& id) noexcept;
        PatchEndpoint const* FindEndpoint(_In_ std::wstring const& id) const noexcept;

        PatchConnection* FindConnection(_In_ std::wstring const& id) noexcept;
        PatchConnection const* FindConnection(_In_ std::wstring const& id) const noexcept;

        // True when the same source point is already wired to the same destination point.
        bool HasConnection(
            _In_ std::wstring const& sourceEndpointId,
            _In_ int32_t sourceGroupIndex,
            _In_ std::wstring const& destinationEndpointId,
            _In_ int32_t destinationGroupIndex) const noexcept;

        void RemoveEndpoint(_In_ std::wstring const& id) noexcept;
        void RemoveConnection(_In_ std::wstring const& id) noexcept;

        static std::wstring NewId() noexcept;
    };

    // Trims, bounds and strips control characters. Used on everything read from a patch file.
    std::wstring SanitizeStoredString(_In_ std::wstring value) noexcept;

    // "Group 3 - Iridium Aux", or the all-groups caption. Never returns empty.
    winrt::hstring DescribeGroupIndex(_In_ int32_t groupIndex, _In_ std::wstring const& portName) noexcept;

    json::JsonObject MatchToJson(_In_ EndpointMatch const& match) noexcept;
    EndpointMatch MatchFromJson(_In_ json::JsonObject const& value) noexcept;
}
