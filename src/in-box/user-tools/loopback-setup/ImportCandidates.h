// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

namespace midiloopbacksetup
{
    // One MIDI 1.0 source and destination which share a group, and so behave as a single port
    // to an older application. A loopback names both sides identically; a device which does not
    // is almost certainly not a loopback, which is why both names are kept.
    struct ImportCandidatePort
    {
        std::wstring SourceName{};
        std::wstring DestinationName{};

        // 1-based, as an application would see it
        uint8_t GroupNumber{ 1 };

        std::wstring EndpointDeviceId{};

        bool NamesMatch() const noexcept { return SourceName == DestinationName; }
    };

    // A device whose driver did not come with Windows, together with the ports it provides.
    // Grouped by the parent device rather than by endpoint because a device with more than
    // sixteen ports is split across several endpoints, and that split is an implementation
    // detail nobody chose.
    struct ImportCandidateDevice
    {
        std::wstring DeviceName{};
        std::wstring DeviceInstanceId{};
        std::wstring ServiceName{};

        std::vector<ImportCandidatePort> Ports{};
    };

    // Every MIDI 1.0 port pair on this PC whose driver is not one of the Windows in-box ones,
    // grouped by the device providing it. Never throws: an enumeration failure returns an empty
    // list, which the dialog reports as nothing to import.
    std::vector<ImportCandidateDevice> FindImportCandidates() noexcept;
}
