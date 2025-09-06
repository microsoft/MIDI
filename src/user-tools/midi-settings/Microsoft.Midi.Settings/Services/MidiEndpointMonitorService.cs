// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using Microsoft.Midi.Settings.Contracts.Services;

using Windows.UI.Popups;

namespace Microsoft.Midi.Settings.Services;

public class MidiEndpointMonitorService : IMidiEndpointMonitorService
{
    public void MonitorMidiEndpoint(string midiEndpointDeviceId)
    {
        // right now, this opens a console. In the future, it may be its own dedicated GUI window

        try
        {
            string arguments =
                " endpoint " +
                midiEndpointDeviceId +
                " monitor";

            using (var monitorProcess = new System.Diagnostics.Process())
            {
                monitorProcess.StartInfo.FileName = "midi.exe";
                monitorProcess.StartInfo.Arguments = arguments;

                monitorProcess.Start();
            }
        }
        catch (Exception ex)
        {
            App.GetService<ILoggingService>().LogError($"Error launching console monitor for endpoint '{midiEndpointDeviceId}'", ex);
        }
    }
}
