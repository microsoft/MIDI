// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <sal.h>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

#include "EndpointCatalog.h"
#include "LayoutModel.h"
#include "BindingEngine.h"

namespace glass
{
    // One entry of a layout's device table, matched against what is plugged in right now.
    struct ResolvedDevice
    {
        // The name the layout uses. This is what a control's messages point at, and it never
        // changes when the hardware does.
        std::wstring Name{};

        // Empty while nothing matches. A control bound to it stops sending rather than throwing,
        // and comes back on its own when the device returns.
        std::wstring EndpointDeviceId{};

        // What the endpoint is actually called on this PC, so the customer can tell whether the
        // match found what they meant.
        std::wstring ResolvedName{};

        DestinationProtocol Protocol{ DestinationProtocol::Midi2 };

        // Groups this layout drives on this device, one bit per group. Panic uses it, so that a
        // panic is loud where the layout was playing and silent everywhere else.
        uint16_t GroupMask{ 0 };

        bool IsAvailable{ false };

        // Present, but only under a looser rule than the one saved. Offered rather than bound,
        // the same way MIDI Patchbay offers a replacement.
        std::wstring SuggestedEndpointDeviceId{};
        std::wstring SuggestedName{};
    };

    // The layout's device table, resolved against the live endpoints, and kept up to date.
    //
    // This is the layer that turns "the layout wants a thing called Main Synth" into "that is
    // endpoint \\?\swd#... and it is here". Matching itself is the shared code every tool in this
    // family uses, so a layout built next to MIDI Patchbay finds the same hardware.
    class DeviceCatalog
    {
    public:
        DeviceCatalog() noexcept = default;
        ~DeviceCatalog() noexcept;

        DeviceCatalog(DeviceCatalog const&) = delete;
        DeviceCatalog& operator=(DeviceCatalog const&) = delete;

        // Starts the shared endpoint watcher if it is not already running, and joins the list of
        // catalogs it notifies.
        //
        // The shared watcher takes exactly one changed handler, so several running layouts cannot
        // each subscribe to it: the second would silently replace the first and the first layout
        // would stop noticing devices. One process-wide subscriber fans out to all of them.
        bool Start() noexcept;

        // Raised after the table has been re-resolved, on a background thread. The window layer
        // marshals; nothing here calls up into the UI.
        void SetChangedHandler(_In_ std::function<void()> handler) noexcept;

        // The layout's device table, plus the groups each control actually uses, so the catalog
        // can report what a panic has to cover.
        void SetDocument(_In_ LayoutDocument const& document) noexcept;

        void Refresh() noexcept;

        std::vector<ResolvedDevice> Devices() const noexcept;

        // What BindingEngine::Prepare wants. Same order as the layout's device table, which is
        // the order every destination index in the engine refers to.
        std::vector<PreparedDestination> BuildDestinations() const noexcept;

        // Ids and group masks for the router, one per device table entry. An entry with no live
        // endpoint carries an empty id and is skipped rather than dropped, so the indexes still
        // line up with the engine's.
        void BuildOutputRequests(
            _Out_ std::vector<std::wstring>& endpointDeviceIds,
            _Out_ std::vector<uint16_t>& groupMasks) const noexcept;

        size_t AvailableCount() const noexcept;
        size_t MissingCount() const noexcept;

        // Leaves the list of catalogs the watcher notifies. Called by the destructor as well,
        // because a handler left behind pointing at a closed window is a use after free waiting
        // for somebody to plug something in.
        void Stop() noexcept;

    private:
        void Resolve() noexcept;

        mutable std::mutex m_lock{};

        std::vector<DeviceEntry> m_entries{};
        std::vector<uint16_t> m_groupMasks{};
        std::vector<ResolvedDevice> m_resolved{};

        std::function<void()> m_changedHandler{};

        bool m_started{ false };
    };
}
