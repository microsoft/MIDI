// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

using Microsoft.Windows.Midi.Internal.ServiceProtocol.Messages.Base;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Xml.Linq;


namespace Microsoft.Windows.Midi.Internal.ServiceProtocol.Messages.Session
{
    [ProtoContract]
    public class CreateSessionRequestMessage : ProtocolMessage
    {
        [ProtoMember(100)]
        public RequestMessageHeader Header;

        [ProtoMember(101)]
        public int ProcessId;

        [ProtoMember(102)]
        public string ProcessName;

        [ProtoMember(103)]
        public string Name;

        // TODO: Log level
    }




}
