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

        public static void Refresh()
        {
            throw new NotImplementedException();
        }

        public static IEnumerable<MidiDeviceInformation> GetDevices()
        {
            throw new NotImplementedException();
        }


        /// <summary>
        /// List all endpoints
        /// </summary>
        public static IEnumerable<MidiEndpointInformation> GetEndpoints()
        {
            throw new NotImplementedException();
        }


        /// <summary>
        /// List all endpoints for a specific device
        /// </summary>
        /// <param name="deviceId"></param>
        public static IEnumerable<MidiEndpointInformation> GetEndpoints(Guid deviceId)
        {
            throw new NotImplementedException();
        }


    }
}
