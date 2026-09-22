// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

namespace miditroubleshooter
{
    // Runs the callable on the thread pool and then puts the caller back on the thread that
    // asked for the work, so a continuation which touches XAML is always on the UI thread.
    // Everything this tool talks to blocks - the MIDI service over RPC, the service control
    // manager, setup API and the console tools - and none of it may be called from the XAML
    // thread.
    winrt::Windows::Foundation::IAsyncAction RunOnBackgroundAsync(std::function<void()> work) noexcept;
}
