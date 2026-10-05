// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

namespace midisoundfontsynth
{
    enum class StartupState
    {
        Off,
        On,

        // Turned off in Task Manager or Settings. Only the customer can turn it back on, there.
        OffByUser,

        // A policy on this PC decides, so the app cannot change it.
        ControlledByPolicy,
    };

    // Starting with Windows works two ways. Installed from an MSIX, the app declares a startup
    // task and Windows owns the switch, which also shows in Task Manager. Run from a folder, it is
    // the per-user Run key. A packaged app cannot use the Run key: its registry writes go to a
    // private copy Windows never reads at sign-in.
    class StartupRegistration
    {
    public:
        static bool IsPackaged() noexcept;

        // UI thread. Both talk to Windows asynchronously when packaged.
        static winrt::Windows::Foundation::IAsyncOperation<int32_t> GetStateAsync();
        static winrt::Windows::Foundation::IAsyncOperation<int32_t> SetEnabledAsync(bool enable);

        // The task id in the package manifest.
        static constexpr wchar_t StartupTaskId[] = L"MidiSoundFontSynthStartup";
    };
}
