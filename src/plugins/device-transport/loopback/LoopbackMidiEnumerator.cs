using Microsoft.Extensions.Logging;
using Microsoft.Windows.Midi.Internal.Data;
using Microsoft.Windows.Midi.Internal.Service.DeviceGraph;
using Microsoft.Windows.Midi.PluginModel.Device;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace MidiLoopbackPlugin
{
    public class LoopbackMidiEnumerator : IMidiDeviceEnumerator
    {
        private ILogger _logger;
        private MidiDeviceGraph _deviceGraph;

        public void Initialize(MidiDeviceGraph graph, ILogger logger)
        {
            _deviceGraph = graph;
            _logger = logger;
        }


        //  b. Call endpoint enumerator for each device
        //      (the endpoint enumerator could take a while if it is making MIDI calls)
        // 7. Enumeration plugin updates "Present" flag for devices it finds

        public void PopulateDevices()
        {
            // Get all devices

            // Check the graph for all devices with this DevicePluginId
            if (_deviceGraph != null)
            {
                var devices = _deviceGraph.Devices.Values.Where(x => x.DeviceData.DevicePluginId == Constants.PluginId);

                if (devices != null && devices.Count() > 0)
                {
                    // update devices to set the "present" flag
                    
                    foreach (var device in devices)
                    {
                        device.IsPresent = true;
                    }

                }
                else
                {
                    // no devices in the graph so just add the ones we have

                    // for this plugin, the devices can only be added by the user
                    // so we'll be in sync with the device graph as it was read
                    // from the setup file. IOW, no need to add devices here.
                }
            }
            else
            {
                _logger.LogError("LoopbackMidiEnumerator: PopulateDevices - deviceGraph is null");
            }


        }


        private void AddDeviceDataToGraph(MidiDeviceSerializableData deviceData)
        {

        }


        public void PopulateEndpoints()
        {
            throw new NotImplementedException();
        }
    }
}
