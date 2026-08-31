// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// Picks a UDP port for a Network MIDI 2.0 host to keep, and tests whether a port is free.
// Shared by the service transport and the Windows.Devices.Midi2 SDK.
//
// Declarations only. The implementation needs winsock2.h, which must precede windows.h, and
// both consumers include windows.h from their precompiled header.
// ============================================================================

#pragma once

#include <cstdint>
#include <sal.h>

namespace WindowsMidiServicesInternal
{
    // Generated host ports come from the IANA registered range, deliberately BELOW the Windows
    // dynamic range. Measured on Windows 11 25H2 the UDP dynamic range starts at 49152, and
    // that is both where Windows hands out transient ports to every other process and where
    // Hyper-V and WinNAT place their reservation ranges, which move across reboots. A port
    // stored in configuration and expected to survive a reboot cannot live there.
    //
    // The band also avoids what else tends to be on a machine running audio software:
    // RTP-MIDI and AppleMIDI 5004-5006, AES67 and RAVENNA 5004, PTP 319 and 320,
    // Dante 4440-4455, 8700-8708 and 14336-14591, mDNS 5353, Cisco RTP 16384-16482,
    // Dropbox LAN sync 17500, and Steam 27015 upwards.
    constexpr uint16_t MidiNetworkGeneratedPortRangeLow = 40000;
    constexpr uint16_t MidiNetworkGeneratedPortRangeHigh = 48999;

    // True if nothing on this machine currently holds the port.
    //
    // A port held by a service running as LocalSystem reports WSAEACCES rather than
    // WSAEADDRINUSE when a normal user process tries to bind it. Both mean taken. Treating only
    // WSAEADDRINUSE as taken hands out ports this very service is already using.
    bool IsUdpPortAvailable(_In_ uint16_t const port);

    // Picks a free port at random from the generated range. False if it could not find one,
    // which in practice means something is very wrong with the machine's networking.
    bool TryGenerateAvailableHostPort(_Out_ uint16_t& port);
}
