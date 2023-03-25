// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

using Microsoft.Extensions.Logging;
using Microsoft.Windows.Midi.Internal.Data;
using Microsoft.Windows.Midi.Internal.Service.DeviceGraph;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Windows.Midi.PluginModel.Device
{
    // This enumerates all devices which are recognized by this enumerator
    // Examples:
    // - The USB MIDI device enumerator will list all USB MIDI
    //   devices on the system.
    // - The Virtual MIDI Device enumerator will list all virtual MIDI
    //   devices created by the user through the setup file, or created
    //   by apps at runtime.
    // These are not necessarily open devices. Just all devices connected
    // to the PC at that time.
    public interface IMidiDeviceEnumerator
    {
        void Initialize(MidiDeviceGraph graph, ILogger logger);

        void PopulateDevices();

        void PopulateEndpoints();

        // TODO. These need change notifications and whatnot so we can update the device graph.


    }
}
