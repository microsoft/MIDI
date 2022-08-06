// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

using Microsoft.Windows.Midi.Internal.Data;

using Windows.Foundation;

namespace Microsoft.Windows.Midi.Enumeration
{
    public sealed class MidiDeviceInformation
    {
        private MidiDeviceSerializableData _deviceData;

        public Guid Id { get => _deviceData.Id; }
        public Guid TransportId { get => _deviceData.TransportId; }

        public string Name { get => _deviceData.Name; }
        public string DeviceSuppliedName { get => _deviceData.DeviceSuppliedName; }
        public string Serial { get => _deviceData.Serial; }
        public string IconFileName { get => _deviceData.IconFileName; }
        public string Description { get => _deviceData.Description; }



    }
}
