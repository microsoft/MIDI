// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using Microsoft.Midi.Settings.Models;

namespace Microsoft.Midi.Settings.Contracts.Services;

public enum MidiToolApp
{
    LoopbackSetup = 0,
    NetworkSetup,
    ScratchPad,
    SysEx,
    Monitor,
    Troubleshooter
}

public enum MidiMonitorTool
{
    MonitorApp = 0,
    MidiConsole = 1
}

public interface IMidiToolsService
{
    IReadOnlyList<MidiToolAppInfo> GetInstalledTools();

    bool IsToolPresent(MidiToolApp tool);

    bool LaunchTool(MidiToolApp tool, params string[] arguments);

    // true when at least one of the monitoring tools (app or console) is installed
    bool IsEndpointMonitoringAvailable();

    bool MonitorEndpoint(MidiEndpointDeviceInformation deviceInformation);
}
