using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Midi.Settings.Contracts.Services
{
    public interface IMidiServiceRegistrySettingsService
    {
        bool GetDefaultToOldMidi1PortNaming();
        bool SetDefaultToOldMidi1PortNaming(bool newValue);


        bool GetUseMMCSS();
        bool SetUseMMCSS(bool newValue);


        bool GetMidi2DiscoveryEnabled();
        bool SetMidi2DiscoveryEnabled(bool newValue);


        UInt32 GetMidi2DiscoveryTimeoutMS();
        bool SetMidi2DiscoveryTimeoutMS(UInt32 newValue);

    }
}
