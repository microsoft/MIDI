// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

using Microsoft.Windows.Midi.Internal.Data;
using Microsoft.Windows.Midi.PluginModel.Plugin;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Windows.Midi.Service.DevicePluginModel.Graph
{
    // This is used in the device graph
    public class MidiDeviceGraphEndpointContainer
    {
        // convenience
        public IMidiDevicePlugin Plugin { get; set; }

        // probably need to hold an instance of the IMidiDevice itself? 


        // The data we need about the device
        public MidiEndpointSerializableData EndpointData { get; set; }


        // Set by the plugin's enumerator
        bool IsPresent { get; set; } = false;
    }
}
