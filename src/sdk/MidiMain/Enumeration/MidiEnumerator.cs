using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

using Microsoft.Windows.Midi.Devices;

namespace Microsoft.Windows.Midi.Enumeration
{
    public sealed class MidiEnumerator
    {

        public static void Refresh()
        {
            throw new NotImplementedException();
        }

        public static IEnumerable<MidiDevice> GetDevices()
        {
            throw new NotImplementedException();
        }


        /// <summary>
        /// List all endpoints
        /// </summary>
        public static IEnumerable<MidiEndpoint> GetEndpoints()
        {
            throw new NotImplementedException();
        }


        /// <summary>
        /// List all endpoints for a specific device
        /// </summary>
        /// <param name="deviceId"></param>
        public static IEnumerable<MidiEndpoint> GetEndpoints(Guid deviceId)
        {
            throw new NotImplementedException();
        }


    }
}
