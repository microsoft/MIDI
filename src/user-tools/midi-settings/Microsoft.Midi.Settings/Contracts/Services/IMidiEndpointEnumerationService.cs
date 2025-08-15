using Microsoft.Midi.Settings.ViewModels;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Midi.Settings.Contracts.Services
{
    public interface IMidiEndpointEnumerationService
    {
        void Start();

        IList<MidiEndpointWrapper> GetEndpoints();
        IList<MidiEndpointWrapper> GetEndpointsForTransportCode(string transportCode);
        IList<MidiEndpointWrapper> GetEndpointsForTransportId(Guid transportId);
        IList<MidiEndpointWrapper> GetEndpointsForPurpose(MidiEndpointDevicePurpose purpose);

    }
}
