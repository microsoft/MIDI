// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Windows.Midi.Session
{
    [Flags]
    public enum MidiSessionLogLevel
    {
        None = 0x00000000,                  // log nothing. Default production setting with best perf.
        Errors = 0x00000001,                // log errors
        Infrastructure = 0x00000002,        // log infrastructure stuff like adding/enumerating devices, etc.
        IncomingMessages = 0x00001000,      // log incoming messages for the client app
        OutgoingMessages = 0x00002000,		// log outgoing messages from the client app
    }
}
