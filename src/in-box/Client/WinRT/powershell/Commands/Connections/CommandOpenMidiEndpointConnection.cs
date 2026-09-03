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


    [Cmdlet(VerbsCommon.Open, "MidiEndpointConnection")]
    [OutputType(typeof(MidiEndpointConnection))]
    public class CommandOpenMidiEndpointConnection : MidiCmdletBase
    {
        [Parameter(Mandatory = true, Position = 0, ValueFromPipeline = true)]
        public MidiSession? Session
        {
            get;set;
        }

        [Parameter(Mandatory = true, Position = 1, ValueFromPipelineByPropertyName = true)]
        [ValidateNotNullOrWhiteSpace]
        public string EndpointDeviceId
        {
            get; set;
        } = string.Empty;


        protected override void ProcessRecord()
        {
            if (Session is null || !Session.IsValid)
            {
                ThrowTerminating(
                    new ArgumentException("An open MIDI session is required. Use Start-MidiSession first.", nameof(Session)),
                    "MidiSessionRequired",
                    ErrorCategory.InvalidArgument);

                return;
            }

            var backingConnection = Session.BackingSession!.CreateEndpointConnection(EndpointDeviceId);

            if (backingConnection is null)
            {
                ThrowTerminating(
                    new InvalidOperationException($"Unable to create a connection to \"{EndpointDeviceId}\"."),
                    "MidiConnectionCreationFailed",
                    ErrorCategory.ResourceUnavailable,
                    EndpointDeviceId);

                return;
            }

            // Wrapped before opening so the message handler is attached before anything arrives.
            var connection = new MidiEndpointConnection(backingConnection);

            if (!backingConnection.Open())
            {
                ThrowTerminating(
                    new InvalidOperationException($"Unable to open the connection to \"{EndpointDeviceId}\"."),
                    "MidiConnectionOpenFailed",
                    ErrorCategory.OpenError,
                    EndpointDeviceId);

                return;
            }

            WriteObject(connection);
        }


    }



}
