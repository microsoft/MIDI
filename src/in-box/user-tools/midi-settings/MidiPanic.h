// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

namespace midisettings
{
    // Sends All Sound Off, Reset All Controllers and All Notes Off on every channel of the
    // given groups, one group per send. Blocking: it opens a connection to the endpoint, so it
    // must be called from a background thread, never from the XAML thread.
    //
    // The session is created once and reused. Nothing here throws; failure comes back as false
    // with the reason in errorMessage.
    bool SendMidiPanic(
        winrt::hstring const& endpointDeviceId,
        std::vector<uint8_t> const& groupIndexes,
        std::wstring& errorMessage) noexcept;

    // Drops the shared session. Called when the window closes so the service sees the client
    // go away cleanly.
    void ShutDownPanicSession() noexcept;
}
