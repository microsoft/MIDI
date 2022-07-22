using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace MidiService.Protocol.Messages.Base
{

    [ProtoContract]
    [ProtoInclude(11, typeof(CreateSessionRequestMessage))]
    [ProtoInclude(12, typeof(CreateSessionResponseMessage))]
    public abstract class ProtocolMessage
    {
        //[ProtoMember(1)]
        //public ProtocolStreamOpCode Opcode;
    }
}
