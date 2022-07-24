using Microsoft.Windows.Midi.PluginModel;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace LoopbackPlugin
{
    public class LoopbackMidiDevice : IMidiDevice
    {
        public Guid Id => throw new NotImplementedException();

        public string Name { get => throw new NotImplementedException(); set => throw new NotImplementedException(); }

        public string DeviceSuppliedName => throw new NotImplementedException();

        public string IconFileName => throw new NotImplementedException();

        public string Description { get => throw new NotImplementedException(); set => throw new NotImplementedException(); }

        public Guid TransportId => throw new NotImplementedException();


    }


}
