using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace MidiService.Data
{
    public struct MidiDeviceSerializableData
    {
        public Guid Id;
        public string Name;
        public string DeviceSuppliedName;
        public string Serial;
        public string IconFileName;
        public string Description;

        public Guid TransportId;
        public string TransportIconFileName;
        public string TransportType;
        public string TransportDescription;

    }
}
