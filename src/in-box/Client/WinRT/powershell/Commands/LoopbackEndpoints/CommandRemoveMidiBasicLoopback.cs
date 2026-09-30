// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using System.Management.Automation;

using Windows.Devices.Midi2.Transports.BasicLoopback;

namespace WindowsMidiServices
{

    [Cmdlet(VerbsCommon.Remove, "MidiBasicLoopback", SupportsShouldProcess = true)]
    [OutputType(typeof(MidiBasicLoopbackRemovalResponse))]
    public class CommandRemoveMidiBasicLoopback : MidiCmdletBase
    {
        [Parameter(Mandatory = true, Position = 0, ValueFromPipelineByPropertyName = true)]
        public Guid AssociationId { get; set; }

        [Parameter]
        public SwitchParameter PassThru { get; set; }

        protected override void ProcessRecord()
        {
            RequireMidiServices();
            RequireTransport(MidiBasicLoopbackManager.IsTransportAvailable, Strings.TransportNameBasicLoopback);

            if (!ShouldProcess(AssociationId.ToString(), Strings.LoopbackBasicRemoveAction))
            {
                return;
            }

            var removalConfig = new MidiBasicLoopbackRemovalConfig(AssociationId);

            var response = MidiBasicLoopbackManager.RemoveTransientLoopback(removalConfig);

            if (response is null || !response.Success)
            {
                WriteNonTerminating(
                    new InvalidOperationException(response is null ? Strings.LoopbackBasicRemovalFailed : response.ErrorMessage),
                    "MidiBasicLoopbackRemovalFailed",
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
