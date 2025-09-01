// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using Microsoft.Midi.Settings.Contracts.Services;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Midi.Settings.Services;

public class MidiConsoleToolsService : IMidiConsoleToolsService
{
    public bool OpenMidiConsole()
    {
        try
        {
            string arguments =
                " new-tab --title \"Windows MIDI Services Console\"" +
                " cmd /k midi.exe";

            var consoleProcess = new System.Diagnostics.Process();

            consoleProcess.StartInfo.FileName = "wt";
            consoleProcess.StartInfo.Arguments = arguments;
            consoleProcess.StartInfo.UseShellExecute = false;
            
            return consoleProcess.Start();
        }
        catch (Exception)
        {
            return false;
        }
    }



    public bool MonitorEndpoint(string endpointDeviceId)
    {
        try
        {
            string arguments =
                " new-tab --title \"Windows MIDI Services Console\"" +
                $" cmd /k midi.exe endpoint {endpointDeviceId} monitor";

            var consoleProcess = new System.Diagnostics.Process();

            consoleProcess.StartInfo.FileName = "wt";
            consoleProcess.StartInfo.Arguments = arguments;
            consoleProcess.StartInfo.UseShellExecute = false;

            return consoleProcess.Start();
        }
        catch (Exception)
        {
            return false;
        }
    }



    public bool RestartMidiService()
    {
        try
        {
            string arguments = "service restart";

            var consoleProcess = new System.Diagnostics.Process();

            consoleProcess.StartInfo.Verb = "runas";
            consoleProcess.StartInfo.FileName = "midi";
            consoleProcess.StartInfo.Arguments = arguments;
            consoleProcess.StartInfo.UseShellExecute = true;

            return consoleProcess.Start();
        }
        catch (Exception)
        {
            return false;
        }
    }

}
