// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

namespace miditroubleshooter
{
    // Runs the callable on the thread pool. Awaiting the returned action from the UI thread
    // restores the apartment context, so the caller carries on back on the UI thread with no
    // marshalling of its own. Everything this tool talks to blocks - the MIDI service over
    // RPC, the service control manager, setup API and the console tools - and none of it may
    // be called from the XAML thread.
    winrt::Windows::Foundation::IAsyncAction RunOnBackgroundAsync(std::function<void()> work) noexcept;
}
