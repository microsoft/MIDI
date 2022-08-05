// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

using Microsoft.Windows.Midi.Internal.ServiceProtocol.Messages.Base;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Windows.Midi.Internal.ServiceProtocol.Messages.Management
{
    [ProtoContract]
    public class ServicePingResponseMessage : ProtocolMessage
    {
        [ProtoMember(100)]
        public ResponseMessageHeader Header;

    }
}
