// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

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

        // TODO: added/removed/changed events for each object type

        /// <summary>
        /// Refresh the local copy of the enumeration graph
        /// </summary>
        public static void Refresh()
        {
            throw new NotImplementedException();
        }

        /// <summary>
        /// List all available transport providers (BLE, USB, etc.)
        /// </summary>
        public static IList<MidiTransportInformation> GetTransports()
        {
            throw new NotImplementedException();
        }

        /// <summary>
        /// List all the available device endpoints
        /// </summary>
        public static IList<MidiDeviceInformation> GetDevices()
        {
            throw new NotImplementedException();
        }


        /// <summary>
        /// List all endpoints
        /// </summary>
        public static IList<MidiEndpointInformation> GetEndpoints()
        {
            throw new NotImplementedException();
        }


        /// <summary>
        /// List all endpoints for a specific device
        /// </summary>
        /// <param name="deviceId">Id of the parent device</param>
        public static IList<MidiEndpointInformation> GetEndpoints(Guid deviceId)
        {
            throw new NotImplementedException();
        }


    }
}
