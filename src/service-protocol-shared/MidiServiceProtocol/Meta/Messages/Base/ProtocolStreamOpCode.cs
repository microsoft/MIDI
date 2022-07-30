using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Windows.Midi.Internal.ServiceProtocol.Messages.Base
{
    public enum ProtocolStreamOpCode : int
    {
        SessionCreateRequestMessage = 0x01,
        SessionCreateResponseMessage = 0x02,

        SessionDestroyRequestMessage = 0x03,
        SessionDestroyResponseMessage = 0x04,
    }
}
