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


    [Cmdlet(VerbsCommon.Open, "MidiEndpointConnection")]
    public class CommandOpenMidiEndpointConnection : Cmdlet
    {
        [Parameter(Mandatory = true, Position = 0)]
        public MidiSession? Session
        {
            get;set;
        }

        [Parameter(Mandatory = true, Position = 1)]
        public string? EndpointDeviceId
        {
            get; set;
        }


        protected override void ProcessRecord()
        {
            if (Session == null || !Session.IsValid)
            {
                throw new ArgumentNullException("Invalid session.");
            }

            if (string.IsNullOrWhiteSpace(EndpointDeviceId))
            {
                throw new ArgumentNullException("Endpoint device id is null or empty.");
            }

            var backingConnection = Session.BackingSession!.CreateEndpointConnection(EndpointDeviceId);

            if (backingConnection == null)
            {
                throw new Exception("Unable to create connection.");
            }

            // do this here to make sure the event handler is wired up before we open
            var conn = new MidiEndpointConnection(backingConnection);

            if (backingConnection.Open())
            {
                WriteObject(conn);
            }
            else
            {
                throw new Exception("Unable to open connection.");
            }



        }


    }



}
