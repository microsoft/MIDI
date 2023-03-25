// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Windows.Midi
{

    // use the WinRT EventHandler<MidiMessagesReceivedEventArgs> to use this type
    public sealed class MidiMessagesReceivedEventArgs : object
    {
        // Can use the helper classes to parse this
        // TODO: may want to consider span etc. here
        public IList<uint> MessageWords { get; internal set; }

    }
}
