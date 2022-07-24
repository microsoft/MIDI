using MidiService.Data;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Windows.Midi.PluginModel
{
    public interface IMidiDevice
    {
        /// <summary>
        /// GUID for this device
        /// </summary>
        Guid Id { get; }

        /// <summary>
        /// User-supplied name. Defaults to device-supplied name
        /// </summary>
        string Name { get; set; }

        /// <summary>
        /// Name of the device as supplied by the device itself
        /// </summary>
        string DeviceSuppliedName { get; }

        /// <summary>
        /// For devices which support this (like some USB), the unique HW ID
        /// </summary>
//        string SerialNumber { get; internal set; }

        /// <summary>
        /// File name of the icon for this device. Not full path, just file name. Icons are all
        /// stored in the same folder structure
        /// </summary>
        string IconFileName { get; }

        /// <summary>
        /// Description of the device. Supplied by user.
        /// </summary>
        string Description { get; set; }


        /// <summary>
        /// Id of the transport associated with this device. The transport is responsible for 
        /// all of the incoming and outgoing messaging, interfacing with any drivers, etc.
        /// </summary>
        Guid TransportId { get; }
    }
}
