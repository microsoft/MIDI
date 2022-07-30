using Microsoft.Windows.Midi.Internal.ServiceProtocol.Messages.Management;
using Microsoft.Windows.Midi.Internal.ServiceProtocol.Messages.Session;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Windows.Midi.Internal.ServiceProtocol.Messages.Base
{

    [ProtoContract]
    [ProtoInclude(11, typeof(CreateSessionRequestMessage))]
    [ProtoInclude(12, typeof(CreateSessionResponseMessage))]

    [ProtoInclude(13, typeof(ServicePingMessage))]
    [ProtoInclude(14, typeof(ServicePingResponseMessage))]


    public abstract class ProtocolMessage
    {
        //[ProtoMember(1)]
        //public ProtocolStreamOpCode Opcode;
    }
}
