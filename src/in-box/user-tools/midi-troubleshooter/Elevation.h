// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

namespace miditroubleshooter
{
    // Nearly every repair on these pages writes HKLM or drives the service control manager, so
    // the app asks for elevation once at startup instead of failing an action at a time.
    bool IsProcessElevated() noexcept;

    // Launches another copy of this executable through the shell "runas" verb. Returns false
    // when the customer declines the prompt or the launch fails, in which case the caller
    // should carry on unelevated rather than exiting.
    bool TryRelaunchElevated(std::wstring const& extraArgument) noexcept;
}
