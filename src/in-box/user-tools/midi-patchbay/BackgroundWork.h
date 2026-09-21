// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

namespace midipatchbay
{
    // Runs the callable on the thread pool and then puts the caller back on the thread that
    // asked for the work, so a continuation which touches XAML is always on the UI thread.
    // The MIDI session and connection calls block on the service over RPC and must never be
    // made from the XAML thread.
    winrt::Windows::Foundation::IAsyncAction RunOnBackgroundAsync(std::function<void()> work) noexcept;
}
