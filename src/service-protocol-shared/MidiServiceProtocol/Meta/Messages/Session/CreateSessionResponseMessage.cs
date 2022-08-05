// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

using Microsoft.Windows.Midi.Internal.ServiceProtocol.Messages.Base;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Windows.Midi.Internal.ServiceProtocol.Messages.Session
{
    [ProtoContract]
    public class CreateSessionResponseMessage : ProtocolMessage
    {
        [ProtoMember(100)]
        public ResponseMessageHeader Header;

        [ProtoMember(101)]
        public Guid NewSessionId;                      // server-generated GUID for the session

        [ProtoMember(102)]
        public string SessionChannelName;              // name of the IPC (pipes, etc.) channel for this session

        [ProtoMember(103)]
        public DateTime CreatedTime;                   // server-provided date/time for when session was created

        //public MidiServiceProtocolStreamOpCode Opcode => MidiServiceProtocolStreamOpCode.SessionCreateResponseMessage;
    }
}
