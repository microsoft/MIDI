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

        public Guid Id { get => _transportData.Id; }

        public string Name { get => _transportData.Name; }
        public string IconFileName { get => _transportData.IconFileName; }
        public string Description { get => _transportData.Description; }

    }
}
