// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


using Windows.Devices.Midi2.Transports.Loopback;
using System.Management.Automation;

namespace WindowsMidiServices
{

    [Cmdlet(VerbsCommon.Remove, "MidiLoopback", SupportsShouldProcess = true)]
    [OutputType(typeof(MidiLoopbackRemovalResponse))]
    public class CommandRemoveMidiLoopback : MidiCmdletBase
    {
        [Parameter(Mandatory = true, Position = 0, ValueFromPipelineByPropertyName = true)]
        public Guid AssociationId
        {
            get; set;
        }

        [Parameter]
        public SwitchParameter PassThru { get; set; }

        protected override void ProcessRecord()
        {
            RequireMidiServices();
            RequireTransport(MidiLoopbackManager.IsTransportAvailable, Strings.TransportNameLoopback);

            if (!ShouldProcess(AssociationId.ToString(), Strings.LoopbackRemoveAction))
            {
                return;
            }

            var removalConfig = new MidiLoopbackRemovalConfig(AssociationId);

            var response = MidiLoopbackManager.RemoveTransientLoopback(removalConfig);

            if (response is null || !response.Success)
            {
                WriteNonTerminating(
                    new InvalidOperationException(response is null ? Strings.LoopbackRemovalFailed : response.ErrorMessage),
                    "MidiLoopbackRemovalFailed",
                    ErrorCategory.InvalidOperation,
                    AssociationId);

                return;
            }

            WriteVerbose(Strings.LoopbackRemoved);

            if (PassThru.IsPresent)
            {
                WriteObject(response);
            }
        }
    }


}
