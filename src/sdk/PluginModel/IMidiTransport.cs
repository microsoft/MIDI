using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Windows.Midi.PluginModel
{
    public interface IMidiTransport
    {
     
        /// <summary>
        /// Unique identifier for this transport type
        /// </summary>
        Guid Id { get; set; } // the service will generate this



        /// <summary>
        /// This is a short identifier for the transport type, like USB, BLE, RTP, Virtual, etc.)
        /// </summary>
        string ShortName { get; }

        /// <summary>
        /// The longer name for the transport. Example: Bluetooth Low Energy MIDI 1.0
        /// </summary>
        string VerboseName { get; }

        /// <summary>
        /// File name of the image associated with this transport. Does not include the path.
        /// </summary>
        string IconFileName { get; }

        /// <summary>
        /// A long description for this transport type. 
        /// </summary>
        string Description { get; }

        void WriteDataFromStream(Stream sourceStream, Guid endpointId);


        // Devices will aggregate an instance of this transport, and use the functions in it
        // to create endpoints, enumerate endpoints, etc.
        // Transports are 1:1 with the TYPE of transport
        // Devices are 1:1 with the piece of hardware (or virtual) which uses the transport
        // So many devices may point to a single transport


    }
}
