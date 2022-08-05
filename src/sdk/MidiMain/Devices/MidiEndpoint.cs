// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

using Microsoft.Windows.Midi.Enumeration;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Windows.Midi.Devices
{
    public sealed class MidiEndpoint
    {
        public MidiEndpointInformation EndpointInformation { get; internal set; }


        public void Close()
        {
        }

    }
}
