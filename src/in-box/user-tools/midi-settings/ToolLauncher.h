// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

namespace midisettings
{
    // The other Windows MIDI Services apps this one launches. Order is the order they appear
    // on the toolbar.
    enum class MidiTool : uint32_t
    {
        LoopbackSetup = 0,
        BluetoothSetup,
        NetworkSetup,
        SysEx,
        ScratchPad,
        Keyboard,
        Troubleshooter,
        Monitor
    };

    struct MidiToolLocation
    {
        MidiTool Tool{ MidiTool::LoopbackSetup };
        std::wstring FullPath{};
        bool Installed{ false };
    };

    // Re-resolves every tool. Called when the window loads and again when the toolbar is
    // clicked, so installing a transport package while this app is open does not require a
    // restart to see its setup app appear.
    void RefreshToolLocations() noexcept;

    MidiToolLocation const& GetToolLocation(MidiTool tool) noexcept;

    // Starts the tool with no arguments. False when it is not installed or would not start.
    bool LaunchTool(MidiTool tool) noexcept;

    // Opens the monitor on an endpoint, falling back to the console tool when the monitor app
    // is not installed. The console is opened in a terminal window because it is interactive.
    bool LaunchMonitorForEndpoint(winrt::hstring const& endpointDeviceId) noexcept;

    bool IsMonitoringAvailable() noexcept;

    // %ProgramFiles%\Windows MIDI Services\Tools\Console\midi.exe, or the HKLM override.
    std::wstring GetMidiConsolePath() noexcept;
}
