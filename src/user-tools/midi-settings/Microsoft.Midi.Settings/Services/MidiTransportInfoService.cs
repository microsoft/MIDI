using Microsoft.Midi.Settings.Contracts.Services;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Midi.Settings.Services
{
    class MidiTransportInfoService : IMidiTransportInfoService
    {
        public MidiServiceTransportPluginInfo GetTransportForCode(string transportCode)
        {
            var codes = MidiReporting.GetInstalledTransportPlugins();

            return codes.Where(p => p.TransportCode.ToUpper() == transportCode.ToUpper()).FirstOrDefault<MidiServiceTransportPluginInfo>();
        }
    }
}
