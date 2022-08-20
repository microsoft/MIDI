// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

using Microsoft.Windows.Midi.Messages.Packet;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Windows.Foundation.Metadata;

namespace Microsoft.Windows.Midi.Session
{
    public partial class MidiSession
    {



        [DefaultOverload]
        public void SendUmp(Guid deviceId, Guid endpointId, Ump32 message)
        {

        }

        public void SendUmp(Guid deviceId, Guid endpointId, Ump64 message)
        {

        }

        public void SendUmp(Guid deviceId, Guid endpointId, Ump96 message)
        {

        }

        public void SendUmp(Guid deviceId, Guid endpointId, Ump128 message)
        {

        }




        // TODO: method to send a buffer of words




    }
}
