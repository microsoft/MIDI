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
    public class MidiDeviceGraphDeviceContainer
    {
        // convenience
        public IMidiDevicePlugin Plugin { get; set; }

        // Device will be null until we have an open instance of that device
 //       public IMidiDevice Device { get; set; }

        // The data we need about the device
        public MidiDeviceSerializableData DeviceData { get; set; }


        // Set by the plugin's enumerator
        public bool IsPresent { get; set; } = false;
    }
}
