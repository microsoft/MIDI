using Microsoft.Midi.Settings.Contracts.Services;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Midi.Settings.Services;

public class MidiDiagnosticsService : IMidiDiagnosticsService
{
    public bool CaptureMidiDiagOutputToNotepad()
    {
        try
        {
            string mididiagTempFileName = Path.GetTempFileName();

            using (var consoleProcess = new System.Diagnostics.Process())
            {
                consoleProcess.StartInfo.FileName = $"mididiag.exe";
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
        catch (Exception ex)
        {
            App.GetService<ILoggingService>().LogError("Error capturing mididiag.exe output and opening in notepad.", ex);

            return false;
        }
    }

}
