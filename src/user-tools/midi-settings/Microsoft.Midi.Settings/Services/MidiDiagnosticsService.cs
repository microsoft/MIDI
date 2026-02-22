using Microsoft.Midi.Settings.Contracts.Services;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Midi.Settings.Services;

public class MidiDiagnosticsService : IMidiDiagnosticsService
{
    private const string MidiRootRegKey = @"HKEY_LOCAL_MACHINE\Software\Microsoft\Windows MIDI Services";
    private const string MidiSdkRootRegKey = MidiRootRegKey + @"\Desktop App SDK Runtime";
    private const string MidiDiagPathRegValue = "MidiDiag";

    private static string GetMidiDiagPath()
    {
        try
        {
            string defaultPath = Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles) + @"\Windows MIDI Services\Tools\mididiag.exe";

            string diagPath = string.Empty;

            var value = Microsoft.Win32.Registry.GetValue(MidiSdkRootRegKey, MidiDiagPathRegValue, defaultPath);

            if (value == null)
            {
                // happens when the key does not exist
                diagPath = defaultPath;
            }
            else if (value is string)
            {
                diagPath = (string)value;
            }
            else
            {
                diagPath = defaultPath;
            }

            return diagPath;
        }
        catch (Exception ex)
        {
            App.GetService<ILoggingService>().LogError($"Exception getting MIDI Diagnostics tool Path", ex);

            return string.Empty;
        }
    }


    public bool IsMidiDiagPresent()
    {
        var path = GetMidiDiagPath();

        if (string.IsNullOrEmpty(path))
        {
            App.GetService<ILoggingService>().LogError($"MIDI Diagnostics tool path is blank.");

            return false;
        }

        if (!File.Exists(path))
        {
            App.GetService<ILoggingService>().LogError($"MIDI Diagnostics tool does not exist in path '{path}' ");

            return false;
        }

        return true;
    }


    public bool CaptureMidiDiagOutputToNotepad()
    {
        try
        {
            if (IsMidiDiagPresent())
            {
                string diagPath = GetMidiDiagPath();

                string mididiagTempFileName = Path.GetTempFileName();

                using (var consoleProcess = new System.Diagnostics.Process())
                {
                    consoleProcess.StartInfo.FileName = diagPath;
                    consoleProcess.StartInfo.RedirectStandardOutput = true;
                    consoleProcess.Start();
                    //consoleProcess.WaitForExit();

                    var output = consoleProcess.StandardOutput.ReadToEnd();

                    var fileStream = File.OpenWrite(mididiagTempFileName);

                    using (StreamWriter writer = new StreamWriter(fileStream))
                    {
                        writer.WriteLine(output);
                    }
                }

                var notepadProcess = new System.Diagnostics.Process();
                notepadProcess.StartInfo.FileName = "notepad.exe";
                notepadProcess.StartInfo.Arguments = mididiagTempFileName;

                return notepadProcess.Start();
            }
            else
            {
                App.GetService<ILoggingService>().LogError("Unable to find mididiag utility.");

                return false; 
            }

        }
        catch (Exception ex)
        {
            App.GetService<ILoggingService>().LogError("Error capturing mididiag.exe output and opening in notepad.", ex);

            return false;
        }
    }

}
