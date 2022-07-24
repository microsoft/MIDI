using Microsoft.Windows.Midi.PluginModel;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace MidiService.Model
{
    // May want to keep a device and endpoint table in shared memory
    // using memory-mapped file, and keep it updated in one single place
    // and just point clients to it when anything changes
    //
    // Built during initial enumeration by combining info from system
    // with the currently loaded devices setup/config. Should be read-only
    // to any processes other than the server
    //
    // Updated whenever devices or endpoints change
    //
    public class DeviceGraph
    {
        // All loaded device transports
        private Dictionary<Guid, IMidiTransport> _transports =
            new Dictionary<Guid, IMidiTransport>();


        // All the devices
        // Key is device ID
        private Dictionary<Guid, MidiDeviceSerializableData> _devices = 
            new Dictionary<Guid, MidiDeviceSerializableData>();
        
        // All the endpoints
        // key is Tuple<DeviceID, EndpointID>
        // may want to simplify to just EndpointID
        private Dictionary<Tuple<Guid, Guid>, MidiEndpointSerializableData> _endpoints = 
            new Dictionary<Tuple<Guid, Guid>, MidiEndpointSerializableData>();


        private void PublishGraph()
        {
            // Put out on shared memory
            
            // Devices and endpoints



            // send an InitialEnumerationComplete message

        }


        public void BuildDeviceGraph()
        {
            // build out the whole graph
            

            // publish the graph for clients to use
            PublishGraph();
        }


    }
}
