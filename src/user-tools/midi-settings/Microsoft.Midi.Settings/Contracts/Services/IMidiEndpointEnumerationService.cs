using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Midi.Settings.Contracts.Services
{
    public interface IMidiEndpointEnumerationService
    {
        MidiEndpointDeviceWatcher MidiEndpointDeviceWatcher { get; }

    }
}
