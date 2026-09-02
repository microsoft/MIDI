// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

namespace midisettings
{
    // Runs the callable on the thread pool. Awaiting the returned action from the UI thread
    // restores the apartment context, so the caller carries on back on the UI thread with no
    // marshaling of its own. Everything this app asks of the MIDI service blocks on an RPC
    // call, and none of that may happen on the XAML thread.
    winrt::Windows::Foundation::IAsyncAction RunOnBackgroundAsync(std::function<void()> work) noexcept;
}
