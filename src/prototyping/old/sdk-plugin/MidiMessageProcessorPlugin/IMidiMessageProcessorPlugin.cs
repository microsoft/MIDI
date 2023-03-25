// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Windows.Midi.PluginModel.Plugin
{
    public interface IMidiMessageProcessorPlugin
    {

        MidiPluginMetadata PluginMetadata { get; }


    }
}
