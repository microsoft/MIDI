using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Windows.Midi.Internal.Data.MidiCI
{
    // The OS isn't going to have to manage / track much MIDI CI data, but the device info is
    // something we need to provide. This is per group/channel
    public struct MidiCIDeviceInfoSerializableData
    {
        public byte[] ManufacturerId { get; set; }
        public byte[] FamilyId { get; set; }

        public byte[] ModelId { get; set; }
        public byte[] VersionId { get; set; }
        public string Manufacturer { get; set; }
        public string Family { get; set; }
        public string Version { get; set; }
        public string SerialNumber { get; set; }

    }
}
