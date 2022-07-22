using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Windows.Midi.Devices
{
    public sealed class MidiDevice
    {
        private MidiDeviceSerializableData _data;

        /// <summary>
        /// GUID for this device
        /// </summary>
        public String Id { get; internal set; }

        /// <summary>
        /// User-supplied name. Defaults to device-supplied name
        /// </summary>
        public String Name { get; set; }

        /// <summary>
        /// Name of the device as supplied by the device itself
        /// </summary>
        public String DeviceSuppliedName { get; internal set; }

        /// <summary>
        /// For devices which support this (like some USB), the unique HW ID
        /// </summary>
        public String Serial { get; internal set; }

        /// <summary>
        /// File name of the icon for this device. Not full path, just file name. Icons are all
        /// stored in the same folder structure
        /// </summary>
        public String IconFileName { get; internal set; }

        /// <summary>
        /// Description of the device. Supplied by user.
        /// </summary>
        public String Description { get; set; }


        /// <summary>
        /// Id of the transport associated with this device. The transport is responsible for 
        /// all of the incoming and outgoing messaging, interfacing with any drivers, etc.
        /// </summary>
        public Guid TransportId { get; internal set; }


    }
}
