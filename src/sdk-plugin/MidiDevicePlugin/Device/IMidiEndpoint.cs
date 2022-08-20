// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

using Microsoft.Windows.Midi.Internal.ServiceProtocol.Midi;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Windows.Midi.PluginModel.Device
{
    public interface IMidiEndpoint
    {
        /// <summary>
        /// GUID / Id for this endpoint on a device
        /// </summary>
        Guid Id { get; }




        void Open();

        void Close();

        void SetOutgoingUmpMessageQueue(Guid endpointId, IMidiUmpMessageQueue outgoingMessageQueue);

        void SetIncomingUmpMessageQueue(Guid endpointId, IMidiUmpMessageQueue incomingMessageQueue);


    }
}
