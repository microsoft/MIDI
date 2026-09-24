// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "MessageFilter.h"
#include "MessageTransform.h"

// Endpoint identity, matching and the live catalog are shared with the other MIDI tools.
#include "EndpointCatalog.h"

namespace midipatchbay
{
    // The shared names, usable unqualified throughout this app the way they always were.
    using EndpointMatch = midiapp::EndpointMatch;
    using EndpointMatchMode = midiapp::EndpointMatchMode;
    using LiveEndpoint = midiapp::LiveEndpoint;
    using EndpointCatalog = midiapp::EndpointCatalog;

    using midiapp::MatchFromJson;
    using midiapp::MatchToJson;
    using midiapp::SanitizeStoredString;

    constexpr int32_t MaximumGroupCount = midiapp::MaximumGroupCount;
    constexpr size_t MaximumStringLength = midiapp::MaximumStringLength;

    // A connection point that carries every group untouched. Stored as -1 so a group index and
    // "all groups" can share one field, the way the group index does elsewhere in this app.
    constexpr int32_t AllGroups = -1;

    // Untrusted input guards. These files live in the customer's Documents folder, which other
    // software can write to, so everything read back is bounded before it reaches the UI.
    constexpr size_t MaximumPatchFileBytes = 4 * 1024 * 1024;
    constexpr size_t MaximumEndpointsPerPatch = 64;
    constexpr size_t MaximumConnectionsPerPatch = 512;
    constexpr size_t MaximumPatchCount = 256;

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

        MessageFilter Filter{};
        MessageTransform Transform{};
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

    // "Group 3 - Iridium Aux", or the all-groups caption. Never returns empty.
    winrt::hstring DescribeGroupIndex(_In_ int32_t groupIndex, _In_ std::wstring const& portName) noexcept;

    // The shared catalog matches on criteria alone, so these are the one place that knows a
    // saved endpoint keeps its criteria, its mode and the name it last went by.
    std::optional<LiveEndpoint> ResolveEndpoint(_In_ PatchEndpoint const& endpoint) noexcept;
    std::optional<LiveEndpoint> SuggestReplacementFor(_In_ PatchEndpoint const& endpoint) noexcept;
}
