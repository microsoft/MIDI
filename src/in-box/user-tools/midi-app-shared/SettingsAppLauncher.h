// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

namespace midiapp
{
    enum class SettingsAppRequestResult
    {
        Shown,

        // Nothing to report: the customer closed the elevation prompt, which is an answer.
        Declined,

        NotInstalled,
        Failed
    };

    // True when the MIDI Settings app can be found, either installed or in a development build.
    bool IsSettingsAppAvailable() noexcept;

    // Opens the MIDI Settings notifications dialog.
    //
    // A copy which is already running is asked to show it, rather than putting the customer
    // through an elevation prompt to be handed the window they already had; that copy cannot be
    // replaced by an elevated one anyway, because it holds the single instance mutex. Otherwise
    // MIDI Settings is started with administrator rights, since starting the notifications app
    // for everyone who uses the PC is a machine wide change.
    SettingsAppRequestResult ShowSettingsNotifications() noexcept;
}
