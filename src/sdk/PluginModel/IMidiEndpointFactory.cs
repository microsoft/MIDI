using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Windows.Midi.PluginModel
{
    public interface IMidiEndpointFactory
    {
        public IMidiEndpoint CreateEndpoint(IMidiDevice parentDevice, IMidiEndpointCreateOptions options);

    }
}
