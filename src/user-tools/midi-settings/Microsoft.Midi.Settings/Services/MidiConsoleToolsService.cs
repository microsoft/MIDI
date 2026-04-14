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
    private const string MidiRootRegKey = @"HKEY_LOCAL_MACHINE\Software\Microsoft\Windows MIDI Services";
    private const string MidiSdkRootRegKey = MidiRootRegKey + @"\Desktop App SDK Runtime";
    private const string MidiConsolePathRegValue = "MidiConsole";

    private static string GetMidiConsolePath()
    {
        App.GetService<ILoggingService>().LogInfo($"Enter");

        try
        {
            string defaultPath = Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles) + @"\Windows MIDI Services\Tools\Console\midi.exe";

            string consolePath = string.Empty;

            var value = Microsoft.Win32.Registry.GetValue(MidiSdkRootRegKey, MidiConsolePathRegValue, defaultPath);

            if (value == null)
            {
                // happens when the key does not exist
                consolePath = defaultPath;
            }
            else if (value is string)
            {
                consolePath = (string)value;
            }
            else
            {
                consolePath = defaultPath;
            }

            return consolePath;
        }
        catch (Exception ex)
        {
            App.GetService<ILoggingService>().LogError($"Exception getting MIDI Console Path", ex);

            return string.Empty;
        }
    }


    public bool IsMidiConsolePresent()
    {
        App.GetService<ILoggingService>().LogInfo($"Enter");

        var path = GetMidiConsolePath();

        if (string.IsNullOrEmpty(path))
        {
            App.GetService<ILoggingService>().LogError($"MIDI Console path is blank.");

            return false;
        }

        if (!File.Exists(path))
        {
            App.GetService<ILoggingService>().LogError($"MIDI Console does not exist in path '{path}' ");

            return false;
        }

        return true;
    }

    public bool OpenMidiConsole()
    {
        App.GetService<ILoggingService>().LogInfo($"Enter");

        try
        {
            if (IsMidiConsolePresent())
            {
                var consoleExe = GetMidiConsolePath();

                string arguments =
                    " new-tab --title \"Windows MIDI Services Console\"" +
                    $" cmd /k \"{consoleExe}\"";

                var consoleProcess = new System.Diagnostics.Process();

                consoleProcess.StartInfo.FileName = "wt";
                consoleProcess.StartInfo.Arguments = arguments;
                consoleProcess.StartInfo.UseShellExecute = false;

                return consoleProcess.Start();
            }
            else
            {
                return false; 
            }
        }
        catch (Exception ex)
        {
            App.GetService<ILoggingService>().LogError("Error opening MIDI console", ex);

            return false;
        }
    }



    public bool MonitorEndpoint(string endpointDeviceId)
    {
        App.GetService<ILoggingService>().LogInfo($"Enter");

        try
        {
            if (IsMidiConsolePresent())
            {
                var consoleExe = GetMidiConsolePath();

                string arguments =
                    " new-tab --title \"Windows MIDI Services Console\"" +
                    $" cmd /k \"{consoleExe}\" endpoint {endpointDeviceId} monitor";

                var consoleProcess = new System.Diagnostics.Process();

                consoleProcess.StartInfo.FileName = "wt";
                consoleProcess.StartInfo.Arguments = arguments;
                consoleProcess.StartInfo.UseShellExecute = false;

                return consoleProcess.Start();
            }
            else
            {
                return false;
            }
        }
        catch (Exception ex)
        {
            App.GetService<ILoggingService>().LogError("Error monitoring endpoint", ex);

            return false;
        }
    }



    public bool RestartMidiService()
    {
        App.GetService<ILoggingService>().LogInfo($"Enter");

        try
        {
            if (IsMidiConsolePresent())
            {
                var consoleExe = GetMidiConsolePath();

                string arguments = "service restart";

                var consoleProcess = new System.Diagnostics.Process();

                consoleProcess.StartInfo.Verb = "runas";
                consoleProcess.StartInfo.FileName = consoleExe;
                consoleProcess.StartInfo.Arguments = arguments;
                consoleProcess.StartInfo.UseShellExecute = true;

                return consoleProcess.Start();
            }
            else
            {
                return false; 
            }
        }
        catch (Exception ex)
        {
            App.GetService<ILoggingService>().LogError("Error restarting MIDI service", ex);

            return false;
        }
    }

    public bool OpenExplorerWithFile(string filePath)
    {
        App.GetService<ILoggingService>().LogInfo($"Enter");

        try
        {
            string arguments = $"/select,\"{filePath}\"";

            var consoleProcess = new System.Diagnostics.Process();

            consoleProcess.StartInfo.FileName = "explorer.exe";
            consoleProcess.StartInfo.Arguments = arguments;
            consoleProcess.StartInfo.UseShellExecute = true;

            return consoleProcess.Start();
        }
        catch (Exception ex)
        {
            App.GetService<ILoggingService>().LogError("Error restarting MIDI service", ex);

            return false;
        }
    }

}
