// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

using Microsoft.Windows.Midi.Internal.Data;

using Windows.Foundation;

namespace Microsoft.Windows.Midi.Enumeration
{
    public sealed class MidiDeviceInformation
    {
        private MidiDeviceSerializableData _deviceData;

        /// <summary>
        /// Unique Id for this device
        /// </summary>
        public Guid Id { get => _deviceData.Id; }

        /// <summary>
        /// The transport type used to speak to this device
        /// </summary>
        public Guid DevicePluginId { get => _deviceData.DevicePluginId; }

        /// <summary>
        /// User-supplied name for this specific device
        /// </summary>
        public string Name { get => _deviceData.Name; }

        /// <summary>
        /// Device or driver-supplied name for this device
        /// </summary>
        public string DeviceSuppliedName { get => _deviceData.DeviceSuppliedName; }

        /// <summary>
        /// If the device has a unique serial number, it is here
        /// </summary>
        public string Serial { get => _deviceData.Serial; }

        /// <summary>
        /// File name for the icon for this device
        /// </summary>
        public string IconFileName { get => _deviceData.IconFileName; }

        /// <summary>
        /// Description of the device
        /// </summary>
        public string Description { get => _deviceData.Description; }



    }
}
