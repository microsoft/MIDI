// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Self-contained, with no precompiled header, so the unit tests compile it exactly as it ships.

#include <sal.h>

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Devices.Midi2.h>
#include <winrt/Windows.Devices.Midi2.Enumeration.h>
#include <winrt/Windows.Devices.Midi2.ServiceConfig.h>

namespace midiapp::customizations
{
    namespace midi2 = ::winrt::Windows::Devices::Midi2;
    namespace midi2enum = ::winrt::Windows::Devices::Midi2::Enumeration;
    namespace midi2config = ::winrt::Windows::Devices::Midi2::ServiceConfig;

    // What the service applies to an endpoint. A save resets whatever it leaves out, so a dialog
    // sends all of this back, including what it does not show.
    struct EndpointSettings
    {
        winrt::hstring EndpointDeviceId{};

        winrt::hstring Name{};
        winrt::hstring Description{};
        winrt::hstring ImageFileName{};

        bool RequiresNoteOffTranslation{ false };
        bool SupportsMidiPolyphonicExpression{ false };
        uint16_t RecommendedControlChangeIntervalMilliseconds{ 0 };
        int64_t OutgoingLatencyTicks{ 0 };

        midi2enum::Midi1PortNamingApproach Midi1PortNamingApproach{ midi2enum::Midi1PortNamingApproach::Default };

        // custom MIDI 1.0 port names by group index
        std::map<uint8_t, winrt::hstring> SourcePortNames{};
        std::map<uint8_t, winrt::hstring> DestinationPortNames{};
    };

    // A MIDI 1.0 port whose custom name a save emptied, and the name it had.
    struct ClearedPortName
    {
        midi2enum::Midi1PortFlow Flow{ midi2enum::Midi1PortFlow::MidiMessageSource };
        uint8_t GroupIndex{ 0 };
        winrt::hstring Name{};
    };

    // Until it restarts, a service without the port name fix keeps showing a cleared name, and the
    // next save would read it back and store it again. Kept between runs for the same reason.
    class ClearedPortNames
    {
    public:
        // Forgets the ports the save named, and remembers the ones it cleared.
        void Saved(
            _In_ EndpointSettings const& saved,
            _In_ std::vector<ClearedPortName> const& cleared) noexcept;

        // Leaves out the names this app cleared, and forgets those the endpoint no longer shows.
        void RemoveFrom(_Inout_ EndpointSettings& settings) noexcept;

        std::wstring Serialize() const noexcept;
        void Load(_In_ std::wstring const& text) noexcept;

        size_t Count() const noexcept { return m_entries.size(); }

    private:
        struct Entry
        {
            winrt::hstring EndpointDeviceId{};
            ClearedPortName Port{};
        };

        void TrimToMaximum() noexcept;

        std::vector<Entry> m_entries{};
    };

    // A row of the port names dialog: what the box holds, and the custom name it opened with.
    struct PortNameRow
    {
        uint8_t GroupIndex{ 0 };
        winrt::hstring Text{};
        winrt::hstring StoredName{};
    };

    // Every named port, not just the changed ones: the saved list replaces the stored one, so a
    // port left out of it loses its name. A port whose box was emptied is added to the cleared list.
    void CollectPortNames(
        _In_ std::vector<PortNameRow> const& rows,
        _In_ midi2enum::Midi1PortFlow const flow,
        _Out_ std::map<uint8_t, winrt::hstring>& names,
        _Inout_ std::vector<ClearedPortName>& cleared) noexcept;

    // KS and KSA create a MIDI 1.0 port per group, so theirs are the port names this app edits.
    bool HasEditablePortNames(
        _In_ midi2enum::MidiEndpointDeviceInformation const& endpoint) noexcept;

    EndpointSettings ReadEndpointSettings(
        _In_ midi2enum::MidiEndpointDeviceInformation const& endpoint,
        _Inout_ ClearedPortNames& clearedPortNames) noexcept;

    bool HasNonDisplayContent(
        _In_ EndpointSettings const& settings) noexcept;

    // An empty name, description or image is left out. A caller which owns those sets them after.
    void CopyInto(
        _In_ EndpointSettings const& source,
        _In_ midi2config::MidiServiceEndpointCustomizationConfig const& destination) noexcept;

    // Sent empty, so the stored list is still replaced when no named port is left.
    void AddClearedPortNames(
        _In_ std::vector<ClearedPortName> const& cleared,
        _In_ midi2config::MidiServiceEndpointCustomizationConfig const& destination) noexcept;
}
