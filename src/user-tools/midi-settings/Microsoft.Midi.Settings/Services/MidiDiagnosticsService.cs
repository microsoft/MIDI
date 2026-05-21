using Microsoft.Extensions.Logging.Console;
using Microsoft.Midi.Settings.Contracts.Services;
using Microsoft.Web.WebView2.Core;
using Microsoft.Win32;
using System;
using System.Collections.Generic;
using System.Data;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Midi.Settings.Services;





public class MidiDiagnosticsService : IMidiDiagnosticsService
{
    private const string MidiRootRegKey = @"HKEY_LOCAL_MACHINE\Software\Microsoft\Windows MIDI Services";
    private const string MidiSdkRootRegKey = MidiRootRegKey + @"\Desktop App SDK Runtime";
    private const string MidiDiagPathRegValue = "MidiDiag";
    private const string MidiFixRegPathRegValue = "MidiFixReg";
    private const string CollectMidiLogsPathRegValue = "CollectMidiLogs";



    private readonly ILoggingService _loggingService;
    public MidiDiagnosticsService(ILoggingService loggingService)
    {
        _loggingService = loggingService;
    }

    private string GetMidiDiagPath()
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
            _loggingService.LogError($"Exception getting MIDI Diagnostics tool Path", ex);

            return string.Empty;
        }
    }

    private string GetCollectMidiLogsPath()
    {
        _loggingService.LogInfo($"Enter");

        try
        {
            string defaultPath = Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles) + @"\Windows MIDI Services\Collect MIDI Logs\CollectMidiLogs.cmd";

            string toolPath = string.Empty;

            var value = Microsoft.Win32.Registry.GetValue(MidiSdkRootRegKey, CollectMidiLogsPathRegValue, defaultPath);

            if (value == null)
            {
                // happens when the key does not exist
                toolPath = defaultPath;
            }
            else if (value is string)
            {
                toolPath = (string)value;
            }
            else
            {
                toolPath = defaultPath;
            }

            return toolPath;
        }
        catch (Exception ex)
        {
            _loggingService.LogError($"Exception getting CollectMIDILogs Path", ex);

            return string.Empty;
        }
    }

    private string GetMidiFixRegPath()
    {
        _loggingService.LogInfo($"Enter");

        try
        {
            string defaultPath = Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles) + @"\Windows MIDI Services\Tools\midifixreg.exe";

            string fixRegPath = string.Empty;

            var value = Microsoft.Win32.Registry.GetValue(MidiSdkRootRegKey, MidiFixRegPathRegValue, defaultPath);

            if (value == null)
            {
                // happens when the key does not exist
                fixRegPath = defaultPath;
            }
            else if (value is string)
            {
                fixRegPath = (string)value;
            }
            else
            {
                fixRegPath = defaultPath;
            }

            return fixRegPath;
        }
        catch (Exception ex)
        {
            _loggingService.LogError($"Exception getting MIDI Registry Fixer tool Path", ex);

            return string.Empty;
        }
    }

    public bool IsCollectMidiLogsPresent()
    {
        _loggingService.LogInfo($"Enter");

        var path = GetCollectMidiLogsPath();

        if (string.IsNullOrEmpty(path))
        {
            _loggingService.LogError($"Collect MIDI Logs path is blank.");

            return false;
        }

        if (!File.Exists(path))
        {
            _loggingService.LogError($"Collect MIDI Logs script does not exist in path '{path}' ");

            return false;
        }

        return true;
    }

    public bool IsMidiDiagPresent()
    {
        _loggingService.LogInfo($"Enter");

        var path = GetMidiDiagPath();

        if (string.IsNullOrEmpty(path))
        {
            _loggingService.LogError($"MIDI Diagnostics tool path is blank.");

            return false;
        }

        if (!File.Exists(path))
        {
            _loggingService.LogError($"MIDI Diagnostics tool does not exist in path '{path}' ");

            return false;
        }

        return true;
    }


    public bool IsMidiFixRegPresent()
    {
        _loggingService.LogInfo($"Enter");

        var path = GetMidiFixRegPath();

        if (string.IsNullOrEmpty(path))
        {
            _loggingService.LogError($"MIDI Registry Fixer tool path is blank.");

            return false;
        }

        if (!File.Exists(path))
        {
            _loggingService.LogError($"MIDI Registry Fixer tool does not exist in path '{path}' ");

            return false;
        }

        return true;
    }

    public bool CaptureMidiDiagOutputToNotepad()
    {
        _loggingService.LogInfo($"Enter");

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
                _loggingService.LogError("Unable to find mididiag utility.");

                return false;
            }

        }
        catch (Exception ex)
        {
            _loggingService.LogError("Error capturing mididiag.exe output and opening in notepad.", ex);

            return false;
        }
    }


    const int MIDIFIXREG_RETURN_NO_CHANGES_NEEDED = -1;
    const int MIDIFIXREG_RETURN_SUCCESS = 0;
    const int MIDIFIXREG_RETURN_INSUFFICIENT_PERMISSIONS = 1;
    const int MIDIFIXREG_RETURN_USER_ABORTED = 2;
    const int MIDIFIXREG_RETURN_NO_MIDI_SERVICES = 3;


    public bool MidiFixReg()
    {
        _loggingService.LogInfo($"Enter");

        try
        {
            if (IsMidiFixRegPresent())
            {
                string diagPath = GetMidiFixRegPath();

                string mididiagTempFileName = Path.GetTempFileName();

                using (var consoleProcess = new System.Diagnostics.Process())
                {
                    consoleProcess.StartInfo.FileName = diagPath;
                    consoleProcess.StartInfo.Verb = "runas";
                    consoleProcess.StartInfo.UseShellExecute = true;

                    consoleProcess.Start();
                    consoleProcess.WaitForExit();

                    if (consoleProcess.ExitCode <= 0)
                    {
                        return true;
                    }
                    else
                    {
                        _loggingService.LogError($"Registry fix failed. midifixreg returned {consoleProcess.ExitCode}");
                    }
                }
            }
            else
            {
                _loggingService.LogError("Unable to find midifixreg utility.");
            }

        }
        catch (Exception ex)
        {
            _loggingService.LogError("Error fixing registry entries.", ex);
        }

        return false;
    }



    public bool RestoreInBoxComponentRegistrations()
    {
        _loggingService.LogInfo($"Enter");

        try
        {


        }
        catch (Exception ex)
        {
            _loggingService.LogError("Error restoring in-box component registrations.", ex);
        }

        return false;
    }


    public string CaptureMidiLogsToFile()
    {
        _loggingService.LogInfo($"Enter");

        try
        {
            if (IsCollectMidiLogsPresent())
            {
                string path = GetCollectMidiLogsPath();

                string outFileName = $"MIDI Capture {Environment.MachineName}_{DateTime.Now.ToString("yyyyMMdd-HHmmss")}.zip";
                string collectMidiLogsOutputFileName = Path.Combine(Path.GetTempPath(), outFileName);

                string arguments = "";
                arguments += " -ExecutionPolicy Bypass";
                arguments += " -NoProfile";
                //arguments += " -NoExit";
                arguments += $" -File \"{path}\" -specifiedOutputFile \"{collectMidiLogsOutputFileName}\" -automatic";

                using (var consoleProcess = new System.Diagnostics.Process())
                {
                    consoleProcess.StartInfo.FileName = "powershell.exe";
                    consoleProcess.StartInfo.Arguments = arguments;
                    consoleProcess.StartInfo.Verb = "runas";
                    consoleProcess.StartInfo.UseShellExecute = true;

                    if (consoleProcess.Start())
                    {
                        consoleProcess.WaitForExit();

                        if (consoleProcess.ExitCode <= 0)
                        {
                            return collectMidiLogsOutputFileName;
                        }
                        else
                        {
                            _loggingService.LogError($"Collect MIDI Logs returned error {consoleProcess.ExitCode}");
                        }
                    }
                    else
                    {
                        _loggingService.LogError($"Unable to start process to collect MIDI Logs");
                    }
                }
            }
            else
            {
                _loggingService.LogError("Unable to find CollectMIDILogs utility.");
            }

        }
        catch (Exception ex)
        {
            _loggingService.LogError("Error collecting MIDI logs.", ex);
        }

        return "";
    }


    private const string Drivers32Path = @"SOFTWARE\Microsoft\Windows NT\CurrentVersion\Drivers32";
    private const string Drivers32WOWPath = @"SOFTWARE\WOW6432Node\Microsoft\Windows NT\CurrentVersion\Drivers32";

    private List<FoundRegistryEntry> GetHklmRegistryEntries(string key, RegistryView view)
    {
        _loggingService.LogInfo($"Enter");

        List<FoundRegistryEntry> entries = new List<FoundRegistryEntry>();

        var root = RegistryKey.OpenBaseKey(RegistryHive.LocalMachine, view);

        var subKey = root.OpenSubKey(key);

        if (subKey != null)
        {
            var valueNames = subKey.GetValueNames();

            foreach (var valueName in valueNames)
            {
                if (valueName != null)
                {
                    if (valueName.ToLower().StartsWith("midi"))
                    {
                        FoundRegistryEntry entry = new FoundRegistryEntry();
                        entry.Name = valueName;

                        var value = subKey.GetValue(valueName);

                        if (value != null)
                        {
                            entry.Value = value.ToString()!;
                        }
                        else
                        {
                            entry.Value = "(null)";
                        }


                        if (entry.Name == "midi")
                        {
                            if (entry.Value.ToLower() == "wdmaud.drv")
                            {
                                entry.HasError = false;
                                entry.Comment = "In-box synth support";
                            }
                            else
                            {
                                entry.HasError = true;
                                entry.Comment = "Must be wdmaud.drv";
                            }

                        }
                        else if (entry.Name == "midi1")
                        {
                            entry.HasError = true;

                            if (entry.Value.ToLower() == "wdmaud2.drv")
                            {
                                entry.HasError = false;
                                entry.Comment = "MIDI Service interface";
                            }
                            else
                            {
                                entry.HasError = true;
                                entry.Comment = "Must be wdmaud2.drv";
                            }
                        }
                        else if (entry.Name == "midimapper" && entry.Value == "midimap.dll")
                        {
                            entry.HasError = false;
                            entry.Comment = "In-box MIDI Mapper";
                        }
                        else if (entry.Name == "MidisrvTransferComplete")
                        {
                            if (entry.Value == "1")
                            {
                                entry.HasError = false;
                                entry.Comment = "Required service flag";
                            }
                            else
                            {
                                entry.HasError = true;
                                entry.Comment = "Must be a REG_DWORD set to 1";
                            }
                        }
                        else if (entry.Name == "midi0")
                        {
                            entry.HasError = true;
                            entry.Comment = "Invalid value name";
                        }
                        else
                        {
                            string[] validEntries = { "midi2", "midi3", "midi4", "midi5", "midi6", "midi7", "midi8", "midi9", "midimapper", "MidisrvTransferComplete" };

                            if (!validEntries.Contains(entry.Name))
                            {
                                entry.HasError = true;
                                entry.Comment = "Unrecognized entry";
                            }
                            else
                            {
                                if (entry.Value.ToUpper() == "KORGUM64.DRV" || entry.Value.ToUpper() == "KORGUMDS.DRV" || entry.Value.ToUpper() == "KORGUMDD.DRV")
                                {
                                    entry.HasError = true;
                                    entry.Comment = "Deprecated KORG USB Driver";
                                }

                                if (entry.Value.ToUpper() == "KORGBM64.DRV")
                                {
                                    entry.HasError = false;
                                    entry.Comment = "KORG BLE MIDI";
                                }

                                if (entry.Value.ToUpper() == "VIRTUALMIDISYNTH.DLL")
                                {
                                    entry.HasError = false;
                                    entry.Comment = "CoolSoft VirtualMIDISynth";
                                }

                                if (entry.Value.ToUpper() == "MIDIMAPPER.DLL")
                                {
                                    entry.HasError = false;
                                    entry.Comment = "CoolSoft MIDIMapper";
                                }
                            }
                        }

                        entries.Add(entry);
                    }
                }
            }
        }

        return entries;
    }

    public List<FoundRegistryEntry> GetDrivers32MidiEntries()
    {
        _loggingService.LogInfo($"Enter");

        var values = GetHklmRegistryEntries(Drivers32Path, RegistryView.Registry64).OrderBy(x => x.Name).ToList();

        if (values.Find(x => x.Name.ToLower() == "midisrvtransfercomplete") == null)
        {
            var entry = new FoundRegistryEntry();
            entry.HasError = true;
            entry.Comment = "Missing";
            entry.Name = "MidisrvTransferComplete";

            values.Add(entry);
        }

        if (values.Find(x => x.Name.ToLower() == "midi") == null)
        {
            var entry = new FoundRegistryEntry();
            entry.HasError = true;
            entry.Comment = "Missing";
            entry.Name = "midi";

            values.Add(entry);
        }

        if (values.Find(x => x.Name.ToLower() == "midi1") == null)
        {
            var entry = new FoundRegistryEntry();
            entry.HasError = true;
            entry.Comment = "Missing";
            entry.Name = "mid1";

            values.Add(entry);
        }

        return values;
    }

    public List<FoundRegistryEntry> GetDrivers32WOWMidiEntries()
    {
        _loggingService.LogInfo($"Enter");

        var values = GetHklmRegistryEntries(Drivers32WOWPath, RegistryView.Registry32).OrderBy(x => x.Name).ToList();

        if (values.Find(x => x.Name.ToLower() == "midi") == null)
        {
            var entry = new FoundRegistryEntry();
            entry.HasError = true;
            entry.Comment = "Missing";
            entry.Name = "midi";

            values.Add(entry);
        }

        if (values.Find(x => x.Name.ToLower() == "midi1") == null)
        {
            var entry = new FoundRegistryEntry();
            entry.HasError = true;
            entry.Comment = "Missing";
            entry.Name = "mid1";

            values.Add(entry);
        }

        return values;
    }

}