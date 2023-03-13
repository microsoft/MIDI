// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

using Microsoft.Windows.Midi.Internal.Data;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Xml.Linq;

namespace Microsoft.Windows.Midi.Enumeration
{
    public sealed class MidiEndpointInformation
    {
        private MidiEndpointSerializableData _data;

        /// <summary>
        /// Unique Id for this endpoint
        /// </summary>
        public Guid Id { get => _data.Id; }
        
        /// <summary>
        /// The Id of the device which owns this endpoint
        /// </summary>
        public Guid ParentDeviceId { get => _data.ParentDeviceId; }

        /// <summary>
        /// The name supplied by the user
        /// </summary>
        public string Name { get => _data.Name; }

        /// <summary>
        /// The name supplied by the device or driver
        /// </summary>
        public string DeviceSuppliedName { get => _data.DeviceSuppliedName; }
        
        /// <summary>
        /// Icon for this specific endpoint
        /// </summary>
        public string IconFileName { get => _data.IconFileName; }

        /// <summary>
        /// Description of this specific endpoint
        /// </summary>
        public string Description { get => _data.Description; }


    }
}
