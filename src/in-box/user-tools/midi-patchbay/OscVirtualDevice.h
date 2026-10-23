// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// An OSC connection as a MIDI device other apps can open, like a DAW, made with the Windows MIDI
// Services Virtual Device API the way the Sound Module and the Sequencer make theirs. What arrives
// over OSC goes to the apps that opened it, and what they send goes out over OSC. Each row of the
// connection is a function block of its own, on its group, named for its address.

#include "OscConnection.h"

#include <functional>
#include <memory>

namespace midipatchbay
{
    struct OscDeviceBlock
    {
        uint8_t Group{ 0 };
        std::wstring Name{};

        bool operator==(OscDeviceBlock const&) const = default;
    };

    // What a connection's device is made from. When it changes, the device is made again. The
    // product instance id is the connection's own, and gets a suffix when it can't be used as it
    // is: see OscDeviceProductInstanceId.
    struct OscDeviceShape
    {
        std::wstring Name{};
        std::wstring ProductInstanceId{};
        std::vector<OscDeviceBlock> Blocks{};

        bool operator==(OscDeviceShape const&) const = default;
    };

    OscDeviceShape OscDeviceShapeOf(
        _In_ std::wstring const& deviceId,
        _In_ std::wstring const& name,
        _In_ OscConnectionSettings const& settings);

    class OscVirtualDevice
    {
    public:
        // Called on the service's thread with each message an app sends. Must not block.
        using Handler = std::function<void(_In_ uint64_t timestamp, _In_reads_(wordCount) uint32_t const* words, _In_ uint32_t wordCount)>;

        // Makes the device. Blocks on the service, so never on the UI thread or under a lock.
        // Null when Windows MIDI Services can't make it. tryAgain is false when trying again could
        // leave another endpoint behind, as the service can't take away one that never opened.
        static std::unique_ptr<OscVirtualDevice> Create(_In_ OscDeviceShape const& shape, _In_ Handler handler, _Out_ bool& tryAgain) noexcept;

        // Takes the device away. Waits for a message being handed to the handler, and none is
        // after this.
        virtual ~OscVirtualDevice() noexcept = default;

        // To the apps that opened it. Whole UMPs only.
        virtual void Send(_In_ uint64_t timestamp, _In_reads_(wordCount) uint32_t const* words, _In_ uint32_t wordCount) noexcept = 0;
    };
}
