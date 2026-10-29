// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Finding PCs that are ready to take a device link connection, and saying this PC is ready, by
// DNS-SD (mDNS). Uses the same browser, registration and announcement workaround as the Network
// MIDI 2.0 and RTP-MIDI transports and Windows MIDI Patchbay's OSC connections, in Inc.
//
// Advertising is meant to last only while a person has a receive screen open, so the advertiser
// is an object with that lifetime, not a service. What's advertised is public on the network:
// the PC's name, the port, and the SHA-256 of its device key, which lets a sender show whether
// it's a PC it's paired with. A record is only a hint: the key is checked again during the TLS
// handshake before anything is sent. Pch-free.

#include <sal.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>
#include <thread>
#include <vector>

#include "DeviceLinkIdentity.h"

namespace midiapp
{
    struct DeviceLinkFoundDevice
    {
        // Already cleaned up for showing.
        std::wstring Name{};

        // An IPv4 address when it has one, or its .local name.
        std::wstring Host{};
        uint16_t Port{ 0 };

        // What it says its key is. Only a hint until the handshake proves it.
        std::optional<DeviceKeyHash> Key{};
    };

    class DeviceLinkBrowser
    {
    public:
        DeviceLinkBrowser() noexcept;
        ~DeviceLinkBrowser();

        DeviceLinkBrowser(DeviceLinkBrowser const&) = delete;
        DeviceLinkBrowser& operator=(DeviceLinkBrowser const&) = delete;

        // Like L"_midiglass._tcp". The list fills over the next few seconds.
        HRESULT Start(_In_ std::wstring const& serviceType) noexcept;

        // Sorted by name, leaving out any that advertise `ownKey`.
        std::vector<DeviceLinkFoundDevice> Found(_In_opt_ DeviceKeyHash const* ownKey) const noexcept;

        void Stop() noexcept;

    private:
        struct State;
        std::unique_ptr<State> m_state;
    };

    class DeviceLinkAdvertiser
    {
    public:
        DeviceLinkAdvertiser() noexcept = default;
        ~DeviceLinkAdvertiser();

        DeviceLinkAdvertiser(DeviceLinkAdvertiser const&) = delete;
        DeviceLinkAdvertiser& operator=(DeviceLinkAdvertiser const&) = delete;

        // Returns at once: registering takes a second or more, so a thread of its own does it.
        void Start(
            _In_ std::wstring const& label,
            _In_ std::wstring const& serviceType,
            _In_ uint16_t port,
            _In_ std::wstring const& deviceName,
            _In_ DeviceKeyHash const& key) noexcept;

        // Withdraws the advertisement and waits for that to go out.
        void Stop() noexcept;

    private:
        std::jthread m_worker{};
    };
}
