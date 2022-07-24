using MidiService.Data;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Windows.Midi.PluginModel
{
    public interface IMidiEndpointEnumerator
    {
        IEnumerable<MidiEndpointSerializableData> GetEndpoints(Guid deviceId);
        
        IEnumerable<MidiEndpointSerializableData> GetEndpoints();


    }
}
