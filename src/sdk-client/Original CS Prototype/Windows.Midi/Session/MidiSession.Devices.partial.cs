// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

using Microsoft.Windows.Midi.Devices;
using Microsoft.Windows.Midi.Enumeration;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Windows.Foundation.Metadata;

namespace Microsoft.Windows.Midi.Session
{
    public partial class MidiSession
    {
        /// <summary>
        /// Opens the endpoint and starts listening for new incoming messages
        /// </summary>
        /// <param name="deviceId">Id of the device which contains the endpoint</param>
        /// <param name="endpointId">Id of the endpoint itself</param>
        /// <returns></returns>
        public MidiEndpoint OpenEndpoint(Guid deviceId, Guid endpointId)
        {
            throw new NotImplementedException();
        }

        /// <summary>
        /// Opens the endpoint and starts listening for new incoming messages
        /// </summary>
        /// <param name="information">The MidiEndpointInformation returned from enumeration</param>
        /// <returns></returns>
        [DefaultOverload]
        public MidiEndpoint OpenEndpoint(MidiEndpointInformation information)
        {
            throw new NotImplementedException();
        }



        ///// <summary>
        ///// Closes the endpoint and releases the queues and event handlers
        ///// associated with it
        ///// </summary>
        ///// <param name="deviceId">Id of the device which contains the endpoint</param>
        ///// <param name="endpointId">Id of the endpoint itself</param>
        //public void CloseEndpoint(Guid deviceId, Guid endpointId)
        //{
        //    throw new NotImplementedException();
        //}

        ///// <summary>
        ///// Closes the endpoint and releases the queues and event handlers
        ///// associated with it
        ///// </summary>
        ///// <param name="information">The MidiEndpointInformation returned from enumeration</param>
        ///// <returns></returns>
        //[DefaultOverload]
        //public void CloseEndpoint(MidiEndpointInformation information)
        //{
        //    throw new NotImplementedException();
        //}






    }
}
