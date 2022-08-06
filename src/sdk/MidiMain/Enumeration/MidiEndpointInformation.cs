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

        public Guid Id { get => _data.Id; }
        public Guid ParentDeviceId { get => _data.ParentDeviceId; }

        public string Name { get => _data.Name; }
        public string DeviceSuppliedName { get => _data.DeviceSuppliedName; }
        public string IconFileName { get => _data.IconFileName; }
        public string Description { get => _data.Description; }


    }
}
