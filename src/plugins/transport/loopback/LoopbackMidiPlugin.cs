// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

using Microsoft.Windows.Midi.PluginModel.Device;
using Microsoft.Windows.Midi.PluginModel.Plugin;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace MidiLoopbackPlugin
{
    public class LoopbackMidiPlugin : IMidiDevicePlugin
    {
        public MidiPluginMetadata PluginMetadata { get; private set; }

        public LoopbackMidiPlugin()
        {
            PluginMetadata = new MidiPluginMetadata()
            {
                PluginId = Constants.PluginId,
                Author = "Microsoft",
                IconFileName = "",
                Description = "A device for performance and general testing. Connects its MIDI Out right back to MIDI In",
                Version = Version.Parse("0.1.0")
            };
        }

        public IMidiDeviceEnumerator GetDeviceEnumerator()
        {
            return new LoopbackMidiEnumerator();
        }

        public IMidiDeviceFactory GetDeviceFactory()
        {
            throw new NotImplementedException();
        }
    }
}
