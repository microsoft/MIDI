// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.ServiceProcess;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Midi.Settings.Helpers
{
    public class MidiFeatureDetectionHelper
    {
        private const string MidiRootRegKey = @"HKEY_LOCAL_MACHINE\Software\Microsoft\Windows MIDI Services";
        private const string MidiSdkRootRegKey = MidiRootRegKey + @"\Desktop App SDK Runtime";
        private const string MidiCheckServicePathRegValue = "MidiCheckService";

        public static async Task<bool> IsWindowsMidiServicesFeatureEnabledAsync()
        {
            return await Task.Run(() =>
            {
                try
                {
                    string defaultPath = Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles) + @"\Windows MIDI Services\Tools\midicheckservice.exe";

                    string checkServicePath = string.Empty;

                    var value = Microsoft.Win32.Registry.GetValue(MidiSdkRootRegKey, MidiCheckServicePathRegValue, defaultPath);

                    if (value == null)
                    {
                        // happens when the key does not exist
                        checkServicePath = defaultPath;
                    }
                    else if (value is string)
                    {
                        checkServicePath = (string)value;
                    }
                    else
                    {
                        checkServicePath = defaultPath;
                    }

                    var consoleProcess = new System.Diagnostics.Process();

                    consoleProcess.StartInfo.FileName = checkServicePath;
                    consoleProcess.StartInfo.Arguments = "--quiet";
                    consoleProcess.StartInfo.UseShellExecute = false;
                    consoleProcess.StartInfo.CreateNoWindow = true;

                    consoleProcess.Start();
                    consoleProcess.WaitForExit();

                    var exitCode = consoleProcess.ExitCode;

                    if (exitCode <= 0)
                    {
                        return true;
                    }

                }
                catch (Exception)
                {
                }

                return false;
            });
        }



    }
}
