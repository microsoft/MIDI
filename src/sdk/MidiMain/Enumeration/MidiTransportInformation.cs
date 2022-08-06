// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

using Microsoft.Windows.Midi.Internal.Data;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Windows.Midi.Enumeration
{
    public sealed class MidiTransportInformation
    {
        private MidiTransportSerializableData _transportData;

        /// <summary>
        /// Unique Id for the transport type
        /// </summary>
        public Guid Id { get => _transportData.Id; }

        /// <summary>
        /// Short name of the transport, like "BLE MIDI 1.0"
        /// </summary>
        public string Name { get => _transportData.Name; }

        /// <summary>
        /// Name of the icon used to represent the transport
        /// </summary>
        public string IconFileName { get => _transportData.IconFileName; }

        /// <summary>
        /// Long name or description of the transport
        /// </summary>
        public string Description { get => _transportData.Description; }

    }
}
