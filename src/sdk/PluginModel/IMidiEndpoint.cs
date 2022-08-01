using Microsoft.Windows.Midi.Internal.ServiceProtocol.Midi;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Windows.Midi.PluginModel
{
    public interface IMidiEndpoint
    {
        /// <summary>
        /// GUID / Id for this endpoint on a device
        /// </summary>
        Guid Id { get; }

        /// <summary>
        /// GUID / Id of the parent device which owns the endpoint. The full identifier
        /// for an endpoint is the combination of the endpoint Id and parent device id
        /// </summary>
        Guid ParentDeviceId { get; }

        /// <summary>
        /// User-supplied name. Defaults to the device-supplied name
        /// </summary>
        string Name { get; set; }

        /// <summary>
        /// Name of the endpoint as supplied by the device itself
        /// </summary>
        string DeviceSuppliedName { get; }

        /// <summary>
        /// File name of the icon for this endpoint. Not full path, just file name.
        /// </summary>
        string IconFileName { get; }

        /// <summary>
        /// Description of the device. Supplied by user.
        /// </summary>
        string Description { get; set; }

        void Open();

        void Close();

        void SetOutgoingUmpMessageQueue(Guid endpointId, IMidiUmpMessageQueue outgoingMessageQueue);

        void SetIncomingUmpMessageQueue(Guid endpointId, IMidiUmpMessageQueue incomingMessageQueue);


    }
}
