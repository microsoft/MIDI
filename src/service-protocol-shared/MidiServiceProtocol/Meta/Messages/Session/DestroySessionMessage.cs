// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

using Microsoft.Windows.Midi.Internal.ServiceProtocol.Messages.Base;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.Serialization;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Windows.Midi.Internal.ServiceProtocol.Messages.Session
{
    public class SessionDestroyRequestMessage : ProtocolMessage
    {
        public Guid HeaderClientId;

        public Guid SessionGuid;                     // server-generated GUID for the session
        public Version ClientVersion;                // Client API version

        //public ProtocolStreamOpCode Opcode => ProtocolStreamOpCode.SessionDestroyRequestMessage;

        public void Deserialize(BinaryReader reader)
        {
            throw new NotImplementedException();
        }

        public void Serialize(BinaryWriter writer)
        {
            throw new NotImplementedException();
        }
        // TODO: Log level


    }

    public enum SessionDestroyResponseCode
    {
        Success = 0,
        ErrorIncompatibleVersion = 9098,
        ErrorOther = 9099
    }

    public class SessionDestroyResponseMessage : ProtocolMessage
    {
        public Guid HeaderClientId;

        public SessionDestroyResponseCode ResponseCode;    // See above
        public Version ServerVersion;                  // version running on the server.

        //public ProtocolStreamOpCode Opcode => ProtocolStreamOpCode.SessionDestroyResponseMessage;

        public void Deserialize(BinaryReader reader)
        {
            throw new NotImplementedException();
        }

        public void Serialize(BinaryWriter writer)
        {
            throw new NotImplementedException();
        }
    }
}
