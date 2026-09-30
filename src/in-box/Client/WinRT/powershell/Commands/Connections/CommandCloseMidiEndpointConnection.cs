// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


using System.Management.Automation;

namespace WindowsMidiServices
{

    [Cmdlet(VerbsCommon.Close, "MidiEndpointConnection", SupportsShouldProcess = true)]
    public class CommandCloseMidiEndpointConnection : MidiCmdletBase
    {
        [Parameter(Mandatory = true, Position = 0)]
        public MidiSession? Session
        {
            get; set;
        }

        [Parameter(Mandatory = true, Position = 1, ValueFromPipeline = true)]
        public MidiEndpointConnection? Connection
        {
            get; set;
        }

        protected override void ProcessRecord()
        {
            if (Session?.BackingSession is null)
            {
                ThrowTerminating(
                    new ArgumentException(Strings.SessionRequired, nameof(Session)),
                    "MidiSessionRequired",
                    ErrorCategory.InvalidArgument);

                return;
            }

            if (Connection?.BackingConnection is null)
            {
                ThrowTerminating(
                    new ArgumentNullException(nameof(Connection)),
                    "MidiConnectionRequired",
                    ErrorCategory.InvalidArgument);

                return;
            }

            var id = Connection.ConnectionId;

            if (!ShouldProcess(Connection.EndpointDeviceId, Strings.ConnectionCloseAction))
            {
                return;
            }

            Session.BackingSession.DisconnectEndpointConnection(id);

            WriteVerbose(Format(Strings.ConnectionClosedFormat, id));
        }
    }


}
