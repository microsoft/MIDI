// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// The network adapters a host can be limited to. Shared by the Network MIDI 2.0 and RTP-MIDI
// transports, the DNS-SD browser, the Windows.Devices.Midi2 SDK and the setup app.
//
// An adapter is found by its interface GUID first, then by its hardware address. A USB network
// adapter plugged into a different port can come back as a new interface with a new GUID, and
// its hardware address is what stays the same.
//
// Declarations only, for the same reason as midi_network_port_picker.h: the implementation
// needs winsock2.h ahead of windows.h.
// ============================================================================

#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include <guiddef.h>
#include <sal.h>

namespace WindowsMidiServicesInternal
{
    struct MidiNetworkAdapterInfo
    {
        GUID Id{};

        // "Ethernet 3", the name Windows Settings shows
        std::wstring Name;

        // "ASIX AX88179 USB 3.0 to Gigabit Ethernet Adapter"
        std::wstring Description;

        // "60-CF-84-A4-A8-5A", or empty when the adapter has none
        std::wstring PhysicalAddress;

        // zero when the adapter does not carry that IP version
        uint32_t IPv4InterfaceIndex{ 0 };
        uint32_t IPv6InterfaceIndex{ 0 };

        // When two adapters reach the same place, Windows prefers the one with the lower metric
        uint32_t Metric{ UINT32_MAX };

        bool IsUp{ false };
        bool SupportsMulticast{ false };

        // Link-local IPv6 addresses carry their %scope, so they can be used as they are
        std::vector<std::wstring> IPv4Addresses;
        std::vector<std::wstring> IPv6Addresses;

        // Up, and with an address something can reach it on
        bool IsUsable() const noexcept { return IsUp && (!IPv4Addresses.empty() || !IPv6Addresses.empty()); }

        // The index DNS-SD registration and browsing take. The two are the same on current Windows.
        uint32_t InterfaceIndex() const noexcept { return IPv4InterfaceIndex != 0 ? IPv4InterfaceIndex : IPv6InterfaceIndex; }
    };

    // Every adapter but loopback and tunnels, including ones which are down. Empty on failure.
    std::vector<MidiNetworkAdapterInfo> GetMidiNetworkAdapters() noexcept;

    // The adapter with this id, whether or not it is up
    bool TryGetMidiNetworkAdapter(_In_ GUID const& id, _Out_ MidiNetworkAdapterInfo& found) noexcept;

    // The adapter configured by id and hardware address, when it is usable. The hardware address
    // is only tried when no adapter has the id at all, so an adapter which is merely down is
    // reported missing rather than swapped for another. False when it is missing, down, or has no
    // address yet.
    bool TryFindUsableMidiNetworkAdapter(
        _In_ std::vector<MidiNetworkAdapterInfo> const& adapters,
        _In_ GUID const& id,
        _In_ std::wstring const& physicalAddress,
        _Out_ MidiNetworkAdapterInfo& found);

    // "{1A946373-4577-440A-BBB9-493FDF6C2A26}", or empty for GUID_NULL
    std::wstring MidiNetworkAdapterIdToString(_In_ GUID const& id);

    // The form above, with or without braces. Empty text is GUID_NULL, which means every adapter.
    bool TryParseMidiNetworkAdapterId(_In_ std::wstring const& text, _Out_ GUID& id) noexcept;


    // Calls back once an adapter gains or loses an address and things have settled. A Wi-Fi
    // connection or a USB adapter coming up changes several addresses in quick succession, and
    // they are reported as one change.
    class MidiNetworkChangeMonitor
    {
    public:
        using ChangedHandler = std::function<void()>;

        MidiNetworkChangeMonitor() = default;
        ~MidiNetworkChangeMonitor() { Stop(); }

        MidiNetworkChangeMonitor(_In_ MidiNetworkChangeMonitor const&) = delete;
        MidiNetworkChangeMonitor& operator=(_In_ MidiNetworkChangeMonitor const&) = delete;

        // onChanged runs on a thread pool thread, settleMilliseconds after the last change in a
        // burst. False when Windows would not register the notification.
        bool Start(_In_ ChangedHandler onChanged, _In_ uint32_t const settleMilliseconds) noexcept;

        // Once this returns the handler is not running, and it will not run again
        void Stop() noexcept;

    private:
        friend struct MidiNetworkChangeMonitorCallbacks;

        ChangedHandler m_onChanged;
        uint32_t m_settleMilliseconds{ 0 };

        // a HANDLE and a PTP_TIMER, kept opaque so this header does not need windows.h
        void* m_notification{ nullptr };
        void* m_timer{ nullptr };
    };
}
