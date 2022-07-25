using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace MidiService.Protocol.Messages.Management
{
    [ProtoContract]
    public class ServicePingMessage : ProtocolMessage
    {
        [ProtoMember(100)]
        public RequestMessageHeader Header;


    }
}
