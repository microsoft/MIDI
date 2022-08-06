// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

using Microsoft.Windows.Midi.Internal.Data;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Windows.Midi.PluginModel
{
    public interface IMidiEndpointEnumerator
    {
        IEnumerable<MidiEndpointSerializableData> GetEndpoints(Guid deviceId);
        
        IEnumerable<MidiEndpointSerializableData> GetEndpoints();

    }
}
