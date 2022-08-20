// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Windows.Midi.Internal.Data
{
    public struct MidiTransportSerializableData
    {
        public Guid Id;
        public string Name;
        public string IconFileName;
        public string Description;
    }
}
