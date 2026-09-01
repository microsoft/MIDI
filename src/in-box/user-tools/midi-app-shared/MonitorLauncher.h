// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

namespace midiapp
{
    // True when the MIDI monitor app can be found, either installed or in a development build.
    bool IsMonitorAvailable() noexcept;

    // Opens the MIDI monitor watching one endpoint. False when the monitor could not be found
    // or would not start.
    bool LaunchMonitorForEndpoint(_In_ winrt::hstring const& endpointDeviceId) noexcept;
}
