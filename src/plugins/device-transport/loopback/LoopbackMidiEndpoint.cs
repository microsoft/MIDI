using Microsoft.Windows.Midi.Internal.ServiceProtocol.Midi;
using Microsoft.Windows.Midi.PluginModel;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace LoopbackPlugin
{
    public class LoopbackMidiEndpoint : IMidiEndpoint
    {
        public Guid Id => throw new NotImplementedException();

        public Guid ParentDeviceId => throw new NotImplementedException();

        public string Name { get => throw new NotImplementedException(); set => throw new NotImplementedException(); }

        public string DeviceSuppliedName => throw new NotImplementedException();

        public string IconFileName => throw new NotImplementedException();

        public string Description { get => throw new NotImplementedException(); set => throw new NotImplementedException(); }

        public void Close()
        {
            throw new NotImplementedException();
        }

        public void Open()
        {
            throw new NotImplementedException();
        }

        public void SetIncomingUmpMessageQueue(Guid endpointId, IMidiUmpMessageQueue incomingMessageQueue)
        {
            throw new NotImplementedException();
        }

        public void SetOutgoingUmpMessageQueue(Guid endpointId, IMidiUmpMessageQueue outgoingMessageQueue)
        {
            throw new NotImplementedException();
        }
    }


}
