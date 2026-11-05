// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

namespace midiapp
{
    enum class MidiServiceAvailability
    {
        Available,

        // The PC is set to Legacy API mode, so the service exits as soon as it starts.
        LegacyApiMode,

        Unavailable,
    };

    // Legacy API mode is answered from the registry. Otherwise this asks the service, which
    // starts it when it is stopped, so it blocks: never call it on the UI thread. Never throws.
    MidiServiceAvailability CheckMidiServiceAvailability() noexcept;

    // Captions come from the calling app's own resources, as with AssistantPromptStrings.
    struct ServiceUnavailableStrings
    {
        winrt::hstring Title{};
        winrt::hstring LegacyApiModeMessage{};
        winrt::hstring UnavailableMessage{};
        winrt::hstring CloseButton{};
    };

    // Checks on a background thread and comes back to the calling thread, which has to be the
    // UI thread of the window that owns root. When the service can't be used, explains why in a
    // dialog, once per process, and completes when the customer closes it. True when the service
    // is available. Never throws: a failure to check counts as unavailable.
    winrt::Windows::Foundation::IAsyncOperation<bool> EnsureMidiServiceOrExplainAsync(
        _In_ winrt::Microsoft::UI::Xaml::XamlRoot root,
        _In_ ServiceUnavailableStrings strings);
}
