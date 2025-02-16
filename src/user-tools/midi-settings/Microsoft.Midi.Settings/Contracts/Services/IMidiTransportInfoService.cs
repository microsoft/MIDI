using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Midi.Settings.Contracts.Services
{
    interface IMidiTransportInfoService
    {
        MidiServiceTransportPluginInfo GetTransportForCode(string transportCode);




    }
}
