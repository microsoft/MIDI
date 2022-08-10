// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Windows.Midi.PluginModel.Device
{
    // The plugin implements this only if you can create new MIDI devices
    // through its API. if it supports creating Devices, it must also 
    // support creating Endpoints

    public interface IMidiDeviceFactory
    {

        // public IMidiDevice CreateDevice(IMidiDeviceCreateOptions options);

    }
}
