// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

using Microsoft.Windows.Midi.Internal.ServiceProtocol.Midi;
using Microsoft.Windows.Midi.PluginModel;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Windows.Midi.Internal.Service.Model
{

    // TODO: Need to have a version of this for MIDI 1.0 as well

    public class UmpEndpoint
    {
        public Guid Id { get; set; }

        /// <summary>
        /// Messages going out to the device
        /// </summary>
        public IMidiUmpMessageQueue OutgoingMessages { get; set; }
        
        
        /// <summary>
        /// Messages coming in from the device
        /// </summary>
        public IMidiUmpMessageQueue IncomingMessages { get; set; }



    }
}
