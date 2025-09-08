// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


using System;
using System.Collections.Generic;
using System.Linq;
using System.Management.Automation;
using System.Text;
using System.Threading.Tasks;

namespace WindowsMidiServices
{

    [Cmdlet(VerbsCommon.Close, "MidiEndpointConnection")]
    public class CommandCloseMidiEndpointConnection : Cmdlet
    {
        [Parameter(Mandatory = true, Position = 0)]
        public MidiSession? Session
        {
            get; set;
        }

        [Parameter(Mandatory = true, Position = 1)]
        public MidiEndpointConnection? Connection
        {
            get; set;
        }

        protected override void ProcessRecord()
        {
            if (Session == null)
            {
                throw new ArgumentNullException("Session is null.");
            }

            if (Session.BackingSession == null)
            {
                throw new ArgumentNullException("Underlying session is null.");
            }

            if (Connection == null)
            {
                throw new ArgumentNullException("Endpoint connection is null.");
            }

            Guid id = Connection.ConnectionId;

            Session.BackingSession.DisconnectEndpointConnection(id);

            WriteVerbose($"MIDI Endpoint Connection {id} closed.");
        }
    }


}
