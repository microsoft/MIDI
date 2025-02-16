using Microsoft.Windows.Devices.Midi2.Endpoints.Loopback;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Midi.Settings.Contracts.Services
{
    public interface IMidiDefaultsService
    {
        string GetDefaultMidiConfigName();
        string GetDefaultMidiConfigFileName();

        MidiLoopbackEndpointCreationConfig GetDefaultLoopbackCreationConfig();
        bool DoesDefaultLoopbackAlreadyExist();
    }
}
