using Microsoft.Midi.Settings.Contracts.Services;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Windows.UI.Popups;

namespace Microsoft.Midi.Settings.Services
{
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
                var dialog = new MessageDialog("Error opening console");
                dialog.Content = ex.ToString();

                dialog.ShowAsync().Wait();
            }
        }
    }
}
