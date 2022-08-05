// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Windows.Midi.Internal.ServiceProtocol.Serialization
{
    public class MidiMessageSerializer
    {
        // serializes to/from a stream
        // puts a header with from/to info in front
        // then writes UMP. UMP itself (through type) 
        // identifies how many more UInt32 values to read.


        // need to make sure these can work with a variety of message counts, spans, etc.


        //       
        //public void SerializeOutgoingToEndpoint(Stream stream, Ump message, Guid destinationEndpointId)
        //{

        //}

        //// reading from a queue of messages coming in from an endpoint
        //public Ump DeserializeIncomingFromEndpoint(Stream stream)
        //{

        //}

        //// this is for something routed from an endpoint to an endpoint. Different queue.
        //public void SerializeOutgoingRoutedMessageFromEndpoint(Stream stream, Ump message, Guid sourceEndpointId, Guid destinationEndpointId)
        //{

        //}

    }
}
