// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using System.Management.Automation;

using Windows.Devices.Midi2.Transports.Loopback;

namespace WindowsMidiServices
{
    // "Mute" is not an approved PowerShell verb, so the state is set rather than toggled. Pass
    // -Muted $true to mute and -Muted $false to unmute.
    [Cmdlet(VerbsCommon.Set, "MidiLoopbackMute", SupportsShouldProcess = true)]
    [OutputType(typeof(MidiLoopbackEntry))]
    public class CommandSetMidiLoopbackMute : MidiCmdletBase
    {
        [Parameter(Mandatory = true, Position = 0, ValueFromPipelineByPropertyName = true)]
        public Guid AssociationId { get; set; }

        [Parameter(Mandatory = true, Position = 1)]
        public bool Muted { get; set; }

        [Parameter]
        public SwitchParameter PassThru { get; set; }

        protected override void ProcessRecord()
        {
            RequireMidiServices();
            RequireTransport(MidiLoopbackManager.IsTransportAvailable, "MIDI 2.0 loopback");

            var action = Muted ? "Mute MIDI 2.0 loopback" : "Unmute MIDI 2.0 loopback";

            if (!ShouldProcess(AssociationId.ToString(), action))
            {
                return;
            }

            var response = Muted
                ? MidiLoopbackManager.MuteLoopback(AssociationId)
                : MidiLoopbackManager.UnmuteLoopback(AssociationId);

            if (response is null || !response.Success)
            {
                WriteNonTerminating(
                    new InvalidOperationException(response is null ? "Unable to change the mute state." : response.ErrorMessage),
                    "MidiLoopbackMuteFailed",
                    ErrorCategory.InvalidOperation,
                    AssociationId);

                return;
            }

            if (!PassThru.IsPresent)
            {
                return;
            }

            var entries = MidiLoopbackManager.GetActiveLoopbackEntries();

            if (entries is null)
            {
                return;
            }

            foreach (var entry in entries)
            {
                if (entry.AssociationId == AssociationId)
                {
                    WriteObject(entry);
                }
            }
        }
    }

}
