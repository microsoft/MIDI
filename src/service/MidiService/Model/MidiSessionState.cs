// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Windows.Midi.Internal.Service.Model
{
    internal struct MidiSessionState
    {
        internal Guid HeaderClientId;       // the id used when communicating on shared channels

        internal Guid Id;                   // server-generated GUID for the session
        //internal MidiServiceAddress Address;
        internal Version ClientVersion;     // Client API version
        //internal int LogLevel;              // TODO: requested log level for this session

        internal string SessionChannelName;     // name of the IPC (pipes, etc.) channel for this session
        internal DateTime CreatedTime;      // server-provided date/time for when session was created

        internal long ProcessId;            // Id of the client process that requested the session
        internal string ProcessName;        // Name of the client process that requested the session

        internal string Name;               // custom name provided by the client process (tab, project, etc.)


        // TODO: link to message queue for messages?
        // TODO: Link to open endpoint information?

    }
}
