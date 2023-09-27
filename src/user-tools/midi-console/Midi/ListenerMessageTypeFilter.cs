using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Devices.Midi2.ConsoleApp
{
    public enum ListenerMessageTypeFilter
    {
        All = 0,

        SysEx7,
        SysEx8,

        Midi1ChannelVoice,
        Midi2ChannelVoice,

        //FlexDataMessages,

        Stream,
    }

}
