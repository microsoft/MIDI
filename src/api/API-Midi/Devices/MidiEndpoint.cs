using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Windows.Midi.Devices
{
    public sealed class MidiEndpoint
    {
        /// <summary>
        /// GUID / Id for this endpoint on a device
        /// </summary>
        public Guid Id { get; internal set; }
        
        /// <summary>
        /// GUID / Id of the parent device which owns the endpoint. The full identifier
        /// for an endpoint is the combination of the endpoint Id and parent device id
        /// </summary>
        public Guid ParentDeviceId { get; internal set; }
        
        /// <summary>
        /// User-supplied name. Defaults to the device-supplied name
        /// </summary>
        public string Name { get; set; }

        /// <summary>
        /// Name of the endpoint as supplied by the device itself
        /// </summary>
        public string DeviceSuppliedName { get; internal set; }

        /// <summary>
        /// File name of the icon for this endpoint. Not full path, just file name.
        /// </summary>
        public string IconFileName { get; internal set; }
        
        /// <summary>
        /// Description of the device. Supplied by user.
        /// </summary>
        public string Description { get; set; }


        // TODO: One-way and bidirectional endpoints

    }
}
