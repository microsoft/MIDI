using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace MidiService.Protocol.Messages.Base
{
    public enum ResponseCode
    {
        Success = 0,

        ErrorTooManyActiveConnectionsFromClient = 500,

        ErrorIncompatibleVersion = 9098,
        ErrorOther = 9099
    }
}
