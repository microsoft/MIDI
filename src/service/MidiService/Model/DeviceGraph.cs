// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

using Microsoft.Windows.Midi.PluginModel;
using Microsoft.Windows.Midi.Internal.Data;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Windows.Midi.Internal.Service.Model
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
        private readonly ILogger<DeviceGraph> _logger;

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


        public DeviceGraph(ILogger<DeviceGraph> logger)
        {
            _logger = logger;
        }


        private void PublishGraph()
        {
            // Put out on shared memory
            
            // Devices and endpoints

            // send an InitialEnumerationComplete message

        }


        public void BuildDeviceGraph()
        {
            // build out the whole graph
            EnumerateAllTheThings();

            // publish the graph for clients to use
            PublishGraph();
        }


        private void EnumerateAllTheThings()
        {
            // Process:
            // 1. Spin up a new thread (or do this from the caller)
            // 2. Lock the current device graph
            // 3. Clear the device graph
            // 4. Populate device graph from the setup file
            // 5. Mark each device as not present, to start
            // 6. Call each enumeration plugin to update the graph with their own info
            //  a. Call device enumerator
            //  b. Call endpoint enumerator for each device
            //      (the endpoint enumerator could take a while if it is making MIDI calls)
            // 7. Enumeration plugin updates "Present" flag for devices it finds
            // 8. Unlock graph.
            // 9. Publish on shared memory
            // 10. Notify that enumeration complete


            // TODO: Sort out where we hook and respond to incoming MIDI CI. Presumably the message
            // router will check and then make a call into this class if needed

        }


    }
}
