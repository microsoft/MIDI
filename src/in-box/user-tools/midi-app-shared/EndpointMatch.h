// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// How an endpoint is identified and matched, with no watcher and no enumeration behind it. Kept
// apart from EndpointCatalog.h so a document layer, or a unit test with no devices and no
// precompiled header, can use these without dragging in the live catalog. Self-sufficient on
// purpose: it includes what it needs rather than relying on a consuming app's pch.h.

#include <sal.h>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Data.Json.h>

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
}
