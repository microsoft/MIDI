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
            RequireTransport(MidiLoopbackManager.IsTransportAvailable, "MIDI 2.0 loopback");

            if (!ShouldProcess(AssociationId.ToString(), "Remove MIDI 2.0 loopback endpoint pair"))
            {
                return;
            }

            var removalConfig = new MidiLoopbackRemovalConfig(AssociationId);

            var response = MidiLoopbackManager.RemoveTransientLoopback(removalConfig);

            if (response is null || !response.Success)
            {
                WriteNonTerminating(
                    new InvalidOperationException(response is null ? "Unable to remove the loopback." : response.ErrorMessage),
                    "MidiLoopbackRemovalFailed",
                    ErrorCategory.InvalidOperation,
                    AssociationId);

                return;
            }

            WriteVerbose("The loopback was removed from the running service. An entry saved in the configuration file is not affected.");

            if (PassThru.IsPresent)
            {
                WriteObject(response);
            }
        }
    }


}
