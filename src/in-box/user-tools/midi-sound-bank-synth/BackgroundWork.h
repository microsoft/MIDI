// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

namespace midisoundbanksynth
{
    // Runs the callable on the thread pool and then puts the caller back on the thread that
    // asked for the work, so a continuation which touches XAML is always on the UI thread.
    winrt::Windows::Foundation::IAsyncAction RunOnBackgroundAsync(_In_ std::function<void()> work) noexcept;
}
